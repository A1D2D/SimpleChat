#include "Client.h"

#include <iostream>
#include "VORTEX_MP/Utils/Util.h"
#include "../Util/StringUtil.h"

SimpleClient::SimpleClient() : socket(context), resolver(context), readBuffer(20 * 1024), writeBuffer(20 * 1024) {}

void SimpleClient::connect(const std::string& host, ushort_16 port) {
   if(HasFlag(state, State::Online)) {
      std::cout << "already oppen\n";
      return;
   }
   AddFlag(state, State::Online);
   AddFlag(state, State::Connecting);
   joinThread();
   host_ = host;
   port_ = port;

   auto connectLambda = [this](const std::error_code& ec, const tcp::endpoint& endpoints) {
      if (ec) {
         printClient("Failed to connect: " + ec.message());
         clientAbort();
         return;
      }
      RemoveFlag(state, State::Connecting);

      // printClient("Connected to: " + endpoints.address().to_string());
      onConnect();
      readData();
   };

   auto resolveLambda = [this, connectLambda](const std::error_code& ec, tcp::resolver::results_type endpoints) {
      if (ec) {
         printClient("Failed to resolve: " + ec.message());
         clientAbort();
         return;
      }

      asio::async_connect(socket, endpoints, connectLambda);
   };

   resolver.async_resolve(host_, std::to_string(port_), resolveLambda);
   
   startThread();
}

void SimpleClient::startThread() {
   thrContext = std::thread([this](){ 
      context.reset();
      context.run();
   });
}

void SimpleClient::joinThread() {
   if (thrContext.joinable()) {
      context.stop();
      thrContext.join();
   }
}

void SimpleClient::disconnect() {
   // auto self(shared_from_this());
   asio::post(context, [this]() {
      clientAbort();
   });
   joinThread();
}

void SimpleClient::send(const std::vector<ubyte_8>& msg) {
   // auto self(shared_from_this());
   asio::post(context, [this, msg]() {
      std::copy(msg.begin(), msg.end(), writeBuffer.begin());

      auto writeLambda = [this, msg](std::error_code ec, std::size_t length) {
         if (ec) {
            clientAbort();
            if (ec == asio::error::eof || ec == asio::error::connection_reset) {
               // printClient("Server closed the connection.");
               return;
            } else if(ec == asio::error::operation_aborted) { 
               return;
            } else {
               std::cerr << "write failed: " << ec.message() << std::endl;
               return;
            }
         }
         // std::cout << "Sent " << length << " bytes: " << msg << std::endl;
      };
      if(socket.is_open()) {
         asio::async_write(socket, asio::buffer(writeBuffer.data(), msg.size()), writeLambda);
      } else std::cout << "socket closed\n";
   });
}

void SimpleClient::send(const std::string& msg) {
   send(StringUtil::stringToBytes(msg));
}

void SimpleClient::readData() {
   // auto self(shared_from_this());
   auto readLambda = [this](std::error_code ec, std::size_t length) {
      if (ec) {
         clientAbort();
         if (ec == asio::error::eof || ec == asio::error::connection_reset) {
            // printClient("Server closed the connection.");
            return;
         } else if(ec == asio::error::operation_aborted) { 
            return;
         } else {
            std::cerr << "Read failed: " << ec.message() << std::endl;
            return;
         }  
      }
      onReceive({readBuffer.data(), readBuffer.data() + length});
      readData();
   };

   socket.async_read_some(asio::buffer(readBuffer.data(), readBuffer.size()), readLambda);
}

void SimpleClient::clientAbort() {
   if(HasFlag(state, State::Online) || HasFlag(state, State::Connecting) || socket.is_open()) {
      RemoveFlag(state, State::Online);
      RemoveFlag(state, State::Connecting);
      std::error_code ec;
      ec = socket.shutdown(tcp::socket::shutdown_both, ec);
      if(ec) std::cerr << "Abort shutdown failed: " << ec.message() << std::endl;
      ec = socket.close(ec);
      if(ec) std::cerr << "Abort close failed: " << ec.message() << std::endl;
      context.stop();
      context.reset();

      onDisconnect();
   }
}


asio::io_context& SimpleClient::getContext() {
   return context;
}

ushort_16 SimpleClient::getPort() {
   return port_;
}

const std::string SimpleClient::getIp() {
   return host_;
}

void SimpleClient::printClient(std::string&& clientStr, const std::string& ip, ushort_16 port, bool wPort) {
   Colorb::SKY_BLUE.printAnsiStyle();
   if (wPort) {
      std::cout << "[Client: " << ip << ":" << port << "]: ";
   } else std::cout << "[Client]: ";
   resetAnsiStyle();
   std::cout << clientStr << "\n";
}