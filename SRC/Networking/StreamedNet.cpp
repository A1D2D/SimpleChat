#include "StreamedNet.h"
#include <iostream>
#include <memory>
#include <utility>

/*---------------------------NET_STREAM_ASIO_WRAPPER---------------------------*/
SN::NetStreamAsioW::NetStreamAsioW(SN::IOContextHandle&& context_, SN::NetStream* parent_, tcp::socket&& socket_) : context(std::move(context_)), socket(std::move(socket_)), parent(parent_) {
}

SN::NetStreamAsioW::NetStreamAsioW(SN::IOContextHandle&& context_, SN::NetStream* parent_) : context(std::move(context_)), socket(*context), parent(parent_) {
}

void SN::NetStreamAsioW::startRead() {
   auto lifeTGuard(shared_from_this());
   std::lock_guard lock(guardMutex);
   if(!parent) return;

   if(HasFlag(parent->state, SNI_IN_READ)) return;

   asio::post(*context, [this, lifeTGuard]() {
      doRead();
   });
}

void SN::NetStreamAsioW::startWrite() {
   auto lifeTGuard(shared_from_this());
   std::lock_guard lock(guardMutex);
   if(!parent) return;
   
   if(HasFlag(parent->state, SNI_IN_WRITE)) return;

   asio::post(*context, [this, lifeTGuard]() {
      doWrite();
   });
}

void SN::NetStreamAsioW::abortHalt() {
   NCore_Log("actual abortHalt called\n")
   std::lock_guard lock(guardMutex);
   if(parent) {
      if(HasNoFlag(parent->state, SNI_ONLINE) && HasNoFlag(parent->state, SNI_RESOLVEING) && HasNoFlag(parent->state, SNI_CONNECTING) && !socket.is_open()) return;
      RemoveFlag(parent->state, SNI_ONLINE);
      RemoveFlag(parent->state, SNI_RESOLVEING);
      RemoveFlag(parent->state, SNI_CONNECTING);
   }

   if(socket.is_open()) {
      ec = socket.shutdown(tcp::socket::shutdown_both, ec);
      if (ec && parent) parent->onError(Error::AbortShutdownFailed, ec);
      ec = socket.close(ec);
      if (ec && parent) parent->onError(Error::AbortCloseFailed, ec);
   }

   if(!parent) return;
   parent->onDisconnect();
   parent->onEvent(Event::Disconnected);
}

void SN::NetStreamAsioW::doTick() {
   auto lifeTGuard(shared_from_this());

   asio::post(*context, [this, lifeTGuard](){
      std::lock_guard lock(guardMutex);
      if(!parent) {
         return;
      }

      parent->onTick();
      doTick();
   });
}

void SN::NetStreamAsioW::doRead() {
   NCore_Log("read\n")
   auto lifeTGuard(shared_from_this());

   auto readLambda = [this, lifeTGuard](std::error_code ec, std::size_t length) {
      std::lock_guard lock(guardMutex);
      if(!parent) return;

      if(ec) {
         NCore_Log("ReadFailed: " << ec.message() << "\n")
         parent->onError(Error::ReadFailed, ec);
         RemoveFlag(parent->state, SNI_IN_READ);
         RemoveFlag(parent->state, SNI_STOP_READ_R);

         parent->abortHalt();
         return;
      }

      for (int i = 0; i < length; ++i) parent->readQ.push(parent->readBuffer[i]);
      parent->onRead();

      if(HasFlag(parent->state, SNI_STOP_READ_R)) {
         RemoveFlag(parent->state, SNI_IN_READ);
         RemoveFlag(parent->state, SNI_STOP_READ_R);
         return;
      }

      doRead();
   };

   socket.async_read_some(asio::buffer(parent->readBuffer.data(), parent->readBuffer.size()), readLambda);
}

