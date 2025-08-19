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
   enum ClientEvent {
      Resolved,
      Connected,
      Disconnected,
      DataSent,
      DataReceived
   };

   enum ClientError {
      AlreadyConnected,
      AlreadyResolved,
      ResolveFailed,
      ConnectFailed,
      ReadFailed,
      WriteFailed,
      ConnectionClosed,
      Aborted,
      AbortShutdownFailed,
      AbortCloseFailed
   };

   class StreamedNetClient {
   public:
      StreamedNetClient(asio::io_context& context);
      StreamedNetClient();
      ~StreamedNetClient();

   private:
      std::unique_ptr<asio::io_context> contextPtr_;

      friend class SNImpl::Client;

   protected:
      asio::io_context* context_;
      virtual void onConnect() {}
      virtual void onResolve() {}
      virtual void onDisconnect() {}
      virtual void onReceive(const std::vector<ubyte_8>& data) {}
      virtual void onEvent(ClientEvent evt);
      virtual void onError(ClientError err, const asio::error_code& ec);

      std::thread thr;

   public:
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

   private:
      std::shared_ptr<SNImpl::Client> clientPtr;

   public:
      static void printClient(std::string&& clientStr,const std::string& ip = "localhost", ushort_16 port = 0, bool wPort = false);
   };


   class StreamedNetConnection : public std::enable_shared_from_this<StreamedNetConnection> {
   private:
      enum State {
         Offline = 0,
         Online = FlagDef(1)
      };

   public:
      StreamedNetConnection(asio::io_context& context, SNImpl::Server& serverRef, tcp::socket& accepted);
      StreamedNetConnection(SNImpl::Server& serverRef, tcp::socket& accepted);
      void start();
      void send(const std::vector<ubyte_8>& msg);
      void send(const std::string& msg);

      void disconnect();

      asio::io_context& getContext();
      SNImpl::Server& getServer();

   protected:
      virtual void onConnect() {}
      virtual void onStart() {}
      virtual void onDisconnect() {}
      virtual void onReceive(const std::vector<ubyte_8>& data) {}

   protected:
      asio::io_context* context_;

   };


   class StreamedNetServer {
   public:
      // StreamedNetServer(asio::io_context& context);
      // StreamedNetServer();
      // ~StreamedNetServer();

   private:
      std::unique_ptr<asio::io_context> contextPtr_;
      
   protected:
      asio::io_context* context_;
   };
}

#endif //SIMPLeCHAT_STREAMED_NET_H

