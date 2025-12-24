#include "StreamedNet.h"
#include <iostream>

/*---------------------------NET_STREAM---------------------------*/
SN::NetStream::NetStream(SN::IOContextController context_, tcp::socket& socket_) :
   context(std::move(context_)), socket(std::move(socket_)), readBuffer(20 * 1024) {
   doTick();
}

SN::NetStream::NetStream(SN::IOContextController context_) :
   context(std::move(context_)), socket(*context), readBuffer(20 * 1024) {
   doTick();
}

void SN::NetStream::startRead() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;

   if(HasFlag(state, SNI_IN_READ)) return;

   if(!oWLock.try_acquire()) return;
   asio::post(*context, [this]() {
      SN::OWLockRelease release(oWLock);

      doRead();
   });
}

void SN::NetStream::startWrite() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;

   if(HasFlag(state, SNI_IN_WRITE)) return;

   if(!oWLock.try_acquire()) return;
   asio::post(*context, [this]() {
      SN::OWLockRelease release(oWLock);
      doWrite();
   });
}

void SN::NetStream::send(const std::vector<uint8_t> msg) {
   writeQ.push(msg);
   startWrite();
}

void SN::NetStream::stopRead() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;
   if(HasNoFlag(state, SNI_IN_READ)) return;
   AddFlag(state, SNI_STOP_READ_R);
}

void SN::NetStream::stopWrite() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;
   if(HasNoFlag(state, SNI_IN_WRITE)) return;
   AddFlag(state, SNI_STOP_WRITE_R);
}

void SN::NetStream::disconnect() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;

   if(!oWLock.try_acquire()) return;
   asio::post(*context, [this]() {
      SN::OWLockRelease release(oWLock);

      abort();
   });
}

