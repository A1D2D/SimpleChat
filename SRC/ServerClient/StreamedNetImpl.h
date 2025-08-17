#ifndef SIMPLeCHAT_STREAMED_NET_IMPL_H
#define SIMPLeCHAT_STREAMED_NET_IMPL_H
#include <memory>
#include <VORTEX_MP/LinearAlg>

#include "../Util/AsioInclude.h"

#include "StreamedNet.h"

namespace SNImpl {
   enum State {
      Offline = 0,
      Online = FlagDef(1),
      Connecting = FlagDef(2)
   };

   class Client : public std::enable_shared_from_this<Client> {
   public:
      Client(asio::io_context& context, SN::StreamedNetClient& parent);

      void connect(const std::string& host, ushort_16 port);
      void send(const std::vector<ubyte_8>& msg);
      void disconnect();

      void readData();
      void clientAbort();

      SN::StreamedNetClient* parentRef;
      asio::io_context& context_;
      asio::error_code ec;
      std::atomic<ubyte_8> state = State::Offline;

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

   };

   class Connection : std::enable_shared_from_this<Connection> {

   };
}

#endif //SIMPLeCHAT_STREAMED_NET_IMPL_H

