#include "Connection.h"

#include <iostream>
#include "../Util/StringUtil.h"

SimpleConnection::SimpleConnection(tcp::socket& accepted) : socket(std::move(accepted)), readBuffer(20 * 1024), writeBuffer(20 * 1024) {
   readData();
}

void SimpleConnection::readData() {
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

void SimpleConnection::send(const std::string& msg) {
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
