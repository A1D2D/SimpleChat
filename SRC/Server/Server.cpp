#include "Server.h"

#include <iostream>

SimpleServer::SimpleServer() {}

void SimpleServer::start(ushort_16 port) {
   port_ = port;
   
   acceptor.emplace(context, tcp::endpoint(tcp::v4(), port));
   std::cout << "Server listening on port " << port_ << "\n";
   std::cout << "Waiting for client...\n";
   acceptClients();
}

void SimpleServer::acceptClients() {
   if(!acceptor) return;
   pendingSocket.emplace(context);

   auto aceptLambda = [&](const asio::error_code& ec) {
      if (!ec) {
         std::unique_ptr<SimpleConnection> connection = std::make_unique<SimpleConnection>(*pendingSocket);
         sockets.emplace_back(std::move(connection));
         std::cout << "Client accepted!\n";
      } else std::cout << "Failed to accept clinet: " << ec.message() << "\n";

      acceptClients();
   };

   acceptor->async_accept(*pendingSocket, aceptLambda);
}

void SimpleServer::startThread() {
   thrContext = std::thread([this](){ context.run(); });
}

void SimpleServer::printServer(std::string&& serverStr, ushort_16 port, bool wPort) {
   Colorb::BRONZE.printAnsiStyle();
   if (wPort) {
      std::cout << "[Server: " << port << "]: ";
   } else
      std::cout << "[Server]: ";
   resetAnsiStyle();
   std::cout << serverStr << "\n";
}