void SN::NetStream::doRead() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;

   if(!oWLock.try_acquire()) return;
   auto readLambda = [this](std::error_code ec, std::size_t length) {
      SN::OWLockRelease release(oWLock);

      if(ec) {
         std::cout << "writeError: " << ec.message() << "\n";
         RemoveFlag(state, SNI_IN_READ);
         RemoveFlag(state, SNI_STOP_READ_R);

         abort();
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

   socket.async_read_some(asio::buffer(readBuffer.data(), readBuffer.size()), readLambda);
}

void SN::NetStream::doWrite() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;

   if(writeQ.empty()) {
      RemoveFlag(state, SNI_IN_WRITE);
      RemoveFlag(state, SNI_STOP_WRITE_R);
      return;
   }

   if(!oWLock.try_acquire()) return;
   auto writeLambda = [this](std::error_code ec, std::size_t length) {
      SN::OWLockRelease release(oWLock);

      if(ec) {
         std::cout << "writeError: " << ec.message() << "\n";
         RemoveFlag(state, SNI_IN_WRITE);
         RemoveFlag(state, SNI_STOP_WRITE_R);

         abort();
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

   asio::async_write(socket, asio::buffer(writeQ.front().data(), writeQ.front().size()), writeLambda);
}

void SN::NetStream::doTick() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;

   if(!oWLock.try_acquire()) return;
   asio::post(*context, [this]() {
      SN::OWLockRelease release(oWLock);

      doTick();
      onTick();
   });
}

void SN::NetStream::abort() {
   printf("NetStreamAbort\n");
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;
   
   if(HasNoFlag(state, SNI_ONLINE) && HasNoFlag(state, SNI_RESOLVEING) && HasNoFlag(state, SNI_CONNECTING) && !socket.is_open()) return;
   RemoveFlag(state, SNI_ONLINE);
   RemoveFlag(state, SNI_RESOLVEING);
   RemoveFlag(state, SNI_CONNECTING);

   if(socket.is_open()) {
      ec = socket.shutdown(tcp::socket::shutdown_both, ec);
      if (ec) onError(Error::AbortShutdownFailed, ec);
      ec = socket.close(ec);
      if (ec) onError(Error::AbortCloseFailed, ec);
   }

   onDisconnect();
   onEvent(Event::Disconnected);
}

SN::NetStream::~NetStream() {
   oWLock.begin_destroy_and_wait();
   printf("destroyed");
}

void SN::NetStream::onEvent(Event evt) {
   switch (evt) {
      case Event::OnStart:
         std::cout << "EVE OnStart\n";
         break;
      case Event::Aborted:
         std::cout << "EVE Abort\n";
         break;
      case Event::Connected:
         std::cout << "EVE Connected\n";
         break;
      case Event::Resolved:
         std::cout << "EVE Resolved\n";
         break;
      case Event::DataSent:
         std::cout << "EVE DataSent\n";
         break;
      case Event::DataReceived:
         std::cout << "EVE DataReceived\n";
         break;
      case Event::Disconnected:
         std::cout << "EVE Disconnected\n";
         break;
      default:
         break;
   }
}

void SN::NetStream::onError(Error err, const asio::error_code& ec) {
   switch (err) {
      case Error::AlreadyStarted:
         std::cout << "ERR AlreadyStarted" << ec.message() << "\n";
         break;
      case Error::AlreadyResolved:
         std::cout << "ERR AlreadyResolved" << ec.message() << "\n";
         break;
      case Error::AlreadyConnected:
         std::cout << "ERR AlreadyConnected" << ec.message() << "\n";
         break;
      case Error::ConnectFailed:
         std::cout << "ERR ConnectFailed" << ec.message() << "\n";
         break;
      case Error::ResolveFailed:
         std::cout << "ERR ResolveFailed" << ec.message() << "\n";
         break;
      case Error::AcceptFailed:
         std::cout << "ERR AcceptFailed" << ec.message() << "\n";
         break;
      case Error::ConnectionClosed:
         std::cout << "ERR ConnectionClosed" << ec.message() << "\n";
         break;
      case Error::Aborted:
         std::cout << "ERR Aborted" << ec.message() << "\n";
         break;
      case Error::WriteFailed:
         std::cout << "ERR WriteFailed" << ec.message() << "\n";
         break;
      case Error::ReadFailed:
         std::cout << "ERR ReadFailed" << ec.message() << "\n";
         break;
      case Error::AbortShutdownFailed:
         std::cout << "ERR AbortShutdownFailed" << ec.message() << "\n";
         break;
      case Error::AbortCloseFailed:
         std::cout << "ERR AbortCloseFailed" << ec.message() << "\n";
         break;
      case Error::AcceptorAbortCancelFailed:
         std::cout << "ERR AcceptorAbortCancelFailed" << ec.message() << "\n";
         break;
      case Error::AcceptorAbortCloseFailed:
         std::cout << "ERR AcceptorAbortCloseFailed" << ec.message() << "\n";
         break;
      default:
         break;
   }
}



/*---------------------------CLIENT---------------------------*/
SN::Client::Client() : NetStream(SN::IOContextController()), resolver(*context) {}

SN::Client::Client(SN::IOContextController context_) : NetStream(std::move(context_)), resolver(*context) {}

void SN::Client::resolve(const std::string& host, uint16_t port) {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;

   if(HasFlag(state, SNI_ONLINE) || HasFlag(state, SNI_RESOLVEING) || HasFlag(state, SNI_CONNECTING)) return;

   if(!oWLock.try_acquire()) return;
   auto resolveLambda = [this](const std::error_code& ec, tcp::resolver::results_type resultEndpoints) {
      SN::OWLockRelease release(oWLock);

      RemoveFlag(state, SNI_RESOLVEING);
      if(ec) {
         std::cout << "resolve failed: " << ec.message() << "\n";
      } else {
         endpoints.clear();
         for (auto it = resultEndpoints.begin(); it != resultEndpoints.end(); ++it)
            endpoints.push_back(it->endpoint());
         onResolve();//TODO: potential guard pass needed
      }
   };

   AddFlag(state, SNI_RESOLVEING);
   resolver.async_resolve(host, std::to_string(port), resolveLambda);
}

void SN::Client::addEndpoint(const std::string& host, uint16_t port) {
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

void SN::Client::connect() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;

   if(HasFlag(state, SNI_ONLINE) || HasFlag(state, SNI_CONNECTING)) return;
   if(endpoints.empty()) return;

   if(!oWLock.try_acquire()) return;
   auto connectLambda = [this](const std::error_code& ec, const tcp::endpoint& connectedEndpoint) {
      SN::OWLockRelease release(oWLock);

      RemoveFlag(state, SNI_CONNECTING);

      if(ec) {
         std::cout << "Connection failed\n";
         return;
      } else {
         AddFlag(state, SNI_ONLINE);
         onConnect(); //TODO: Potential guard pass needed
      }
   };

   AddFlag(state, SNI_CONNECTING);
   asio::async_connect(socket, endpoints, connectLambda);
}

void SN::Client::disconnect() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;

   if(!oWLock.try_acquire()) return;
   asio::post(*context, [this]() {
      SN::OWLockRelease release(oWLock);

      abort();
   });
}

