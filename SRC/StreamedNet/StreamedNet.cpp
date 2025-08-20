#include "StreamedNet.h"
#include "StreamedNetImpl.h"
#include "asio/io_context.hpp"
#include <memory>
#include <iostream>
#include "../Util/StringUtil.h"

using namespace SN;
using namespace SNImpl;
using SNC = StreamedNetClient;
using SNS = StreamedNetServer;

/*CLIENT*/
Client::Client(asio::io_context& context, StreamedNetClient& parent) : context_(context), parentRef(&parent), resolver(context), socket(context), readBuffer(20 * 1024), writeBuffer(20 * 1024) {}

void Client::autoConnect(const std::string& host, ushort_16 port) {
   if (HasFlag(state, SNC::Online)) {
      if (parentRef) parentRef->onError(SNC::Error::AlreadyConnected, ec);
      return;
   }
   AddFlag(state, SNC::Online);
   AddFlag(state, SNC::Connecting);
   if (parentRef) parentRef->joinThread();

   host_ = host;
   port_ = port;

   auto self(shared_from_this());
   auto connectLambda = [this, self](const std::error_code& ec, const tcp::endpoint& endpoints_) {
      connectedEndpoints = endpoints_;
      if (ec) {
         if (parentRef) parentRef->onError(SNC::Error::ConnectFailed, ec);
         clientAbort();
         return;
      }
      RemoveFlag(state, StreamedNetClient::Connecting);

      if (parentRef) {
         parentRef->onEvent(SNC::Event::Connected);
         parentRef->onConnect();
      }
      readData();
      };

   auto resolveLambda = [this, connectLambda, self](const std::error_code& ec, tcp::resolver::results_type endpoints_) {
      resolvedEndpoints = endpoints_;
      if (ec) {
         if (parentRef) parentRef->onError(SNC::Error::ResolveFailed, ec);

         clientAbort();
         return;
      }
      if (parentRef) {
         parentRef->onEvent(SNC::Event::Resolved);
         parentRef->onResolve();
      }

      asio::async_connect(socket, resolvedEndpoints, connectLambda);
   };

   resolver.async_resolve(host_, std::to_string(port_), resolveLambda);

   if (parentRef) parentRef->startThread();
}

void Client::send(const std::vector<ubyte_8>& msg) {
   auto self(shared_from_this());
   asio::post(context_, [this, msg, self]() {
      std::copy(msg.begin(), msg.end(), writeBuffer.begin());

      auto writeLambda = [this, msg, self](std::error_code ec, std::size_t length) {
         if (ec) {
            clientAbort();
            if (ec == asio::error::eof || ec == asio::error::connection_reset) {
               if (parentRef) parentRef->onError(SNC::Error::ConnectionClosed, ec);
               return;
            } else if (ec == asio::error::operation_aborted) {
               if (parentRef) parentRef->onError(SNC::Error::Aborted, ec);
               return;
            } else {
               if (parentRef) parentRef->onError(SNC::Error::WriteFailed, ec);
               return;
            }
         }
         if (parentRef) parentRef->onEvent(SNC::Event::DataSent);
         };
      if (socket.is_open()) {
         asio::async_write(socket, asio::buffer(writeBuffer.data(), msg.size()), writeLambda);
      } else if (parentRef) parentRef->onError(SNC::Error::ConnectionClosed, ec);
   });
}

void Client::disconnect() {
   auto self(shared_from_this());
   asio::post(context_, [this, self]() {
      clientAbort();
   });
   if (parentRef) parentRef->joinThread();
}

void Client::readData() {
   auto self(shared_from_this());
   auto readLambda = [this, self](std::error_code ec, std::size_t length) {
      if (ec) {
         clientAbort();
         if (ec == asio::error::eof || ec == asio::error::connection_reset) {
            if (parentRef) parentRef->onError(SNC::Error::ConnectionClosed, ec);
            return;
         } else if (ec == asio::error::operation_aborted) {
            if (parentRef) parentRef->onError(SNC::Error::Aborted, ec);
            return;
         } else {
            if (parentRef) parentRef->onError(SNC::Error::ReadFailed, ec);
            return;
         }
      }
      if (parentRef) {
         parentRef->onReceive({ readBuffer.data(), readBuffer.data() + length });
         parentRef->onEvent(SNC::Event::DataReceived);
      }
      readData();
   };

   socket.async_read_some(asio::buffer(readBuffer.data(), readBuffer.size()), readLambda);
}

