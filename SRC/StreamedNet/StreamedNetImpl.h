#ifndef SIMPLeCHAT_STREAMED_NET_IMPL_H
#define SIMPLeCHAT_STREAMED_NET_IMPL_H
#include <memory>
#include <VORTEX_MP/LinearAlg>

#include "../Util/AsioInclude.h"

#include "StreamedNet.h"

namespace SNImpl {
   class Client : public std::enable_shared_from_this<Client> {
   public:
      Client(asio::io_context& context, SN::StreamedNetClient& parent);

      void autoConnect(const std::string& host, ushort_16 port);
      
      void send(const std::vector<ubyte_8>& msg);
      void disconnect();

      void readData();
      void clientAbort();

      SN::StreamedNetClient* parentRef;
      asio::io_context& context_;
      asio::error_code ec;
      std::atomic<ubyte_8> state = SN::StreamedNetClient::Offline;

      tcp::resolver resolver;
      tcp::socket socket;

      ushort_16 port_ = 0;
      std::string host_;
      tcp::endpoint connectedEndpoints;
      tcp::resolver::results_type resolvedEndpoints;

      VArray<ubyte_8> readBuffer;
      VArray<ubyte_8> writeBuffer;
   };

   class Server : std::enable_shared_from_this<Server> {
   public:
      Server(asio::io_context& context, SN::StreamedNetServer& parent);

      void start(ushort_16 port);
      void close();
      
      void acceptClients();
      void serverAbort();
      void removeConnection(std::shared_ptr<SN::StreamedNetConnection> connection);

      SN::StreamedNetServer* parentRef;
      asio::io_context& context_;
      asio::error_code ec;
      std::atomic<ubyte_8> state = SN::StreamedNetServer::Offline;
      std::optional<tcp::acceptor> acceptor;
      std::optional<tcp::socket> pendingSocket;
      ushort_16 port_ = 0;

      std::vector<std::shared_ptr<SN::StreamedNetConnection>> connections;
   };
}

#endif //SIMPLeCHAT_STREAMED_NET_IMPL_H

