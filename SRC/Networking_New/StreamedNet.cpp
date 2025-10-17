#include "StreamedNet.h"
#include <iostream>

thread_local unsigned int SN::OWLock::tl_recursion_count = 0;

/*---------------------------NET_STREAM---------------------------*/
SNImpl::NetStream::NetStream(std::shared_ptr<asio::io_context> context, tcp::socket& socket) :
   context_(context), socket_(std::move(socket)), readBuffer(20 * 1024) {
   doTick();
}

SNImpl::NetStream::NetStream(std::shared_ptr<asio::io_context> context) :
   context_(context), socket_(*context_), readBuffer(20 * 1024) {
   doTick();
}

void SNImpl::NetStream::startRead() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;
   if(HasFlag(state, SNI_IN_READ)) return;

   asio::post(*context_, [this]() {
      SN::OWLockGuard guard(oWLock);
      if(!guard) return;

      doRead();
   });
}

void SNImpl::NetStream::startWrite() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;
   if(HasFlag(state, SNI_IN_WRITE)) return;

   asio::post(*context_, [this]() {
      SN::OWLockGuard guard(oWLock);
      if(!guard) return;

      doWrite();
   });
}

void SNImpl::NetStream::send(const std::vector<ubyte_8> msg) {
   writeQ.push(msg);
   startWrite();
}

void SNImpl::NetStream::stopRead() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;
   if(HasNoFlag(state, SNI_IN_READ)) return;
   AddFlag(state, SNI_STOP_READ_R);
}

void SNImpl::NetStream::stopWrite() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;
   if(HasNoFlag(state, SNI_IN_WRITE)) return;
   AddFlag(state, SNI_STOP_WRITE_R);
}

void SNImpl::NetStream::disconnect() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;

   asio::post(*context_, [this]() {
      SN::OWLockGuard guard(oWLock);
      if(!guard) return;
      abortConnection();
   });
}

void SNImpl::NetStream::doRead() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;

   auto readLambda = [this](std::error_code ec, std::size_t length) {
      SN::OWLockGuard guard(oWLock);
      if(!guard) return;

      if(ec) {
         std::cout << "writeError: " << ec.message() << "\n";
         RemoveFlag(state, SNI_IN_READ);
         RemoveFlag(state, SNI_STOP_READ_R);

         abortConnection();
         return;
      }

      for (int i = 0; i < length; ++i) readQ.push(readBuffer[i]);
      onRead();

      if(HasFlag(state, SNI_STOP_READ_R)) {
         RemoveFlag(state, SNI_IN_READ);
         RemoveFlag(state, SNI_STOP_READ_R);
         return;
      }

      doRead();
   };

   socket_.async_read_some(asio::buffer(readBuffer.data(), readBuffer.size()), readLambda);
}

void SNImpl::NetStream::doWrite() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;

   if(writeQ.empty()) {
      RemoveFlag(state, SNI_IN_WRITE);
      RemoveFlag(state, SNI_STOP_WRITE_R);
      return;
   }

   auto writeLambda = [this](std::error_code ec, std::size_t length) {
      SN::OWLockGuard guard(oWLock);
      if(!guard) return;

      if(ec) {
         std::cout << "writeError: " << ec.message() << "\n";
         RemoveFlag(state, SNI_IN_WRITE);
         RemoveFlag(state, SNI_STOP_WRITE_R);

         abortConnection();
         return;
      }
      onWrite();

      writeQ.pop();
      if (writeQ.empty() || HasFlag(state, SNI_STOP_WRITE_R)) {
         RemoveFlag(state, SNI_IN_WRITE);
         RemoveFlag(state, SNI_STOP_WRITE_R);
         return;
      }

      doWrite();
   };

   asio::async_write(socket_, asio::buffer(writeQ.front().data(), writeQ.front().size()), writeLambda);
}

void SNImpl::NetStream::doTick() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;

   asio::post(*context_, [this]() {
      SN::OWLockGuard guard(oWLock);
      if(!guard) return;

      doTick();
      onTick();
   });
}

void SNImpl::NetStream::abortConnection() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;
   
   if(HasNoFlag(state, SNI_ONLINE) && HasNoFlag(state, SNI_RESOLVEING) && HasNoFlag(state, SNI_CONNECTING) && !socket_.is_open()) return;
   RemoveFlag(state, SNI_ONLINE);
   RemoveFlag(state, SNI_RESOLVEING);
   RemoveFlag(state, SNI_CONNECTING);

   if(socket_.is_open()) {
      ec = socket_.shutdown(tcp::socket::shutdown_both, ec);
      //if (ec) onError(SNC::Error::AbortShutdownFailed, ec);
      ec = socket_.close(ec);
      //if (ec) onError(SNC::Error::AbortCloseFailed, ec);
   }

   /*if (parentRef) {
      parentRef->stopContext();
      parentRef->onDisconnect();
      parentRef->onEvent(SNC::Event::Disconnected);
   }*/
}

SNImpl::NetStream::~NetStream() {
   oWLock.begin_destroy_and_wait();
}



/*---------------------------CLIENT---------------------------*/
SNImpl::Client::Client(std::shared_ptr<asio::io_context> context) : NetStream(context), resolver(*context_) {}


