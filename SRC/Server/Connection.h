#ifndef SIMPLECHAT_CONNECTION_H
#define SIMPLECHAT_CONNECTION_H
#include <VORTEX_MP/LinearAlg>

#include "../Util/AsioInclude.h"

class SimpleServer;

class SimpleConnection : public std::enable_shared_from_this<SimpleConnection> {
public:
   enum State {
      Offline = 0,
      Online = FlagDef(1)
   };

public:
   SimpleConnection(asio::io_context& serverContext, SimpleServer& serverRef, tcp::socket& accepted);
   void start();
   void send(const std::vector<ubyte_8>& msg);
   void send(const std::string& msg);
   void disconnect();

   asio::io_context& getContext();
   SimpleServer& getServer();

protected:
   virtual void onConnect() {}
   virtual void onStart() {}
   virtual void onDisconnect() {}
   virtual void onReceive(const std::vector<ubyte_8>& data) {}

private:
   void readData();
   void connectionAbort();
   tcp::socket socket;
   asio::io_context& context;
   SimpleServer& server;

   std::vector<ubyte_8> readBuffer;
   std::vector<ubyte_8> writeBuffer;
public:
   friend class SimpleServer;
   std::atomic<ubyte_8> state = State::Offline;
};

#endif //SIMPLECHAT_CONNECTION_H