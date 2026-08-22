#ifndef NCORE_STREAMED_NET_H
#define NCORE_STREAMED_NET_H

#include <memory>
#include <string>
#include <mutex>

#include "AsioInclude.h"

#define FlagDef(ID) (1LL << ((ID)-1))
#define HasFlag(flags, flag) (((flags) & (flag)) != 0)
#define HasNoFlag(flags, flag) (((flags) & (flag)) == 0)
#define AddFlag(flags, flag) ((flags) |= (flag))
#define RemoveFlag(flags, flag) ((flags) &= ~(flag))

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

   class Resolver {
   private:
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
      void doTick();
      void stopTick();

      virtual void onTcpResolve(tcp::resolver::results_type resultEndpoints) {}
      virtual void onUdpResolve(udp::resolver::results_type resultEndpoints) {}
      virtual void onTick() {}

   private:
      std::shared_ptr<State> state;
      Context context;
   };

   template<NetworkMode Mode>
   class Client;

   //TCP
   template<>
   class Client<NetworkMode::TCP> {
   public:
      Client();
      Client(Context context);

      Client(Client&& other) noexcept;
      Client& operator=(Client&& other) noexcept;

      void shutdown();
      ~Client();

      void setContext(Context context);
      Context getContext() const;

      std::vector<tcp::endpoint> endpoints;

   private:
      std::shared_ptr<std::recursive_mutex> mutex;
      std::shared_ptr<Client*> reference;
      std::shared_ptr<tcp::socket> socket;
      Context context;
   };

   template<NetworkMode Mode>
   class Server;
}

#endif // ~NCORE_STREAMED_NET_H