void SNImpl::Client::resolve(const std::string& host, ushort_16 port) {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;

   if(HasFlag(state, SNI_ONLINE) || HasFlag(state, SNI_RESOLVEING) || HasFlag(state, SNI_CONNECTING)) return;

   auto resolveLambda = [this](const std::error_code& ec, tcp::resolver::results_type resultEndpoints) {
      SN::OWLockGuard guard(oWLock);
      if(!guard) return;

      RemoveFlag(state, SNI_RESOLVEING);
      if(ec) {
         std::cout << "resolve failed: " << ec.message() << "\n";
      } else {
         endpoints.clear();
         for (auto it = resultEndpoints.begin(); it != resultEndpoints.end(); ++it)
            endpoints.push_back(it->endpoint());
         onResolve();
      }
   };

   AddFlag(state, SNI_RESOLVEING);
   resolver.async_resolve(host, std::to_string(port), resolveLambda);
}

void SNImpl::Client::addEndpoint(const std::string& host, ushort_16 port) {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;

   asio::error_code ec;
   auto addr = asio::ip::make_address(host, ec);
   if (ec) {
      std::cout << "Invalid address (" << host << "): " << ec.message() << "\n";
      return;
   }
   endpoints.push_back(tcp::endpoint(addr, port));
}

void SNImpl::Client::connect() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;

   if(HasFlag(state, SNI_ONLINE) || HasFlag(state, SNI_CONNECTING)) return;
   if(endpoints.empty()) return;

   auto connectLambda = [this](const std::error_code& ec, const tcp::endpoint& connectedEndpoint) {
      SN::OWLockGuard guard(oWLock);
      if(!guard) return;
      
      RemoveFlag(state, SNI_CONNECTING);
      if(ec) {
         std::cout << "Connection failed\n";
         return;
      } else {
         AddFlag(state, SNI_ONLINE);
         onConnect();
      }
   };

   AddFlag(state, SNI_CONNECTING);
   asio::async_connect(socket_, endpoints, connectLambda);
}

void SNImpl::Client::printClient(std::string&& clientStr, const std::string& ip, ushort_16 port, bool wPort) {
   std::cout << "\033[34m";
   if (wPort) {
      std::cout << "[Client: " << ip << ":" << port << "]: ";
   } else
      std::cout << "[Client]: ";
   std::cout << "\033[0m";
   std::cout << clientStr << "\n";
}



/*---------------------------CONNECTION---------------------------*/
SNImpl::Connection::Connection(std::shared_ptr<asio::io_context> context, Server& serverRef, tcp::socket& accepted) : NetStream(context, accepted), server(serverRef) {
   AddFlag(state, SNI_ONLINE);
   onConnect();
}

void SNImpl::Connection::start() {
   onStart();
}

SNImpl::Server& SNImpl::Connection::getServer() {
   return server;
}


/*---------------------------SERVER---------------------------*/
SNImpl::Server::Server(std::shared_ptr<asio::io_context> context) : context_(context) {
   doTick();
}

void SNImpl::Server::start(ushort_16 port) {
   if(HasFlag(state, SNI_ONLINE)) {
      return;
   }

   AddFlag(state, SNI_ONLINE);
   port_ = port;

   acceptor.emplace(*context_, tcp::endpoint(tcp::v4(), port));

   onStart();
}

void SNImpl::Server::startAccept() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;
   if(HasFlag(state, SNI_IN_ACCEPT)) return;

   asio::post(*context_, [this]() {
      SN::OWLockGuard guard(oWLock);
      if(!guard) return;

      doAccept();
   });
}

void SNImpl::Server::stopAccept() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;
   if(HasNoFlag(state, SNI_IN_ACCEPT)) return;
   AddFlag(state, SNI_STOP_ACCEPT_R);
}

void SNImpl::Server::close() {
}


void SNImpl::Server::doAccept() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;

   if(!acceptor) {
      RemoveFlag(state, SNI_OFFLINE);
      RemoveFlag(state, SNI_STOP_ACCEPT_R);
      return;
   }

   pendingSocket.emplace(*context_);

   auto acceptLambda = [&](const asio::error_code& errorCode) {
      SN::OWLockGuard guard(oWLock);
      if(!guard) return;
      
      ec = errorCode;
      if(ec) {
         serverAbort();
         return;
      } else {
         std::shared_ptr<Connection> connection = onAccept(*pendingSocket);
         connection->start();
         connections.emplace_back(std::move(connection));
      }
   };

   acceptor->async_accept(*pendingSocket, acceptLambda);
}

void SNImpl::Server::doTick() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;

   asio::post(*context_, [this]() {
      SN::OWLockGuard guard(oWLock);
      if(!guard) return;

      doTick();
      onTick();
   });
}

void SNImpl::Server::serverAbort() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;
}

std::shared_ptr<asio::io_context> SNImpl::Server::getContext() {
   return context_;
}

SNImpl::ushort_16 SNImpl::Server::getPort() {
   return port_;
}

std::vector<std::shared_ptr<SNImpl::Connection>>& SNImpl::Server::getConnections() {
   return connections;
}

std::shared_ptr<SNImpl::Connection> SNImpl::Server::onAccept(tcp::socket& socket) {
   return std::make_shared<Connection>(context_, *this, socket);
}

SNImpl::Server::~Server() {
   oWLock.begin_destroy_and_wait();
}

void SNImpl::Server::printServer(std::string&& serverStr, ushort_16 port, bool wPort) {
   std::cout << "\033[38;2;205;127;50m";
   if (wPort) {
      std::cout << "[Server: " << port << "]: ";
   } else std::cout << "[Server]: ";
   std::cout << "\033[0m";
   std::cout << serverStr << "\n";
}