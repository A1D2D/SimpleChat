#include "Client.h"

#include <iostream>
#include "../Util/StringUtil.h"

SimpleClient::SimpleClient() : socket(context), resolver(context), readBuffer(20 * 1024), writeBuffer(20 * 1024) {}

void SimpleClient::connect(const std::string& host, ushort_16 port) {
   host_ = host;
   port_ = port;

   auto connectLambda = [this](const std::error_code& ec, const asio::ip::tcp::endpoint& endpoint) {
      if (ec) {
         printClient("Failed to connect: " + ec.message());
         return;
      }

      printClient("Connected to: " + endpoint.address().to_string());

      readData();
   };

   auto resolveLambda = [this, connectLambda](const std::error_code& ec, asio::ip::tcp::resolver::results_type results) {
      if (ec) {
         printClient("Failed to resolve: " + ec.message());
         return;
      }

      asio::async_connect(socket, results, connectLambda);
   };

   resolver.async_resolve(host, std::to_string(port), resolveLambda);
}

void SimpleClient::startThread() {
   thrContext = std::thread([this](){ context.run(); });
}

void SimpleClient::send(const std::string& msg) {
   std::copy(msg.begin(), msg.end(), writeBuffer.begin());

   auto writeLambda = [msg](std::error_code ec, std::size_t length) {
      if (!ec) {
         std::cout << "Sent " << length << " bytes: " << msg << std::endl;
      } else {
         std::cerr << "Send failed: " << ec.message() << std::endl;
      }
   };

   asio::async_write(socket, asio::buffer(writeBuffer.data(), msg.size()), writeLambda);
}

void SimpleClient::readData() {
   auto readLambda = [this](std::error_code ec, std::size_t length) {
      if (!ec) {
         std::cout << "Read " << length << " bytes: " << StringUtil::bytesToString({readBuffer.data(), readBuffer.data() + length}) << "\n";

         readData();
      } else {
         std::cerr << "Read failed: " << ec.message() << std::endl;
      }
   };

   socket.async_read_some(asio::buffer(readBuffer.data(), readBuffer.size()), readLambda);
}

void SimpleClient::printClient(std::string&& clientStr, const std::string& ip, ushort_16 port, bool wPort) {
   Colorb::SKY_BLUE.printAnsiStyle();
   if (wPort) {
      std::cout << "[Client: " << ip << ":" << port << "]: ";
   } else
      std::cout << "[Client]: ";
   resetAnsiStyle();
   std::cout << clientStr << "\n";
}