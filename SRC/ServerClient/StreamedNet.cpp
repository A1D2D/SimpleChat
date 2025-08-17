#include "StreamedNet.h"
#include "StreamedNetImpl.h"
#include "asio/io_context.hpp"
#include <memory>
#include <iostream>
#include "../Util/StringUtil.h"

using namespace SN;
using namespace SNImpl;

SNImpl::Client::Client(asio::io_context& context, SN::StreamedNetClient& parent) : context_(context), parentRef(&parent), resolver(context), socket(context), readBuffer(20 * 1024), writeBuffer(20 * 1024) {}

void SNImpl::Client::connect(const std::string& host, ushort_16 port) {
   if(HasFlag(state, State::Online)) {
      if(parentRef) parentRef->onError(AlreadyConnected, ec);
      return;
   }
   AddFlag(state, State::Online);
   AddFlag(state, State::Connecting);
   if(parentRef) parentRef->joinThreadIfA();

   host_ = host;
   port_ = port;

   auto self(shared_from_this());
   auto connectLambda = [this, self](const std::error_code& ec, const tcp::endpoint& endpoints_) {
      connectedEndpoints = endpoints_;
      if (ec) {
         if(parentRef) parentRef->onError(ConnectFailed, ec);
         clientAbort();
         return;
      }
      RemoveFlag(state, State::Connecting);

      if(parentRef) {
         parentRef->onEvent(Connected);
         parentRef->onConnect();
      }
      readData();
   };

   auto resolveLambda = [this, connectLambda, self](const std::error_code& ec, tcp::resolver::results_type endpoints_) {
      resolvedEndpoints = endpoints_;
      if (ec) {
         if(parentRef) parentRef->onError(ResolveFailed, ec);
      
         clientAbort();
         return;
      }
      if(parentRef) {
         parentRef->onEvent(Resolved);
         parentRef->onResolve();
      }

      asio::async_connect(socket, resolvedEndpoints, connectLambda);
   };

   resolver.async_resolve(host_, std::to_string(port_), resolveLambda);

   if(parentRef) parentRef->startThreadIfA();
}

void SNImpl::Client::send(const std::vector<ubyte_8>& msg) {
   auto self(shared_from_this());
   asio::post(context_, [this, msg, self]() {
      std::copy(msg.begin(), msg.end(), writeBuffer.begin());

      auto writeLambda = [this, msg, self](std::error_code ec, std::size_t length) {
         if (ec) {
            clientAbort();
            if (ec == asio::error::eof || ec == asio::error::connection_reset) {
               if(parentRef) parentRef->onError(ConnectionClosed, ec);
               return;
            } else if(ec == asio::error::operation_aborted) {
               if(parentRef) parentRef->onError(Aborted, ec);
               return;
            } else {
               if(parentRef) parentRef->onError(WriteFailed, ec);
               return;
            }
         }
         if(parentRef) parentRef->onEvent(DataSent);
      };
      if(socket.is_open()) {
         asio::async_write(socket, asio::buffer(writeBuffer.data(), msg.size()), writeLambda);
      } else if(parentRef) parentRef->onError(ConnectionClosed, ec);
   });
}

void SNImpl::Client::disconnect() {
   auto self(shared_from_this());
   asio::post(context_, [this, self]() {
      clientAbort();
   });
   if(parentRef) parentRef->joinThreadIfA();
}

void SNImpl::Client::readData() {
   auto self(shared_from_this());
   auto readLambda = [this, self](std::error_code ec, std::size_t length) {
      if (ec) {
         clientAbort();
         if (ec == asio::error::eof || ec == asio::error::connection_reset) {
            if(parentRef) parentRef->onError(ConnectionClosed, ec);
            return;
         } else if(ec == asio::error::operation_aborted) {
            if(parentRef) parentRef->onError(Aborted, ec);
            return;
         } else {
            if(parentRef) parentRef->onError(ReadFailed, ec);
            return;
         }  
      }
      if(parentRef) {
         parentRef->onReceive({readBuffer.data(), readBuffer.data() + length});
         parentRef->onEvent(DataReceived);
      }
      readData();
   };

   socket.async_read_some(asio::buffer(readBuffer.data(), readBuffer.size()), readLambda);
}