void SN::NetStreamAsioW::doWrite() {
   NCore_Log("write\n")
   auto lifeTGuard(shared_from_this());

   auto writeLambda = [this, lifeTGuard](std::error_code ec, std::size_t length) {
      std::lock_guard lock(guardMutex);
      if(!parent) return;

      if(ec) {
         NCore_Log("WriteFailed: " << ec.message() << "\n")
         parent->onError(Error::WriteFailed, ec);
         RemoveFlag(parent->state, SNI_IN_WRITE);
         RemoveFlag(parent->state, SNI_STOP_WRITE_R);

         parent->abortHalt();
         return;
      }
      parent->onWrite();

      parent->writeQ.pop();
      if (parent->writeQ.empty() || HasFlag(parent->state, SNI_STOP_WRITE_R)) {
         RemoveFlag(parent->state, SNI_IN_WRITE);
         RemoveFlag(parent->state, SNI_STOP_WRITE_R);
         return;
      }

      doWrite();
   };

   asio::async_write(socket, asio::buffer(parent->writeQ.front().data(), parent->writeQ.front().size()), writeLambda);
}

SN::NetStreamAsioW::~NetStreamAsioW() {
   NCore_Log("object got actualy destroyed\n")
}



/*---------------------------NET_STREAM---------------------------*/
SN::NetStream::NetStream(SN::IOContextController&& controller, tcp::socket&& socket_) : readBuffer(20 * 1024), runner(std::move(controller.runner)) {
   processHandler = std::make_shared<SN::NetStreamAsioW>(std::move(controller.handle), this, std::move(socket_));
   processHandler->doTick();
   if(runner.mode == SN::IOContextRunner::Mode::InternalOwned) {
      runner.startThread(processHandler->context.ptr());
   }
}

SN::NetStream::NetStream(SN::IOContextController&& controller) : readBuffer(20 * 1024), runner(std::move(controller.runner)) {
   processHandler = std::make_shared<SN::NetStreamAsioW>(std::move(controller.handle), this);
   processHandler->doTick();
   if(runner.mode == SN::IOContextRunner::Mode::InternalOwned) {
      runner.startThread(processHandler->context.ptr());
   }
}

void SN::NetStream::startRead() {
   processHandler->startRead();
}

void SN::NetStream::startWrite() {
   processHandler->startWrite();
}

void SN::NetStream::stopRead() {
   if(HasNoFlag(state, SNI_IN_READ)) return;
   AddFlag(state, SNI_STOP_READ_R);
}

void SN::NetStream::stopWrite() {
   if(HasNoFlag(state, SNI_IN_WRITE)) return;
   AddFlag(state, SNI_STOP_WRITE_R);
}

void SN::NetStream::send(const std::vector<uint8_t> msg) {
   writeQ.push(msg);
   startWrite();
}

void SN::NetStream::abortHalt() {
   if(!processHandler->parent) return;
   NCore_Log("abortHalt queued\n")
   asio::post(*processHandler->context, [handler = processHandler]() {
      handler->abortHalt();
   });
}

void SN::NetStream::disconnect() {
   abortHalt();
}

void SN::NetStream::shutdown() {
   abortHalt();
   {
      std::lock_guard lock(processHandler->guardMutex);
      processHandler->parent = nullptr;
   }
}

SN::IOContextHandle& SN::NetStream::getContext() {
   return processHandler->context;
}

SN::IOContextRunner& SN::NetStream::getRunner() {
   return runner;
}

SN::IOContextController SN::NetStream::getControllerClone() {
   return SN::IOContextController(processHandler->context.ptr(), runner.ptr());
}

tcp::socket& SN::NetStream::getSocket() {
   return processHandler->socket;
}

std::shared_ptr<SN::NetStreamAsioW> SN::NetStream::getHandle() {
   return processHandler;
}

