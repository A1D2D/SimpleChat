#ifndef SIMPLECHAT_SERVER_H
#define SIMPLECHAT_SERVER_H
#include <VORTEX_MP/LinearAlg>

#include "../Util/AsioInclude.h"
#include "Connection.h"

class SimpleServer {
public:
   enum State {
      Offline = 0,
      Online = FlagDef(1)
   };

public:
   SimpleServer();

   void start(ushort_16 port);
   void close();
   void joinThread();

   asio::io_context& getContext();
   ushort_16 getPort();

protected:
   virtual void onStart() {}
   virtual std::shared_ptr<SimpleConnection> onAccept(tcp::socket &socket);
   virtual void onDisconnect(std::shared_ptr<SimpleConnection> connection) {}

private:
   asio::error_code ec;
   asio::io_context context;
   std::optional<tcp::acceptor> acceptor;
   std::optional<tcp::socket> pendingSocket;
   ushort_16 port_ = 0;
   
   void acceptClients();
   void serverAbort();
   void startThread();
   void removeConnection(std::shared_ptr<SimpleConnection> connection);
   
public:
   friend class SimpleConnection;
   std::thread thrContext;
   std::atomic<ubyte_8> state = State::Offline;
   std::vector<std::shared_ptr<SimpleConnection>> connections;
   static void printServer(std::string&& serverStr, ushort_16 port = 0, bool wPort = false);
};

#endif //SIMPLECHAT_SERVER_H