void SN::Client::abort() {
   printf("ClientAbort\n");
   NetStream::abort();
}

void SN::Client::onEvent(Event evt) {
   switch (evt) {
      case Event::OnStart:
         std::cout << "EVE OnStart\n";
         break;
      case Event::Aborted:
         std::cout << "EVE Abort\n";
         break;
      case Event::Connected:
         std::cout << "EVE Connected\n";
         break;
      case Event::Resolved:
         std::cout << "EVE Resolved\n";
         break;
      case Event::DataSent:
         std::cout << "EVE DataSent\n";
         break;
      case Event::DataReceived:
         std::cout << "EVE DataReceived\n";
         break;
      case Event::Disconnected:
         std::cout << "EVE Disconnected\n";
         break;
      default:
         break;
   }
}

void SN::Client::onError(Error err, const asio::error_code& ec) {
   switch (err) {
      case Error::AlreadyStarted:
         std::cout << "ERR AlreadyStarted" << ec.message() << "\n";
         break;
      case Error::AlreadyResolved:
         std::cout << "ERR AlreadyResolved" << ec.message() << "\n";
         break;
      case Error::AlreadyConnected:
         std::cout << "ERR AlreadyConnected" << ec.message() << "\n";
         break;
      case Error::ConnectFailed:
         std::cout << "ERR ConnectFailed" << ec.message() << "\n";
         break;
      case Error::ResolveFailed:
         std::cout << "ERR ResolveFailed" << ec.message() << "\n";
         break;
      case Error::AcceptFailed:
         std::cout << "ERR AcceptFailed" << ec.message() << "\n";
         break;
      case Error::ConnectionClosed:
         std::cout << "ERR ConnectionClosed" << ec.message() << "\n";
         break;
      case Error::Aborted:
         std::cout << "ERR Aborted" << ec.message() << "\n";
         break;
      case Error::WriteFailed:
         std::cout << "ERR WriteFailed" << ec.message() << "\n";
         break;
      case Error::ReadFailed:
         std::cout << "ERR ReadFailed" << ec.message() << "\n";
         break;
      case Error::AbortShutdownFailed:
         std::cout << "ERR AbortShutdownFailed" << ec.message() << "\n";
         break;
      case Error::AbortCloseFailed:
         std::cout << "ERR AbortCloseFailed" << ec.message() << "\n";
         break;
      case Error::AcceptorAbortCancelFailed:
         std::cout << "ERR AcceptorAbortCancelFailed" << ec.message() << "\n";
         break;
      case Error::AcceptorAbortCloseFailed:
         std::cout << "ERR AcceptorAbortCloseFailed" << ec.message() << "\n";
         break;
      default:
         break;
   }
}

void SN::Client::printClient(std::string&& clientStr, const std::string& ip, uint16_t port, bool wPort) {
   std::cout << "\033[34m";
   if (wPort) {
      std::cout << "[Client: " << ip << ":" << port << "]: ";
   } else
      std::cout << "[Client]: ";
   std::cout << "\033[0m";
   std::cout << clientStr << "\n";
}



/*---------------------------CONNECTION---------------------------*/
SN::Connection::Connection(SN::IOContextController context, Server& serverRef, tcp::socket& accepted) : NetStream(std::move(context), accepted), server(serverRef) {
   AddFlag(state, SNI_ONLINE);
   onConnect();
}

void SN::Connection::start() {
   onStart();
}

void SN::Connection::disconnect() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;

   if(!oWLock.try_acquire()) return;
   asio::post(*context, [this]() {
      SN::OWLockRelease release(oWLock);

      abort();
   });
}

SN::Server& SN::Connection::getServer() {
   return server;
}

void SN::Connection::abort() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;

   NetStream::abort();
   server.removeConnection(this);
}

void SN::Connection::onEvent(Event evt) {
   switch (evt) {
      case Event::OnStart:
         std::cout << "EVE OnStart\n";
         break;
      case Event::Aborted:
         std::cout << "EVE Abort\n";
         break;
      case Event::Connected:
         std::cout << "EVE Connected\n";
         break;
      case Event::Resolved:
         std::cout << "EVE Resolved\n";
         break;
      case Event::DataSent:
         std::cout << "EVE DataSent\n";
         break;
      case Event::DataReceived:
         std::cout << "EVE DataReceived\n";
         break;
      case Event::Disconnected:
         std::cout << "EVE Disconnected\n";
         break;
      default:
         break;
   }
}

