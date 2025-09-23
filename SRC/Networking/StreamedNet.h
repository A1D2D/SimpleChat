#ifndef NETWORK_STREAMED_NET_H
#define NETWORK_STREAMED_NET_H
#include <memory>
#include <thread>

#ifdef _WIN32
   #undef WINAPI_FAMILY
   #define WIN32_WINNT 0x0A00
#endif

#include <asio.hpp>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>

#define FlagDef(ID) (1LL << ((ID)-1))
#define HasFlag(flags, flag) (((flags) & (flag)) != 0)
#define AddFlag(flags, flag) ((flags) |= (flag))
#define RemoveFlag(flags, flag) ((flags) &= ~(flag))

using asio::ip::tcp;

namespace SNImpl {
   class Client;
   class Server;
}

namespace SN {
   class StreamedNetServer;
   using ubyte_8 = std::uint8_t;
   using ushort_16 = std::uint16_t;

   class StreamedNetClient {
   public:
      enum State {
         Offline = 0,
         Online = FlagDef(1),
         Connecting = FlagDef(2),
      };
      enum class Event {
         Connected,
         Resolved,
         DataSent,
         DataReceived,
         Disconnected
      };
      enum class Error {
         AlreadyConnected,
         ConnectFailed,
         ResolveFailed,
         ConnectionClosed,
         Aborted,
         WriteFailed,
         ReadFailed,
         AbortShutdownFailed,
         AbortCloseFailed
      };

      StreamedNetClient(asio::io_context& context);
      StreamedNetClient();
      ~StreamedNetClient();

      StreamedNetClient(const StreamedNetClient&) = delete;
      StreamedNetClient& operator=(const StreamedNetClient&) = delete;

      StreamedNetClient(StreamedNetClient&& other) noexcept;
      StreamedNetClient& operator=(StreamedNetClient&& other) noexcept;

      void autoConnect(const std::string& ip, ushort_16 port);
      void send(const std::vector<ubyte_8>& msg);
      void send(const std::string& msg);
      void disconnect();
      void startThread();
      void joinThread();
      void stopContext();

      asio::io_context& getContext();
      ushort_16 getPort();
      const std::string getIp();
      tcp::resolver::results_type& getREndpoints();
      const tcp::endpoint& getCEndpoints();
      tcp::socket& getSocket();
      ubyte_8 getState();

      static void printClient(std::string&& clientStr, const std::string& ip = "localhost", ushort_16 port = 0, bool wPort = false);
      friend class SNImpl::Client;

   protected:
      virtual void onConnect() {}
      virtual void onStart() {}
      virtual void onResolve() {}
      virtual void onDisconnect() {}
      virtual void onReceive(const std::vector<ubyte_8>& data) {}
      virtual void onEvent(Event evt);
      virtual void onError(Error err, const asio::error_code& ec);

      std::thread thr;

   private:
      std::unique_ptr<asio::io_context> contextPtr_;

   protected:
      asio::io_context* context_;
   private:
      std::shared_ptr<SNImpl::Client> clientPtr;
   };

   class StreamedNetConnection : public std::enable_shared_from_this<StreamedNetConnection> {
   public:
      enum State {
         Offline = 0,
         Online = FlagDef(1)
      };
      enum class Event {
         DataSent,
         DataReceived,
         Disconnected
      };
      enum class Error {
         ConnectionClosed,
         Aborted,
         WriteFailed,
         ReadFailed
      };

      StreamedNetConnection(asio::io_context& context, StreamedNetServer& serverRef, tcp::socket& accepted);
      void start();
      void send(const std::vector<ubyte_8>& msg);
      void send(const std::string& msg);
      void disconnect();

      asio::io_context& getContext();
      StreamedNetServer& getServer();
      tcp::socket& getSocket();
      ubyte_8 getState();

      std::atomic<ubyte_8> state = State::Offline;
      friend class StreamedNetServer;
      friend class SNImpl::Server;

   protected:
      virtual void onConnect() {}
      virtual void onStart() {}
      virtual void onDisconnect() {}
      virtual void onReceive(const std::vector<ubyte_8>& data) {}
      virtual void onEvent(Event evt);
      virtual void onError(Error err, const asio::error_code& ec);

      asio::io_context& context_;
      asio::error_code ec;

   private:
      void readData();
      void connectionAbort();
      tcp::socket socket;
      StreamedNetServer& server;

      std::vector<ubyte_8> readBuffer;
      std::vector<ubyte_8> writeBuffer;
   };

   class StreamedNetServer {
   public:
      enum State {
         Offline = 0,
         Online = FlagDef(1)
      };
      enum class Event {
         OnStart,
         Aborted
      };
      enum class Error {
         AlreadyStarted,
         AcceptFailed,
         AcceptorAbortCancelFailed,
         AcceptorAbortCloseFailed,
         AbortShutdownFailed,
         AbortCloseFailed,
      };

      StreamedNetServer(asio::io_context& context);
      StreamedNetServer();
      ~StreamedNetServer();

      StreamedNetServer(const StreamedNetServer&) = delete;
      StreamedNetServer& operator=(const StreamedNetServer&) = delete;

      StreamedNetServer(StreamedNetServer&& other) noexcept;
      StreamedNetServer& operator=(StreamedNetServer&& other) noexcept;

      void start(ushort_16 port);
      void close();
      void startThread();
      void joinThread();
      void stopContext();

      asio::io_context& getContext();
      ushort_16 getPort();
      std::vector<std::shared_ptr<SN::StreamedNetConnection>> getConnections();

      static void printServer(std::string&& serverStr, ushort_16 port = 0, bool wPort = false);
      friend class SNImpl::Server;
      friend class SN::StreamedNetConnection;

   protected:
      virtual void onStart() {}
      virtual void onAbort() {}
      virtual std::shared_ptr<StreamedNetConnection> onAccept(tcp::socket& socket);
      virtual void onDisconnect(std::shared_ptr<StreamedNetConnection> connection) {}
      virtual void onEvent(Event evt) {}
      virtual void onError(Error err, const asio::error_code& ec);

   private:
      SNImpl::Server& getImpl();

   protected:
      std::thread thr;

   private:
      std::unique_ptr<asio::io_context> contextPtr_;

   protected:
      asio::io_context* context_;
   private:
      std::shared_ptr<SNImpl::Server> serverPtr;
   };
}

#endif //NETWORK_STREAMED_NET_H