SN::NetStream::~NetStream() {
   NCore_Log("destroy object\n")
   shutdown();
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
SN::Client::Client() : NetStream(SN::IOContextController()) {
   resolver = std::make_shared<tcp::resolver>(*processHandler->context);
}

SN::Client::Client(SN::IOContextController&& controller_) : NetStream(std::move(controller_)) {
   resolver = std::make_shared<tcp::resolver>(*processHandler->context);
}

SN::Client::Client(SN::IOContextHandle&& handle_) : NetStream(std::move(handle_)) {
   resolver = std::make_shared<tcp::resolver>(*processHandler->context);
}

SN::Client::Client(SN::IOContextRunner&& runner_) : NetStream(std::move(runner_)) {
   resolver = std::make_shared<tcp::resolver>(*processHandler->context);
}

SN::Client::Client(SN::IOContextHandle&& handle_, SN::IOContextRunner&& runner_) : NetStream(SN::IOContextController(std::move(handle_), std::move(runner_))) {
   resolver = std::make_shared<tcp::resolver>(*processHandler->context);
}

void SN::Client::resolve(const std::string& host, uint16_t port) {
   if(HasFlag(state, SNI_ONLINE) || HasFlag(state, SNI_RESOLVEING) || HasFlag(state, SNI_CONNECTING)) return;

   auto resolveLambda = [handler = processHandler, res = resolver](const std::error_code& ec, tcp::resolver::results_type resultEndpoints) {
      std::lock_guard lock(handler->guardMutex);
      if(!handler->parent) return;
      SN::Client* parent = dynamic_cast<SN::Client*>(handler->parent);
      if(!parent) return;

      RemoveFlag(parent->state, SNI_RESOLVEING);
      if(ec) {
         NCore_Log("ResolveFailed: " << ec.message() << "\n")
         parent->onError(Error::ResolveFailed, ec);
      } else {
         parent->endpoints.clear();
         for (auto it = resultEndpoints.begin(); it != resultEndpoints.end(); ++it)
            parent->endpoints.push_back(it->endpoint());
         parent->onResolve();
      }
   };

   AddFlag(state, SNI_RESOLVEING);
   resolver->async_resolve(host, std::to_string(port), resolveLambda);
}

void SN::Client::addEndpoint(const std::string& host, uint16_t port) {
   asio::error_code ec;
   auto addr = asio::ip::make_address(host, ec);
   if (ec) {
      NCore_Log("Invalid address (" << host << "): " << ec.message() << "\n")
      onError(Error::InvalidAddress, ec);
      return;
   }
   endpoints.push_back(tcp::endpoint(addr, port));
}

void SN::Client::connect() {
   if(HasFlag(state, SNI_ONLINE) || HasFlag(state, SNI_CONNECTING)) return;
   if(endpoints.empty()) return;

   auto connectLambda = [handler = processHandler](const std::error_code& ec, const tcp::endpoint& connectedEndpoint) {
      std::lock_guard lock(handler->guardMutex);
      if(!handler->parent) return;
      SN::Client* parent = dynamic_cast<SN::Client*>(handler->parent);
      if(!parent) return;

      RemoveFlag(parent->state, SNI_CONNECTING);

      if(ec) {
         NCore_Log("Connection failed: " << ec.message() << "\n")
         parent->onError(Error::ConnectFailed, ec);
         return;
      } else {
         AddFlag(parent->state, SNI_ONLINE);
         parent->onConnect();
      }
   };

   AddFlag(state, SNI_CONNECTING);
   asio::async_connect(processHandler->socket, endpoints, connectLambda);
}

void SN::Client::abortHalt() {
   NetStream::abortHalt();
}

void SN::Client::disconnect() {
   abortHalt();
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
SN::Connection::Connection(SN::IOContextController&& context, Server& server_, tcp::socket& accepted) : NetStream(std::move(context), std::move(accepted)), server(server_) {
   AddFlag(state, SNI_ONLINE);
   onConnect();
}

void SN::Connection::start() {
   onStart();
}

SN::Server& SN::Connection::getServer() {
   return server;
}

void SN::Connection::abortHalt() {
   NetStream::abortHalt();

   server.removeConnection(this);
}

void SN::Connection::disconnect() {
   abortHalt();
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



/*---------------------------SERVER_ASIO_WRAPPER---------------------------*/
SN::ServerAsioW::ServerAsioW(SN::IOContextHandle&& context_, SN::Server* parent_) : context(std::move(context_)), parent(parent_) {
}

void SN::ServerAsioW::startAccept() {
   auto lifeTGuard(shared_from_this());
   std::lock_guard lock(guardMutex);
   if(!parent) return;

   if(HasFlag(parent->state, SNI_IN_ACCEPT)) return;

   asio::post(*context, [this, lifeTGuard]() {
      doAccept();
   });
}

void SN::ServerAsioW::abortHalt() {
   NCore_Log("actual server abortHalt called\n")
   std::lock_guard lock(guardMutex);
   if(parent) {
      if(HasNoFlag(parent->state, SNI_ONLINE) && (!acceptor || !acceptor->is_open())) return;
      RemoveFlag(parent->state, SNI_ONLINE);
   }

   if(acceptor && acceptor->is_open()) {
      ec = acceptor->cancel(ec);
      if (ec && parent) parent->onError(Error::AcceptorAbortCancelFailed, ec);
      ec = acceptor->close(ec);
      if (ec && parent) parent->onError(Error::AcceptorAbortCloseFailed, ec);
   }

   if(!parent) return;
   for(auto& connection : parent->connections) {
      connection->disconnect();
   }

   parent->connections.clear();
   parent->onEvent(Event::Aborted);
}

void SN::ServerAsioW::doTick() {
   auto lifeTGuard(shared_from_this());

   asio::post(*context, [this, lifeTGuard](){
      std::lock_guard lock(guardMutex);
      if(!parent) {
         return;
      }

      parent->onTick();
      doTick();
   });
}

void SN::ServerAsioW::doAccept() {
   NCore_Log("accept\n")
   auto lifeTGuard(shared_from_this());
   std::lock_guard lock(guardMutex);
   if(!parent) return;

   if(!acceptor) {
      RemoveFlag(parent->state, SNI_OFFLINE);
      RemoveFlag(parent->state, SNI_STOP_ACCEPT_R);
      return;
   }

   pendingSocket.emplace(*context);

   auto acceptLambda = [this, lifeTGuard](const std::error_code& ec_) {
      std::lock_guard lock(guardMutex);
      if(!parent) return;

      ec = ec_;
      if(ec) {
         NCore_Log("AcceptFailed: " << ec.message() << "\n")
         
         if(parent) {
            parent->onError(Error::AcceptFailed, ec);
            RemoveFlag(parent->state, SNI_IN_READ);
            RemoveFlag(parent->state, SNI_STOP_READ_R);
            parent->abortHalt();
         }

         return;
      }

      std::shared_ptr<SN::Connection> connection = parent->onAccept(*pendingSocket);
      connection->start();
      parent->connections.emplace_back(std::move(connection));
      doAccept();
   };

   acceptor->async_accept(*pendingSocket, acceptLambda);
}

SN::ServerAsioW::~ServerAsioW() {
   NCore_Log("server object got actualy destroyed\n")
}



/*---------------------------SERVER---------------------------*/
SN::Server::Server() : runner(SN::IOContextRunner()){
   processHandler = std::make_shared<SN::ServerAsioW>(SN::IOContextHandle(), this);
   processHandler->doTick();
   if(runner.mode == SN::IOContextRunner::Mode::InternalOwned) {
      runner.startThread(processHandler->context.ptr());
   }
}

SN::Server::Server(SN::IOContextController&& controller_) : runner(std::move(controller_.runner)) {
   processHandler = std::make_shared<SN::ServerAsioW>(std::move(controller_.handle), this);
   processHandler->doTick();
   if(runner.mode == SN::IOContextRunner::Mode::InternalOwned) {
      runner.startThread(processHandler->context.ptr());
   }
}

SN::Server::Server(SN::IOContextHandle&& handle_) : runner(SN::IOContextRunner()) {
   processHandler = std::make_shared<SN::ServerAsioW>(std::move(handle_), this);
   processHandler->doTick();
   if(runner.mode == SN::IOContextRunner::Mode::InternalOwned) {
      runner.startThread(processHandler->context.ptr());
   }
}

SN::Server::Server(SN::IOContextRunner&& runner_) : runner(std::move(runner_)) {
   processHandler = std::make_shared<SN::ServerAsioW>(SN::IOContextHandle(), this);
   processHandler->doTick();
   if(runner.mode == SN::IOContextRunner::Mode::InternalOwned) {
      runner.startThread(processHandler->context.ptr());
   }
}

SN::Server::Server(SN::IOContextHandle&& handle_, SN::IOContextRunner&& runner_) : runner(std::move(runner_)) {
   processHandler = std::make_shared<SN::ServerAsioW>(std::move(handle_), this);
   processHandler->doTick();
   if(runner.mode == SN::IOContextRunner::Mode::InternalOwned) {
      runner.startThread(processHandler->context.ptr());
   }
}

void SN::Server::start(uint16_t port_) {
   if(HasFlag(state, SNI_ONLINE)) {
      return;
   }

   AddFlag(state, SNI_ONLINE);
   port = port_;

   processHandler->acceptor.emplace(*processHandler->context, tcp::endpoint(tcp::v4(), port));

   onStart();
}

void SN::Server::startAccept() {
   processHandler->startAccept();
}

void SN::Server::stopAccept() {
   if(HasNoFlag(state, SNI_IN_ACCEPT)) return;
   AddFlag(state, SNI_STOP_ACCEPT_R);
}

void SN::Server::abortHalt() {
   if(!processHandler->parent) return;
   NCore_Log("server abortHalt queued\n")
   asio::post(*processHandler->context, [handler = processHandler]() {
      handler->abortHalt();
   });
}

void SN::Server::close() {
   abortHalt();
}

void SN::Server::removeConnection(Connection* connectionPtr) {
   auto removeLambda = [this, connectionPtr](std::shared_ptr<Connection>& conn) -> bool {
      if(conn.get() == connectionPtr) {
         onDisconnect(conn);
         return true;
      }
      return false;
   };

   connections.erase(std::remove_if(connections.begin(), connections.end(), removeLambda), connections.end());
}

void SN::Server::shutdown() {
   abortHalt();
   {
      std::lock_guard lock(processHandler->guardMutex);
      processHandler->parent = nullptr;
   }
}

SN::IOContextHandle& SN::Server::getContext() {
   return processHandler->context;
}

SN::IOContextRunner& SN::Server::getRunner() {
   return runner;
}

SN::IOContextController SN::Server::getControllerClone() {
   return SN::IOContextController(processHandler->context.ptr(), runner.ptr());
}

std::optional<tcp::acceptor>& SN::Server::getAcceptor() {
   return processHandler->acceptor;
}

std::optional<tcp::socket>& SN::Server::getPending() {
   return processHandler->pendingSocket;
}

uint16_t SN::Server::getPort() {
   return port;
}

std::vector<std::shared_ptr<SN::Connection>>& SN::Server::getConnections() {
   return connections;
}

std::shared_ptr<SN::ServerAsioW> SN::Server::getHandle() {
   return processHandler;
}

SN::Server::~Server() {
   NCore_Log("server destroy object\n")
   shutdown();
}

std::shared_ptr<SN::Connection> SN::Server::onAccept(tcp::socket& socket) {
   return std::make_shared<Connection>(this->getControllerClone(), *this, socket);
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
      case Error::InvalidAddress:
         std::cout << "ERR InvalidAddress" << ec.message() << "\n";
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