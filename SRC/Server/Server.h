#ifndef SIMPLECHAT_SERVER_H
#define SIMPLECHAT_SERVER_H
#include <VORTEX_MP/LinearAlg>

#include "../Util/AsioInclude.h"
#include "Connection.h"

class SimpleServer {
public:
   SimpleServer();

   void start(ushort_16 port);
   void startThread();
private:
   asio::error_code ec;
   asio::io_context context;
   std::thread thrContext;
   std::optional<tcp::acceptor> acceptor;
   std::optional<tcp::socket> pendingSocket;
   ushort_16 port_ = 0;
   
   void acceptClients();
public:
   std::vector<std::unique_ptr<SimpleConnection>> sockets;
   static void printServer(std::string&& serverStr, ushort_16 port = 0, bool wPort = false);
};

#endif //SIMPLECHAT_SERVER_H