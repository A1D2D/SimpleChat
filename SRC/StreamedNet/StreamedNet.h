#ifndef SIMPLeCHAT_STREAMED_NET_H
#define SIMPLeCHAT_STREAMED_NET_H
#include <memory>

#include <VORTEX_MP/LinearAlg>
#include "../Util/AsioInclude.h"
#include <thread>

namespace SNImpl {
   class Client;
   class Server;
}

namespace SN {
   class StreamedNetServer;


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

      static void printClient(std::string&& clientStr, const std::string& ip = "localhost", ushort_16 port = 0, bool wPort = false);
      friend class SNImpl::Client;

   protected:
      virtual void onConnect() {}
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

      VArray<ubyte_8> readBuffer;
      VArray<ubyte_8> writeBuffer;
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

      std::thread thr;

   private:
      SNImpl::Server& getImpl();

      std::unique_ptr<asio::io_context> contextPtr_;

   protected:
      asio::io_context* context_;
   private:
      std::shared_ptr<SNImpl::Server> serverPtr;
   };
}

#endif //SIMPLeCHAT_STREAMED_NET_H

