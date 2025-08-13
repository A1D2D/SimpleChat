#include "Connection.h"
#include "Server.h"

#include <iostream>
#include "../Util/StringUtil.h"

SimpleConnection::SimpleConnection(asio::io_context& serverContext, SimpleServer& serverRef, tcp::socket& accepted) : 
socket(std::move(accepted)), readBuffer(20 * 1024), writeBuffer(20 * 1024), context(serverContext), server(serverRef) {
   AddFlag(state, State::Online);
   onConnect();
}

void SimpleConnection::start() {
   readData();
   onStart();
}

void SimpleConnection::disconnect() {
   auto self(shared_from_this());
   asio::post(context, [this, self]() {
      connectionAbort();
   });
}

void SimpleConnection::send(const std::vector<ubyte_8>& msg) {
   auto self(shared_from_this());
   asio::post(context, [this, msg, self]() {
      std::copy(msg.begin(), msg.end(), writeBuffer.begin());

      auto writeLambda = [this, msg, self](std::error_code ec, std::size_t length) {
         if (ec) {
            connectionAbort();
            if (ec == asio::error::eof || ec == asio::error::connection_reset) {
               // printClient("Client closed the connection.");
               return;
            } else if(ec == asio::error::operation_aborted) { 
               return;
            } else {
               std::cerr << "write failed: " << ec.message() << std::endl;
               return;
            }
         }
      };
      if(socket.is_open()) {
         asio::async_write(socket, asio::buffer(writeBuffer.data(), msg.size()), writeLambda);
      } else std::cout << "socket closed\n";
   });
}

void SimpleConnection::send(const std::string& msg) {
   send(StringUtil::stringToBytes(msg));
}

void SimpleConnection::readData() {
   auto self(shared_from_this());
   auto readLambda = [this, self](std::error_code ec, std::size_t length) {
      if (ec) {
         connectionAbort();
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

void SimpleConnection::connectionAbort() {
   if(HasFlag(state, State::Online) || socket.is_open()) {
      RemoveFlag(state, State::Online);
      onDisconnect();
      server.removeConnection(shared_from_this());
   }
}


asio::io_context& SimpleConnection::getContext() {
   return context;
}

SimpleServer& SimpleConnection::getServer() {
   return server;
}