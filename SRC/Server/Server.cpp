#include "Server.h"

#include <iostream>

SimpleServer::SimpleServer() {}

void SimpleServer::start(ushort_16 port) {
   if(HasFlag(state, State::Online)) {
      std::cout << "already oppen\n";
      return;
   }
   AddFlag(state, State::Online);
   joinThread();
   connections.clear();
   port_ = port;
   
   acceptor.emplace(context, tcp::endpoint(tcp::v4(), port));
   onStart();
   acceptClients();
   startThread();
}

void SimpleServer::startThread() {
   thrContext = std::thread([this](){
      context.reset();
      context.run(); 
   });
}

void SimpleServer::joinThread() {
   if (thrContext.joinable()) {
      context.stop();
      thrContext.join();
   }
}

void SimpleServer::acceptClients() {
   if(!acceptor) return;
   pendingSocket.emplace(context);

   auto aceptLambda = [&](const asio::error_code& ec) {
      if (ec) {
         printServer("Failed to accept clinet: " + ec.message());
         serverAbort();
         return;
      } else {
         std::shared_ptr<SimpleConnection> connection = onAccept(*pendingSocket);
         connection->start();
         connections.emplace_back(std::move(connection));
      }
      acceptClients();
   };

   acceptor->async_accept(*pendingSocket, aceptLambda);
}

void SimpleServer::close() {
   asio::post(context, [this]() {
      serverAbort();
   });
   joinThread();
}

void SimpleServer::serverAbort() {
   if(HasFlag(state, State::Online)) {
      RemoveFlag(state, State::Online);

      ec = acceptor->cancel(ec);
      if(ec) std::cerr << "Abort acceptor failed: " << ec.message() << std::endl;
      ec = acceptor->close(ec);
      if(ec) std::cerr << "Abort close failed: " << ec.message() << std::endl;

      auto removeLambda = [&](std::shared_ptr<SimpleConnection>& connection) {
         tcp::socket& socket = connection->socket;
         if (socket.is_open()) {
            ec = socket.shutdown(tcp::socket::shutdown_both, ec);
            if(ec) std::cerr << "Abort shutdown failed: " << ec.message() << std::endl;
            ec = socket.close(ec);
            if(ec) std::cerr << "Abort close failed: " << ec.message() << std::endl;
         }
         return true;
      };

      connections.erase(
         std::remove_if(connections.begin(), connections.end(), removeLambda),
         connections.end()
      );
      context.stop();
      context.reset();
      // onDisconnect();
   }
}

void SimpleServer::removeConnection(std::shared_ptr<SimpleConnection> connection) {
   auto it = std::find(connections.begin(), connections.end(), connection);
   if (it != connections.end()) {
      std::error_code ec;
      onDisconnect(connection);

      ec = connection->socket.shutdown(tcp::socket::shutdown_both, ec);
      if(ec) std::cerr << "Abort shutdown failed: " << ec.message() << std::endl;

      ec = connection->socket.close(ec);
      if(ec) std::cerr << "Abort close failed: " << ec.message() << std::endl;

      connections.erase(it);
   }
}


std::shared_ptr<SimpleConnection> SimpleServer::onAccept(tcp::socket &socket) {
  return std::make_shared<SimpleConnection>(context, *this, socket);
}


asio::io_context& SimpleServer::getContext() {
   return context;
}

ushort_16 SimpleServer::getPort() {
   return port_;
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