void Client::clientAbort() {
   if (HasFlag(state, SNC::Online) || HasFlag(state, SNC::Connecting) || socket.is_open()) {
      RemoveFlag(state, SNC::Online);
      RemoveFlag(state, SNC::Connecting);
      ec = socket.shutdown(tcp::socket::shutdown_both, ec);
      if (ec && parentRef) parentRef->onError(SNC::Error::AbortShutdownFailed, ec);
      ec = socket.close(ec);
      if (ec && parentRef) parentRef->onError(SNC::Error::AbortCloseFailed, ec);

      if (parentRef) {
         parentRef->stopContext();
         parentRef->onDisconnect();
         parentRef->onEvent(SNC::Event::Disconnected);
      }
   }
}

/*CLIENT_WRAPPER*/
SNC::StreamedNetClient() : contextPtr_(std::make_unique<asio::io_context>()), context_(contextPtr_.get()),
clientPtr(std::make_shared<Client>(*context_, *this)) {
}

SNC::StreamedNetClient(asio::io_context& context) : contextPtr_(nullptr), context_(&context),
clientPtr(std::make_shared<Client>(*context_, *this)) {
}

SNC::~StreamedNetClient() {
   clientPtr->parentRef = nullptr;
}

void SNC::autoConnect(const std::string& ip, ushort_16 port) {
   clientPtr->autoConnect(ip, port);
}

void SNC::send(const std::vector<ubyte_8>& msg) {
   clientPtr->send(msg);
}

void SNC::send(const std::string& msg) {
   clientPtr->send(StringUtil::stringToBytes(msg));
}

void SNC::disconnect() {
   clientPtr->disconnect();
}

void SNC::startThread() {
   if (!contextPtr_ || !context_) return;

   thr = std::thread([this]() {
      context_->reset();
      context_->run();
      });
}

void SNC::joinThread() {
   if (!contextPtr_ || !context_) return;

   if (thr.joinable()) {
      context_->stop();
      thr.join();
   }
}

void SNC::stopContext() {
   if (!contextPtr_ || !context_) return;

   context_->stop();
   context_->reset();
}

asio::io_context& SNC::getContext() {
   return clientPtr->context_;
}

ushort_16 SNC::getPort() {
   return clientPtr->port_;
}

const std::string SNC::getIp() {
   return clientPtr->host_;
}

tcp::resolver::results_type& SNC::getREndpoints() {
   return clientPtr->resolvedEndpoints;
}

const tcp::endpoint& SNC::getCEndpoints() {
   return clientPtr->connectedEndpoints;
}

void SNC::onEvent(Event evt) {
   switch (evt) {
   case SNC::Event::Resolved:
      printClient("Enpoints resolved: " + getREndpoints()->host_name() + ", " + getREndpoints()->service_name() + ", " + getREndpoints()->endpoint().address().to_string(),
         getIp(), getPort(), true);
      break;

   case SNC::Event::Connected:
      printClient("Connected to: " + getCEndpoints().address().to_string(), getIp(), getPort(), true);
      break;
   case SNC::Event::Disconnected:
      printClient("Disconnected");
      break;

   default:
      break;
   }
}

void SNC::onError(Error err, const asio::error_code& ec) {
   switch (err) {
   case SNC::Error::AlreadyConnected:
      std::cout << "already oppen\n";
      break;
   case SNC::Error::ConnectFailed:
      printClient("Failed to connect: " + ec.message());
      break;
   case SNC::Error::ResolveFailed:
      printClient("Failed to resolve: " + ec.message());
      break;
   case SNC::Error::ConnectionClosed:
      printClient("Server closed the connection.");
      break;
   case SNC::Error::Aborted:
      //connectionAborted
      break;
   case SNC::Error::WriteFailed:
      std::cerr << "write failed: " << ec.message() << std::endl;
      break;
   case SNC::Error::ReadFailed:
      std::cerr << "Read failed: " << ec.message() << std::endl;
      break;
   case SNC::Error::AbortShutdownFailed:
      std::cerr << "Abort shutdown failed: " << ec.message() << std::endl;
      break;
   case SNC::Error::AbortCloseFailed:
      std::cerr << "Abort close failed: " << ec.message() << std::endl;
      break;
   default:
      break;
   }
}

SNImpl::Server& SN::StreamedNetServer::getImpl() {
   return *serverPtr;
}

void SNC::printClient(std::string&& clientStr, const std::string& ip, ushort_16 port, bool wPort) {
   Colorb::SKY_BLUE.printAnsiStyle();
   if (wPort) {
      std::cout << "[Client: " << ip << ":" << port << "]: ";
   } else
      std::cout << "[Client]: ";
   resetAnsiStyle();
   std::cout << clientStr << "\n";
}


/*CONNECTION*/
StreamedNetConnection::StreamedNetConnection(asio::io_context& context, StreamedNetServer& serverRef, tcp::socket& accepted) :
   context_(context), socket(std::move(accepted)), readBuffer(20 * 1024), writeBuffer(20 * 1024), server(serverRef) {
   AddFlag(state, State::Online);
   onConnect();
}

