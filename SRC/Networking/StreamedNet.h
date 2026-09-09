#ifndef NCORE_STREAMED_NET_H
#define NCORE_STREAMED_NET_H

#include <memory>
#include <string>
#include <mutex>
#include <queue>

#include "AsioInclude.h"

namespace SN {
   enum class NetworkMode {
      TCP,
      UDP
   };

   enum class SNI {
      Offline = 0,
      Online = 1,
      Resolveing = 2,
      Connecting = 4,
      InRead = 8,
      StopReadR = 16,
      InWrite = 32,
      StopWriteR = 64,
      InAccept = 128,
      StopAcceptR = 256
   };

   enum class Event {
      OnStart,
      Aborted,
      Connected,
      Resolved,
      DataSent,
      DataReceived,
      Disconnected
   };

   enum class Error {
      AlreadyStarted,
      AlreadyResolved,
      AlreadyConnected,
      ConnectFailed,
      ResolveFailed,
      AcceptFailed,
      ConnectionClosed,
      Aborted,
      WriteFailed,
      ReadFailed,
      AbortShutdownFailed,
      AbortCloseFailed,
      AcceptorAbortCancelFailed,
      AcceptorAbortCloseFailed,
      InvalidAddress
   };

   template<NetworkMode Mode>
   class Client;

   template<NetworkMode Mode>
   class Server;

   template<NetworkMode Mode>
   class Connection;

   class Context {
   private:
      using CallbackPtr = std::shared_ptr<std::function<void()>>;

      struct State {
         asio::io_context io;
         std::atomic_uint32_t usageCount{ 0 };
         std::vector<CallbackPtr> callbacks;
         std::mutex callbackMutex;
      };
   public:
      using CallbackHandle = std::weak_ptr<std::function<void()>>;
      Context();

      void use();
      void release();
      size_t usage() const;

      CallbackHandle addCallback(std::function<void()> callback);
      void removeCallback(const CallbackHandle& callback);

      void poll();

      class UsageGuard;
      class Callback;

   private:
      std::shared_ptr<State> state;

   public:
      friend class Resolver;
      friend class Client<NetworkMode::TCP>;
      friend class Client<NetworkMode::UDP>;
      friend class Server<NetworkMode::TCP>;
      friend class Server<NetworkMode::UDP>;
      friend class Connection<NetworkMode::TCP>;
      friend class Connection<NetworkMode::UDP>;
   };

   class Context::UsageGuard {
   public:
      explicit UsageGuard(Context context);

      ~UsageGuard();

      UsageGuard(const UsageGuard&) = delete;
      UsageGuard& operator=(const UsageGuard&) = delete;

      UsageGuard(UsageGuard&& other) noexcept;

      UsageGuard& operator=(UsageGuard&& other) noexcept;

      void release();

   private:
      Context context;
      bool active = true;
   };

   class Context::Callback {
   public:
      Callback(Context context, std::function<void()> callback);

      ~Callback();

      Callback(const Callback&) = delete;
      Callback& operator=(const Callback&) = delete;

      Callback(Callback&& other) noexcept;

      Callback& operator=(Callback&& other) noexcept;

      void remove();

   private:
      Context context;
      CallbackHandle handle;
      bool active = true;
   };

   template<class Owner>
   class TickMannager {
   public:
      void startTick() {
         auto& owner = static_cast<Owner&>(*this);
         auto state = owner.state;
         if(!state || state->callback) return;
         state->callback.emplace(Context::Callback(owner.context, [st = state](){
            if(!st) return;

            std::lock_guard guard(st->mutex);
            if (!st->reference) return;
            st->reference->onTick();
         }));
      }

      void stopTick() {
         auto& owner = static_cast<Owner&>(*this);
         auto state = owner.state;
         if(!state || !state->callback) return;
         state->callback.reset();
      }

      virtual void onTick() {}
   };

   
   class Resolver : public TickMannager<Resolver> {
   private:
      friend class TickMannager<Resolver>;
      struct State {
         State() = delete;
         State(Resolver* resolverPtr, Context context) : reference(resolverPtr), tcpResolver(context.state->io), udpResolver(context.state->io), usageGuard(context) {}

         std::recursive_mutex mutex;
         Resolver* reference;
         tcp::resolver tcpResolver;
         udp::resolver udpResolver;
         Context::UsageGuard usageGuard;
         std::optional<Context::Callback> callback;
      };
   public:
      Resolver();
      Resolver(Context context);

      Resolver(Resolver&& other) noexcept;
      Resolver& operator=(Resolver&& other) noexcept;

      void shutdown();
      ~Resolver();

      void setContext(Context context);
      Context getContext() const;

      template<NetworkMode Mode>
      void resolve(const std::string& host, uint16_t port);

      virtual void onTcpResolve(std::vector<tcp::endpoint> resultEndpoints) {}
      virtual void onUdpResolve(std::vector<udp::endpoint> resultEndpoints) {}

   private:
      std::shared_ptr<State> state;
      Context context;
   };

