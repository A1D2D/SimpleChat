#ifndef NCORE_STREAMED_NET_H
#define NCORE_STREAMED_NET_H

#include <memory>
#include <string>

#include "../Util/AsioInclude.h"
#include "NetContextRef.h"
#include "NetTSQueue.h"


#define FlagDef(ID) (1LL << ((ID)-1))
#define HasFlag(flags, flag) (((flags) & (flag)) != 0)
#define HasNoFlag(flags, flag) (((flags) & (flag)) == 0)
#define AddFlag(flags, flag) ((flags) |= (flag))
#define RemoveFlag(flags, flag) ((flags) &= ~(flag))

using asio::ip::tcp;

#define SNI_OFFLINE 0
#define SNI_ONLINE 1
#define SNI_RESOLVEING 2
#define SNI_CONNECTING 4
#define SNI_IN_READ 8
#define SNI_STOP_READ_R 16
#define SNI_IN_WRITE 32
#define SNI_STOP_WRITE_R 64
#define SNI_IN_ACCEPT 128
#define SNI_STOP_ACCEPT_R 256

namespace SN {
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

   class NetStream;
   class Server;

   class NetStreamAsioW : public std::enable_shared_from_this<SN::NetStreamAsioW> {
   public:
      NetStreamAsioW(SN::IOContextHandle&& context, NetStream* parent, tcp::socket&& socket);
      NetStreamAsioW(SN::IOContextHandle&& context, NetStream* parent);

      void startRead();
      void startWrite();
      void abortHalt();
      
      void doTick();
      void doRead();
      void doWrite();

      ~NetStreamAsioW();

      SN::IOContextHandle context;
      tcp::socket socket;
      NetStream* parent = nullptr;
      std::recursive_mutex guardMutex;
      asio::error_code ec;
   };

   class NetStream {
   public:
      NetStream(SN::IOContextController&& controller, tcp::socket&& socket);
      NetStream(SN::IOContextController&& controller);
      
      friend class NetStreamAsioW;

      void startRead();
      void startWrite();
      void stopRead();
      void stopWrite();

      void send(const std::vector<uint8_t> msg);
      void disconnect();
      void shutdown();

      SN::IOContextHandle& getContext();
      SN::IOContextRunner& getRunner();
      SN::IOContextController getControllerClone();
      tcp::socket& getSocket();
      std::shared_ptr<SN::NetStreamAsioW> getHandle();
      ~NetStream();

   protected:
      virtual void abortHalt();

      virtual void onRead() {}
      virtual void onWrite() {}
      virtual void onDisconnect() {}
      virtual void onTick() {}

      virtual void onEvent(Event evt);
      virtual void onError(Error err, const asio::error_code& ec);

   public:
      std::atomic<int> state = SNI_OFFLINE;
      std::shared_ptr<NetStreamAsioW> processHandler;

      SN::IOContextRunner runner;
      std::vector<uint8_t> readBuffer;
      SN::TSQueue<std::vector<uint8_t>> writeQ;
      SN::TSQueue<uint8_t> readQ;
   };

   class Client : public NetStream {
   public:
      Client();
      Client(SN::IOContextController&& controller);
      Client(SN::IOContextHandle&& handle);
      Client(SN::IOContextRunner&& runner);
      Client(SN::IOContextHandle&& handle, SN::IOContextRunner&& runner);
      

      void resolve(const std::string& host, uint16_t port);
      void addEndpoint(const std::string& host, uint16_t port);
      void connect();
      void disconnect();

   protected:
      virtual void abortHalt() override;

      virtual void onResolve() {}
      virtual void onConnect() {}

      virtual void onRead() override {}
      virtual void onWrite() override {}
      virtual void onDisconnect() override {}
      virtual void onTick() override {}

      virtual void onEvent(Event evt) override;
      virtual void onError(Error err, const asio::error_code& ec) override;

   protected:
      std::shared_ptr<tcp::resolver> resolver;
      std::vector<tcp::endpoint> endpoints;

   public:
      static void printClient(std::string&& clientStr, const std::string& ip = "localhost", uint16_t port = 0, bool wPort = false);
   };

   class Connection : public NetStream {
   public:
      Connection(SN::IOContextController&& context, Server& server, tcp::socket& accepted);

      void start();
      Server& getServer();

      void disconnect();

   protected:
      virtual void abortHalt() override;

      virtual void onConnect() {}
      virtual void onStart() {}

      virtual void onRead() override {}
      virtual void onWrite() override {}
      virtual void onDisconnect() override {}
      virtual void onTick() override {}

      virtual void onEvent(Event evt) override;
      virtual void onError(Error err, const asio::error_code& ec) override;

      Server& server;
      friend class Server;
   };

   class ServerAsioW : public std::enable_shared_from_this<SN::ServerAsioW> {
   public:
      ServerAsioW(SN::IOContextHandle&& context, SN::Server* parent);

      void startAccept();
      void abortHalt();

      void doTick();
      void doAccept();

      ~ServerAsioW();

      SN::IOContextHandle context;
      std::optional<tcp::acceptor> acceptor;
      std::optional<tcp::socket> pendingSocket;
      SN::Server* parent = nullptr;
      std::recursive_mutex guardMutex;
      asio::error_code ec;
   };

   class Server {
   public:
      Server();
      Server(SN::IOContextController&& controller);
      Server(SN::IOContextHandle&& handle);
      Server(SN::IOContextRunner&& runner);
      Server(SN::IOContextHandle&& handle, SN::IOContextRunner&& runner);

      friend class ServerAsioW;

      void start(uint16_t port);
      void startAccept();
      void stopAccept();

      void close();
      void removeConnection(Connection* connectionPtr);
      void shutdown();
      
      SN::IOContextHandle& getContext();
      SN::IOContextRunner& getRunner();
      SN::IOContextController getControllerClone();
      std::optional<tcp::acceptor>& getAcceptor();
      std::optional<tcp::socket>& getPending();
      uint16_t getPort();
      std::vector<std::shared_ptr<SN::Connection>>& getConnections();
      std::shared_ptr<SN::ServerAsioW> getHandle();
      ~Server();

   protected:
      virtual void abortHalt();

      virtual std::shared_ptr<Connection> onAccept(tcp::socket& socket);
      virtual void onStart() { startAccept(); }

      virtual void onTick() {}
      virtual void onDisconnect(std::shared_ptr<Connection> connection) {}

      virtual void onEvent(Event evt);
      virtual void onError(Error err, const asio::error_code& ec);

   public:
      std::atomic<int> state = SNI_OFFLINE;
      std::shared_ptr<ServerAsioW> processHandler;

      SN::IOContextRunner runner;

      std::vector<std::shared_ptr<Connection>> connections;
      uint16_t port = 0;

   public:
      static void printServer(std::string&& serverStr, uint16_t port = 0, bool wPort = false);
   };
}

#endif // ~NCORE_STREAMED_NET_H