void StreamedNetConnection::start() {
   readData();
   onStart();
}

void StreamedNetConnection::disconnect() {
   auto self(shared_from_this());
   asio::post(context_, [this, self]() {
      connectionAbort();
   });
}

void StreamedNetConnection::send(const std::vector<ubyte_8>& msg) {
   auto self(shared_from_this());
   asio::post(context_, [this, msg, self]() {
      std::copy(msg.begin(), msg.end(), writeBuffer.begin());

      auto writeLambda = [this, msg, self](std::error_code ec, std::size_t length) {
         if (ec) {
            connectionAbort();
            if (ec == asio::error::eof || ec == asio::error::connection_reset) {
               onError(Error::ConnectionClosed, ec);
               return;
            } else if (ec == asio::error::operation_aborted) {
               onError(Error::Aborted, ec);
               return;
            } else {
               onError(Error::WriteFailed, ec);
               return;
            }
         }
         onEvent(Event::DataSent);
      };
      if (socket.is_open()) {
         asio::async_write(socket, asio::buffer(writeBuffer.data(), msg.size()), writeLambda);
      } else onError(Error::ConnectionClosed, ec);
   });
}

void StreamedNetConnection::send(const std::string& msg) {
   send(StringUtil::stringToBytes(msg));
}

void StreamedNetConnection::readData() {
   auto self(shared_from_this());
   auto readLambda = [this, self](std::error_code ec, std::size_t length) {
      if (ec) {
         connectionAbort();
         if (ec == asio::error::eof || ec == asio::error::connection_reset) {
            onError(Error::ConnectionClosed, ec);
            return;
         } else if (ec == asio::error::operation_aborted) {
            onError(Error::Aborted, ec);
            return;
         } else {
            onError(Error::ReadFailed, ec);
            return;
         }
      }
      onReceive({ readBuffer.data(), readBuffer.data() + length });
      onEvent(Event::DataReceived);
      readData();
   };

   socket.async_read_some(asio::buffer(readBuffer.data(), readBuffer.size()), readLambda);
}

void StreamedNetConnection::connectionAbort() {
   if (HasFlag(state, State::Online) || socket.is_open()) {
      RemoveFlag(state, State::Online);
      onDisconnect();
      onEvent(Event::Disconnected);
      server.getImpl().removeConnection(shared_from_this());
   }
}

asio::io_context& StreamedNetConnection::getContext() {
   return context_;
}

StreamedNetServer& StreamedNetConnection::getServer() {
   return server;
}

void StreamedNetConnection::onEvent(Event evt) {
   switch (evt) {
   case Event::DataReceived:
      std::cout << "data received\n";
      break;
   case Event::Disconnected:
      std::cout << "disconnectd\n";
      break;
   default:
      break;
   }
}

void StreamedNetConnection::onError(Error err, const asio::error_code& ec) {
   switch (err) {
   case Error::ConnectionClosed:
      std::cerr << "connection closed\n";
      break;
   case Error::Aborted:
      std::cerr << "connection closed\n";
      break;
   case Error::WriteFailed:
      std::cerr << "write failed\n";
      break;
   case Error::ReadFailed:
      std::cerr << "read failed\n";
      break;
   default:
      break;
   }
}

/*SERVER*/
Server::Server(asio::io_context& context, StreamedNetServer& parent) : context_(context), parentRef(&parent) {}

void Server::start(ushort_16 port) {
   if (HasFlag(state, SNS::Online)) {
      if (parentRef) parentRef->onError(SNS::Error::AlreadyStarted, ec);
      return;
   }
   AddFlag(state, SNS::Online);
   if (parentRef) parentRef->joinThread();
   connections.clear();
   port_ = port;

   acceptor.emplace(context_, tcp::endpoint(tcp::v4(), port));
   if (parentRef) {
      parentRef->onStart();
      parentRef->onEvent(SNS::Event::OnStart);
   }
   acceptClients();
   if (parentRef) parentRef->startThread();
}

void Server::close() {
   asio::post(context_, [this]() {
      serverAbort();
   });
   if (parentRef) parentRef->joinThread();
}

void Server::acceptClients() {
   if (!acceptor) return;
   pendingSocket.emplace(context_);

   auto aceptLambda = [&](const asio::error_code& ec) {
      if (ec) {
         if (parentRef) parentRef->onError(SNS::Error::AcceptFailed, ec);
         serverAbort();
         return;
      } else {
         if (!parentRef) {
            serverAbort();
            return;
         }
         std::shared_ptr<StreamedNetConnection> connection = parentRef->onAccept(*pendingSocket);
         connection->start();
         connections.emplace_back(std::move(connection));
      }
      acceptClients();
   };

   acceptor->async_accept(*pendingSocket, aceptLambda);
}