   //TCP
   template<>
   class Client<NetworkMode::TCP> : public TickMannager<Client<NetworkMode::TCP>> {
   private:
      friend class TickMannager<Client<NetworkMode::TCP>>;
      struct State {
         State() = delete;
         State(Client* clientPtr, Context context) : reference(clientPtr), socket(context.state->io), usageGuard(context), readBuffer(20*1024) {}

         std::recursive_mutex mutex;
         Client* reference;
         tcp::socket socket;
         Context::UsageGuard usageGuard;
         std::optional<Context::Callback> callback;
         std::vector<uint8_t> readBuffer;
         std::queue<std::vector<uint8_t>> writeQueue;
         bool reading = false;
         bool writing = false;
      };
   public:
      Client();
      Client(Context context);

      Client(Client&& other) noexcept;
      Client& operator=(Client&& other) noexcept;

      void shutdown();
      ~Client();

      void setContext(Context context);
      Context getContext() const;

      void connect(std::vector<tcp::endpoint> endpoints);
      void send(const std::vector<uint8_t>& msg);
      void disconnect();

      void startRead();
      void startWrite();
      void stopRead();
      void stopWrite();

      virtual void onConnect() {}
      virtual void onRead(std::vector<uint8_t> msg) {}
      virtual void onWrite() {}
      virtual void onDisconnect() {}

   private:
      void close();
      void doRead();
      void doWrite();

      std::shared_ptr<State> state;
      Context context;
   };

   //UDP
   template<>
   class Client<NetworkMode::UDP> : public TickMannager<Client<NetworkMode::UDP>> {
   private:
      friend class TickMannager<Client<NetworkMode::UDP>>;
      struct State {
         State() = delete;
         State(Client* clientPtr, Context context) : reference(clientPtr), socket(context.state->io), usageGuard(context), readBuffer(20*1024) {}

         std::recursive_mutex mutex;
         Client* reference;
         udp::socket socket;
         Context::UsageGuard usageGuard;
         std::optional<Context::Callback> callback;
         std::vector<uint8_t> readBuffer;
         std::queue<std::vector<uint8_t>> writeQueue;
         bool reading = false;
         bool writing = false;
      };
   public:
      Client();
      Client(Context context);

      Client(Client&& other) noexcept;
      Client& operator=(Client&& other) noexcept;

      void shutdown();
      ~Client();

      void setContext(Context context);
      Context getContext() const;

      void connect(std::vector<udp::endpoint> endpoints);
      void send(const std::vector<uint8_t>& msg);
      void disconnect();

      void startRead();
      void startWrite();
      void stopRead();
      void stopWrite();

      virtual void onConnect() {}
      virtual void onRead(std::vector<uint8_t> msg) {}
      virtual void onWrite() {}
      virtual void onDisconnect() {}

   private:
      void close();
      void doRead();
      void doWrite();

      std::shared_ptr<State> state;
      Context context;
   };


   //TCP
   template<>
   class Server<NetworkMode::TCP> : public TickMannager<Server<NetworkMode::TCP>> {
   private:
   friend class TickMannager<Server<NetworkMode::TCP>>;
      struct State {
         State() = delete;
         State(Server* serverPtr, Context context) : reference(serverPtr), usageGuard(context) {}

         std::recursive_mutex mutex;
         Server* reference;
         std::vector<std::shared_ptr<tcp::acceptor>> acceptors;
         std::vector<std::shared_ptr<Connection<NetworkMode::TCP>>> connections;
         Context::UsageGuard usageGuard;
         std::optional<Context::Callback> callback;
         bool accepting = false;
      };
   public:
      Server();
      Server(Context context);

      Server(Server&& other) noexcept;
      Server& operator=(Server&& other) noexcept;

      void shutdown();
      ~Server();

      void setContext(Context context);
      Context getContext() const;
      std::vector<std::shared_ptr<Connection<NetworkMode::TCP>>> getConnections();

      void start(std::vector<tcp::endpoint> endpoints);
      void start(tcp::endpoint endpoint);

      void send(const std::vector<uint8_t>& msg);
      void disconnect();

      void startRead();
      void stopAccept();

      virtual void onStart() {}
      virtual std::shared_ptr<Connection<NetworkMode::TCP>> onAccept(tcp::socket acceptedSocket);

   private:
      void removeConnection(Connection<NetworkMode::TCP>* connection);
      void doAccept(std::shared_ptr<tcp::acceptor> acceptor);

      std::shared_ptr<State> state;
      Context context;
   public:
      friend class Connection<NetworkMode::TCP>;
   };


   //TCP
   template<>
   class Connection<NetworkMode::TCP> : public TickMannager<Connection<NetworkMode::TCP>> {
   private:
      friend class TickMannager<Connection<NetworkMode::TCP>>;
      struct State {
         State() = delete;
         State(Connection* connectionPtr, Context context, tcp::socket socket_) : reference(connectionPtr), socket(std::move(socket_)), usageGuard(context), readBuffer(20*1024) {}

         std::recursive_mutex mutex;
         Connection* reference;
         Server<NetworkMode::TCP>* server;
         tcp::socket socket;
         Context::UsageGuard usageGuard;
         std::optional<Context::Callback> callback;
         std::vector<uint8_t> readBuffer;
         std::queue<std::vector<uint8_t>> writeQueue;
         bool reading = false;
         bool writing = false;
      };
   public:
      Connection();
      Connection(Server<NetworkMode::TCP>* server, tcp::socket socket);

