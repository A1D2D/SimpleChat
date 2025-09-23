#ifndef PORTBRIDGE_STREAMED_NET_H
#define PORTBRIDGE_STREAMED_NET_H
#include <memory>
#include <string>

#ifdef _WIN32
   #undef WINAPI_FAMILY
   #define WIN32_WINNT 0x0A00
#endif


#include <asio.hpp>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>

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

namespace SNImpl {
   using ubyte_8 = unsigned char;
   using ushort_16 = unsigned short;

   class NetStream {
   public:
      NetStream(std::shared_ptr<asio::io_context> context, tcp::socket& socket);
      NetStream(std::shared_ptr<asio::io_context> context);
   
      void startRead();
      void startWrite();
      void stopRead();
      void stopWrite();
      void disconnect();

      void doRead();
      void doWrite();
      void doTick();
      ~NetStream();

      virtual void onRead() {}
      virtual void onWrite() {}
      virtual void onDisconnect() {}
      virtual void onTick() {}

      std::atomic<int> state;

      std::shared_ptr<asio::io_context> context_;
      tcp::socket socket_;
      std::mutex mutex;

      std::vector<ubyte_8> readBuffer;
      std::vector<ubyte_8> writeBuffer;

      SN::TSQueue<ubyte_8> writeQ;
      SN::TSQueue<ubyte_8> readQ;
   };

   class Client : public NetStream {
      Client(std::shared_ptr<asio::io_context> context);
      void resolve(const std::string& host, ushort_16 port);
      void addEndpoint(const std::string& host, ushort_16 port);
      void connect();

      virtual void onResolve() {}
      virtual void onConnect() {}

      tcp::resolver resolver;

      std::vector<tcp::endpoint> endpoints;
   };

   class Connection : public NetStream {

   };

   class Server {

   };
}

#endif