void SN::Connection::onError(Error err, const asio::error_code& ec) {
   switch (err) {
      case Error::AlreadyStarted:
         std::cout << "ERR AlreadyStarted" << ec.message() << "\n";
         break;
      case Error::AlreadyResolved:
         std::cout << "ERR AlreadyResolved" << ec.message() << "\n";
         break;
      case Error::AlreadyConnected:
         std::cout << "ERR AlreadyConnected" << ec.message() << "\n";
         break;
      case Error::ConnectFailed:
         std::cout << "ERR ConnectFailed" << ec.message() << "\n";
         break;
      case Error::ResolveFailed:
         std::cout << "ERR ResolveFailed" << ec.message() << "\n";
         break;
      case Error::AcceptFailed:
         std::cout << "ERR AcceptFailed" << ec.message() << "\n";
         break;
      case Error::ConnectionClosed:
         std::cout << "ERR ConnectionClosed" << ec.message() << "\n";
         break;
      case Error::Aborted:
         std::cout << "ERR Aborted" << ec.message() << "\n";
         break;
      case Error::WriteFailed:
         std::cout << "ERR WriteFailed" << ec.message() << "\n";
         break;
      case Error::ReadFailed:
         std::cout << "ERR ReadFailed" << ec.message() << "\n";
         break;
      case Error::AbortShutdownFailed:
         std::cout << "ERR AbortShutdownFailed" << ec.message() << "\n";
         break;
      case Error::AbortCloseFailed:
         std::cout << "ERR AbortCloseFailed" << ec.message() << "\n";
         break;
      case Error::AcceptorAbortCancelFailed:
         std::cout << "ERR AcceptorAbortCancelFailed" << ec.message() << "\n";
         break;
      case Error::AcceptorAbortCloseFailed:
         std::cout << "ERR AcceptorAbortCloseFailed" << ec.message() << "\n";
         break;
      default:
         break;
   }
}


/*---------------------------SERVER---------------------------*/
SN::Server::Server() : context(SN::IOContextController()) {
   doTick();
}

SN::Server::Server(SN::IOContextController context_) : context(std::move(context_)) {
   doTick();
}

void SN::Server::start(uint16_t port_) {
   if(HasFlag(state, SNI_ONLINE)) {
      return;
   }

   AddFlag(state, SNI_ONLINE);
   port = port_;

   acceptor.emplace(*context, tcp::endpoint(tcp::v4(), port));

   onStart();
}

void SN::Server::startAccept() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;
   if(HasFlag(state, SNI_IN_ACCEPT)) return;

   if(!oWLock.try_acquire()) return;
   asio::post(*context, [this]() {
      SN::OWLockRelease release(oWLock);

      doAccept();
   });
}

void SN::Server::stopAccept() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;
   if(HasNoFlag(state, SNI_IN_ACCEPT)) return;
   AddFlag(state, SNI_STOP_ACCEPT_R);
}

void SN::Server::close() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;

   if(!oWLock.try_acquire()) return;
   asio::post(*context, [this]() {
      SN::OWLockRelease release(oWLock);

      abort();
   });
}

void SN::Server::doAccept() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;

   if(!acceptor) {
      RemoveFlag(state, SNI_OFFLINE);
      RemoveFlag(state, SNI_STOP_ACCEPT_R);
      return;
   }

   pendingSocket.emplace(*context);

   if(!oWLock.try_acquire()) return;
   auto acceptLambda = [&](const asio::error_code& errorCode) {
      SN::OWLockRelease release(oWLock);

      ec = errorCode;
      if(ec) {
         abort();
         return;
      }

      std::shared_ptr<Connection> connection = onAccept(*pendingSocket);
      connection->start();
      connections.emplace_back(std::move(connection));
      doAccept();
   };

   acceptor->async_accept(*pendingSocket, acceptLambda);
}

void SN::Server::doTick() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;

   if(!oWLock.try_acquire()) return;
   asio::post(*context, [this]() {
      SN::OWLockRelease release(oWLock);

      doTick();
      onTick();
   });
}