      Connection(Connection&& other) noexcept;
      Connection& operator=(Connection&& other) noexcept;

      void shutdown();
      ~Connection();

      void setServer(Server<NetworkMode::TCP>* server);
      void setEnv(Context context, tcp::socket socket);
      Context getContext() const;

      void start();
      void send(const std::vector<uint8_t>& msg);
      void disconnect();

      void startRead();
      void startWrite();
      void stopRead();
      void stopWrite();

      virtual void onStart() { startRead(); }
      virtual void onRead(std::vector<uint8_t> msg) {}
      virtual void onWrite() {}
      virtual void onDisconnect() {}

   private:
      void close();
      void doRead();
      void doWrite();

      std::shared_ptr<State> state;
      Context context;
   };


   //UDP
   template<>
   class Server<NetworkMode::UDP> : public TickMannager<Server<NetworkMode::UDP>> {
   public:
      struct UdpHandle {
         UdpHandle(udp::endpoint endpoint, udp::socket* socket) : endpoint(endpoint), socket(socket) {}
         UdpHandle(udp::endpoint endpoint, std::shared_ptr<udp::socket> socket) : endpoint(endpoint), socket(socket.get()) {}
         udp::endpoint endpoint;
         udp::socket* socket;
      };
   private:
      friend class TickMannager<Server<NetworkMode::UDP>>;
      struct State {
         State() = delete;
         State(Server* serverPtr, Context context) : reference(serverPtr), usageGuard(context) {}

         std::recursive_mutex mutex;
         Server* reference;
         std::vector<std::shared_ptr<udp::socket>> sockets;
         std::vector<std::shared_ptr<Connection<NetworkMode::UDP>>> connections;
         Context::UsageGuard usageGuard;
         std::optional<Context::Callback> callback;
         bool reading = false;
      };
   public:
      Server();
      Server(Context context);

      Server(Server&& other) noexcept;
      Server& operator=(Server&& other) noexcept;

      void shutdown();
      ~Server();

      void setContext(Context context);
      Context getContext() const;
      std::vector<std::shared_ptr<Connection<NetworkMode::UDP>>> getConnections();

      void start(std::vector<udp::endpoint> endpoints);
      void start(udp::endpoint endpoint);

      void send(const std::vector<uint8_t>& msg);
      std::shared_ptr<SN::Connection<SN::NetworkMode::UDP>> connect(UdpHandle handle, const std::vector<uint8_t>& msg = {});
      void disconnect();

      void startRead();
      void stopRead();

      virtual void onStart() {}
      virtual std::shared_ptr<SN::Connection<SN::NetworkMode::UDP>> onAccept(UdpHandle handle, std::vector<uint8_t> msg);
      virtual std::shared_ptr<SN::Connection<SN::NetworkMode::UDP>> onConnect(UdpHandle handle, std::vector<uint8_t> msg) { return onAccept(handle, msg); }
   private:
      void removeConnection(Connection<NetworkMode::UDP>* connection);
      void doRead(std::shared_ptr<udp::socket> socket);

      std::shared_ptr<State> state;
      Context context;
   public:
      friend class Connection<NetworkMode::UDP>;
   };


   //UDP
   template<>
   class Connection<NetworkMode::UDP> : public TickMannager<Connection<NetworkMode::UDP>> {
   private:
      friend class TickMannager<Connection<NetworkMode::UDP>>;
      struct State {
         State() = delete;
         State(Connection* connectionPtr, Context context, Server<NetworkMode::UDP>::UdpHandle handle_) : reference(connectionPtr), handle(std::move(handle_)), usageGuard(context) {};

         std::recursive_mutex mutex;
         Connection* reference;
         Server<NetworkMode::UDP>::UdpHandle handle;
         Server<NetworkMode::UDP>* server;
         Context::UsageGuard usageGuard;
         std::optional<Context::Callback> callback;
         std::queue<std::vector<uint8_t>> writeQueue;
         bool writing = false;
      };

   public:
      Connection();
      Connection(Server<NetworkMode::UDP>* server, Server<NetworkMode::UDP>::UdpHandle handle);

      Connection(Connection&& other) noexcept;
      Connection& operator=(Connection&& other) noexcept;

      void shutdown();
      ~Connection();

      void setServer(Server<NetworkMode::UDP>* server);
      void setEnv(Context context, Server<NetworkMode::UDP>::UdpHandle handle);
      Context getContext() const;

      void start();
      void send(const std::vector<uint8_t>& msg);
      void disconnect();

      void startWrite();
      void stopWrite();

      virtual void onStart() {}
      virtual void onRead(std::vector<uint8_t> msg) {}
      virtual void onWrite() {}
      virtual void onDisconnect() {}

   private:
      void close();
      void doWrite();

      std::shared_ptr<State> state;
      Context context;
   public:
      friend class Server<NetworkMode::UDP>;
   };
}

#endif // ~NCORE_STREAMED_NET_H