void Server::serverAbort() {
   if (HasFlag(state, SNS::Online)) {
      RemoveFlag(state, SNS::Online);

      ec = acceptor->cancel(ec);
      if (ec && parentRef) parentRef->onError(SNS::Error::AcceptorAbortCancelFailed, ec);

      ec = acceptor->close(ec);
      if (ec && parentRef) parentRef->onError(SNS::Error::AcceptorAbortCloseFailed, ec);

      auto removeLambda = [&](std::shared_ptr<StreamedNetConnection>& connection) {
         tcp::socket& socket = connection->socket;
         if (socket.is_open()) {
            ec = socket.shutdown(tcp::socket::shutdown_both, ec);
            if (ec && parentRef) parentRef->onError(SNS::Error::AbortShutdownFailed, ec);
            ec = socket.close(ec);
            if (ec && parentRef) parentRef->onError(SNS::Error::AbortCloseFailed, ec);
         }
         return true;
         };

      connections.erase(
         std::remove_if(connections.begin(), connections.end(), removeLambda),
         connections.end()
      );
      if (parentRef) {
         parentRef->stopContext();
         parentRef->onAbort();
         parentRef->onEvent(SNS::Event::Aborted);
      }
   }
}

void Server::removeConnection(std::shared_ptr<StreamedNetConnection> connection) {
   auto it = std::find(connections.begin(), connections.end(), connection);
   if (it != connections.end()) {
      std::error_code ec;
      if(parentRef) parentRef->onDisconnect(connection);

      ec = connection->socket.shutdown(tcp::socket::shutdown_both, ec);
      if(ec && parentRef) parentRef->onError(SNS::Error::AbortShutdownFailed, ec);
      ec = connection->socket.close(ec);
      if(ec && parentRef) parentRef->onError(SNS::Error::AbortCloseFailed, ec);

      connections.erase(it);
   }
}

/*SERVER_WRAPPER*/
SNS::StreamedNetServer() : contextPtr_(std::make_unique<asio::io_context>()), context_(contextPtr_.get()),
serverPtr(std::make_shared<Server>(*context_, *this)) {
}

SNS::StreamedNetServer(asio::io_context& context) : contextPtr_(nullptr), context_(&context),
serverPtr(std::make_shared<Server>(*context_, *this)) {
}

SNS::~StreamedNetServer() {
   serverPtr->parentRef = nullptr;
}

void SNS::start(ushort_16 port) {
   serverPtr->start(port);
}

void SNS::close() {
   serverPtr->close();
}

void SNS::startThread() {
   if (!contextPtr_ || !context_) return;

   thr = std::thread([this]() {
      context_->reset();
      context_->run();
   });
}

void SNS::joinThread() {
   if (!contextPtr_ || !context_) return;

   if (thr.joinable()) {
      context_->stop();
      thr.join();
   }
}

void SNS::stopContext() {
   if (!contextPtr_ || !context_) return;

   context_->stop();
   context_->reset();
}

asio::io_context& SNS::getContext() {
   return serverPtr->context_;
}

ushort_16 SNS::getPort() {
   return serverPtr->port_;
}

std::vector<std::shared_ptr<SN::StreamedNetConnection>> SN::StreamedNetServer::getConnections() {
   std::vector<std::shared_ptr<SN::StreamedNetConnection>> copy = serverPtr->connections;
   return copy;
}

std::shared_ptr<StreamedNetConnection> SNS::onAccept(tcp::socket& socket) {
   return std::make_shared<StreamedNetConnection>(*context_, *this, socket);
}

void SNS::onError(Error err, const asio::error_code& ec) {
   switch (err) {
   case Error::AlreadyStarted:
      std::cout << "already oppen\n";
      break;
   case Error::AcceptFailed:
      printServer("Failed to accept clinet: " + ec.message());
      break;
   case Error::AbortShutdownFailed:
      std::cerr << "Abort shutdown failed: " << ec.message() << std::endl;
      break;
   case Error::AbortCloseFailed:
      std::cerr << "Abort close failed: " << ec.message() << std::endl;
      break;
   case Error::AcceptorAbortCancelFailed:
      std::cerr << "Abort acceptor failed: " << ec.message() << std::endl;
      break;
   case Error::AcceptorAbortCloseFailed:
      std::cerr << "Abort close failed: " << ec.message() << std::endl;
      break;
   default:
      break;
   }
}

void SNS::printServer(std::string&& serverStr, ushort_16 port, bool wPort) {
   Colorb::BRONZE.printAnsiStyle();
   if (wPort) {
      std::cout << "[Server: " << port << "]: ";
   } else
      std::cout << "[Server]: ";
   resetAnsiStyle();
   std::cout << serverStr << "\n";
}