void SN::Server::removeConnection(Connection* connectionPtr) {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;

   if(!oWLock.try_acquire()) return;
   auto removeLambda = [this, connectionPtr](const std::shared_ptr<Connection>& conn) -> bool {
      SN::OWLockRelease release(oWLock);

      return conn.get() == connectionPtr;
   };

   connections.erase(std::remove_if(connections.begin(), connections.end(), removeLambda), connections.end());
}

asio::io_context& SN::Server::getContext() {
   return *context;
}

uint16_t SN::Server::getPort() {
   return port;
}

std::vector<std::shared_ptr<SN::Connection>>& SN::Server::getConnections() {
   return connections;
}

void SN::Server::abort() {
   SN::OWLockGuard guard(oWLock);
   if(!guard) return;

   if(HasNoFlag(state, SNI_ONLINE) && !acceptor->is_open()) return;
   RemoveFlag(state, SNI_ONLINE);

   ec = acceptor->cancel(ec);
   if (ec) onError(Error::AcceptorAbortCancelFailed, ec);

   ec = acceptor->close(ec);
   if (ec) onError(Error::AcceptorAbortCloseFailed, ec);

   for(auto& connection : getConnections()) {
      connection->disconnect();
   }

   connections.clear();
   onEvent(Event::Aborted);
}

std::shared_ptr<SN::Connection> SN::Server::onAccept(tcp::socket& socket) {
   return std::make_shared<Connection>(context.ptr(), *this, socket);
}

SN::Server::~Server() {
   close();
   oWLock.begin_destroy_and_wait();
}

void SN::Server::onEvent(Event evt) {
   switch (evt) {
      case Event::OnStart:
         std::cout << "EVE OnStart\n";
         break;
      case Event::Aborted:
         std::cout << "EVE Abort\n";
         break;
      case Event::Connected:
         std::cout << "EVE Connected\n";
         break;
      case Event::Resolved:
         std::cout << "EVE Resolved\n";
         break;
      case Event::DataSent:
         std::cout << "EVE DataSent\n";
         break;
      case Event::DataReceived:
         std::cout << "EVE DataReceived\n";
         break;
      case Event::Disconnected:
         std::cout << "EVE Disconnected\n";
         break;
      default:
         break;
   }
}

void SN::Server::onError(Error err, const asio::error_code& ec) {
   switch (err) {
      case Error::AlreadyStarted:
         std::cout << "ERR AlreadyStarted" << ec.message() << "\n";
         break;
      case Error::AlreadyResolved:
         std::cout << "ERR AlreadyResolved" << ec.message() << "\n";
         break;
      case Error::AlreadyConnected:
         std::cout << "ERR AlreadyConnected" << ec.message() << "\n";
         break;
      case Error::ConnectFailed:
         std::cout << "ERR ConnectFailed" << ec.message() << "\n";
         break;
      case Error::ResolveFailed:
         std::cout << "ERR ResolveFailed" << ec.message() << "\n";
         break;
      case Error::AcceptFailed:
         std::cout << "ERR AcceptFailed" << ec.message() << "\n";
         break;
      case Error::ConnectionClosed:
         std::cout << "ERR ConnectionClosed" << ec.message() << "\n";
         break;
      case Error::Aborted:
         std::cout << "ERR Aborted" << ec.message() << "\n";
         break;
      case Error::WriteFailed:
         std::cout << "ERR WriteFailed" << ec.message() << "\n";
         break;
      case Error::ReadFailed:
         std::cout << "ERR ReadFailed" << ec.message() << "\n";
         break;
      case Error::AbortShutdownFailed:
         std::cout << "ERR AbortShutdownFailed" << ec.message() << "\n";
         break;
      case Error::AbortCloseFailed:
         std::cout << "ERR AbortCloseFailed" << ec.message() << "\n";
         break;
      case Error::AcceptorAbortCancelFailed:
         std::cout << "ERR AcceptorAbortCancelFailed" << ec.message() << "\n";
         break;
      case Error::AcceptorAbortCloseFailed:
         std::cout << "ERR AcceptorAbortCloseFailed" << ec.message() << "\n";
         break;
      default:
         break;
   }
}

void SN::Server::printServer(std::string&& serverStr, uint16_t port, bool wPort) {
   std::cout << "\033[38;2;205;127;50m";
   if (wPort) {
      std::cout << "[Server: " << port << "]: ";
   } else std::cout << "[Server]: ";
   std::cout << "\033[0m";
   std::cout << serverStr << "\n";
}