void SNImpl::Client::clientAbort() {
   if(HasFlag(state, State::Online) || HasFlag(state, State::Connecting) || socket.is_open()) {
      RemoveFlag(state, State::Online);
      RemoveFlag(state, State::Connecting);
      std::error_code ec;
      ec = socket.shutdown(tcp::socket::shutdown_both, ec);
      if(ec && parentRef) parentRef->onError(AbortShutdownFailed, ec);
      ec = socket.close(ec);
      if(ec && parentRef) parentRef->onError(AbortCloseFailed, ec);

      if(parentRef) {
         parentRef->stopContextIfA();
         parentRef->onDisconnect();
         parentRef->onEvent(Disconnected);
      }
   }
}

SN::StreamedNetClient::StreamedNetClient() : contextPtr_(std::make_unique<asio::io_context>()), context_(contextPtr_.get()),
clientPtr(std::make_shared<SNImpl::Client>(*context_, *this)) {
}

SN::StreamedNetClient::StreamedNetClient(asio::io_context& context) : contextPtr_(nullptr), context_(&context), 
clientPtr(std::make_shared<SNImpl::Client>(*context_, *this))  {
}

SN::StreamedNetClient::~StreamedNetClient() {
   clientPtr->parentRef = nullptr;
}

void SN::StreamedNetClient::startThreadIfA() {
   if(!contextPtr_ || !context_) return;

   thr = std::thread([this](){ 
      context_->reset();
      context_->run();
   });
}

void SN::StreamedNetClient::joinThreadIfA() {
   if(!contextPtr_ || !context_) return;

   if (thr.joinable()) {
      context_->stop();
      thr.join();
   }
}

void SN::StreamedNetClient::stopContextIfA() {
   if(!contextPtr_ || !context_) return;

   context_->stop();
   context_->reset();
}

void SN::StreamedNetClient::connect(const std::string& ip, ushort_16 port) {
   clientPtr->connect(ip, port);
}

void SN::StreamedNetClient::send(const std::vector<ubyte_8>& msg) {
   clientPtr->send(msg);
}

void SN::StreamedNetClient::send(const std::string& msg) {
   clientPtr->send(StringUtil::stringToBytes(msg));
}

void SN::StreamedNetClient::disconnect() {
   clientPtr->disconnect();
}

void SN::StreamedNetClient::joinThread() {
   joinThreadIfA();
}

asio::io_context& SN::StreamedNetClient::getContext() {
   return clientPtr->context_;
}

ushort_16 SN::StreamedNetClient::getPort() {
   return clientPtr->port_;
}

const std::string SN::StreamedNetClient::getIp() {
   return clientPtr->host_;
}

const tcp::resolver::results_type& SN::StreamedNetClient::getREndpoints() {
   return clientPtr->resolvedEndpoints;
}

const tcp::endpoint& SN::StreamedNetClient::getCEndpoints() {
   return clientPtr->connectedEndpoints;
}

void SN::StreamedNetClient::onEvent(ClientEvent evt) {
   switch (evt) {
      case Resolved:
         printClient("Enpoints resolved" + getREndpoints()->host_name() + ", " + getREndpoints()->service_name() + ", " + getREndpoints()->endpoint().address().to_string());
         break;

      case Connected:
         printClient("Connected to: " + getCEndpoints().address().to_string());
         break;
      case Disconnected:
         printClient("Disconnected");
         break;

      default:
         break;
   }
}

void SN::StreamedNetClient::onError(ClientError err, const asio::error_code& ec) {
   switch (err) {
      case AlreadyConnected:
         std::cout << "already oppen\n";
         break;
      case ConnectFailed:
         printClient("Failed to connect: " + ec.message());
         break;
      case ResolveFailed:
         printClient("Failed to resolve: " + ec.message());
         break;
      case ConnectionClosed:
         printClient("Server closed the connection.");
         break;
      case Aborted:
         //connectionAborted
         break;
      case WriteFailed:
         std::cerr << "write failed: " << ec.message() << std::endl;
         break;
      case ReadFailed:
         std::cerr << "Read failed: " << ec.message() << std::endl;
         break;
      case AbortShutdownFailed:
         std::cerr << "Abort shutdown failed: " << ec.message() << std::endl;
         break;
      case AbortCloseFailed:
         std::cerr << "Abort close failed: " << ec.message() << std::endl;
         break;
      default:
         break;
   }
}

void SN::StreamedNetClient::printClient(std::string&& clientStr, const std::string& ip, ushort_16 port, bool wPort) {
   Colorb::SKY_BLUE.printAnsiStyle();
   if (wPort) {
      std::cout << "[Client: " << ip << ":" << port << "]: ";
   } else
      std::cout << "[Client]: ";
   resetAnsiStyle();
   std::cout << clientStr << "\n";
}