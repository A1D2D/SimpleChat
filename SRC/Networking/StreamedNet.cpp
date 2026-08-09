#include "StreamedNet.h"
#include "NetContextRef.h"
#include <iostream>
#include <memory>
#include <utility>

/*---------------------------NET_STREAM_ASIO_WRAPPER---------------------------*/
SN::NetStreamAsioW::NetStreamAsioW(SN::IOContextHandle&& context_, SN::NetStream* parent_, tcp::socket&& socket_) : context(std::move(context_)), socket(std::move(socket_)), parent(parent_) {
   NCore_Log("netstream wrapper created\n")
}

SN::NetStreamAsioW::NetStreamAsioW(SN::IOContextHandle&& context_, SN::NetStream* parent_) : context(std::move(context_)), socket(*context), parent(parent_) {
   NCore_Log("netstream wrapper created\n")
}

void SN::NetStreamAsioW::startRead() {
   auto lifeTGuard(shared_from_this());

   if(HasFlag(state, SNI_IN_READ)) return;

   asio::post(*context, [this, lifeTGuard]() {
      doRead();
   });
}

void SN::NetStreamAsioW::startWrite() {
   auto lifeTGuard(shared_from_this());
   
   if(HasFlag(state, SNI_IN_WRITE)) return;

   asio::post(*context, [this, lifeTGuard]() {
      doWrite();
   });
}

void SN::NetStreamAsioW::stopRead() {
   if(HasNoFlag(state, SNI_IN_READ)) return;
   AddFlag(state, SNI_STOP_READ_R);
}

void SN::NetStreamAsioW::stopWrite() {
   if(HasNoFlag(state, SNI_IN_WRITE)) return;
   AddFlag(state, SNI_STOP_WRITE_R);
}

void SN::NetStreamAsioW::abortHalt() {
   auto lifeTGuard(shared_from_this());
   NCore_Log("NetStream actual abortHalt called\n")
   
   if(HasNoFlag(state, SNI_ONLINE) && HasNoFlag(state, SNI_RESOLVEING) && HasNoFlag(state, SNI_CONNECTING) && !socket.is_open()) return;
   RemoveFlag(state, SNI_ONLINE);
   RemoveFlag(state, SNI_RESOLVEING);
   RemoveFlag(state, SNI_CONNECTING);

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

void SN::NetStreamAsioW::doRead() {
   NCore_Log("read\n")
   auto lifeTGuard(shared_from_this());

   auto readLambda = [this, lifeTGuard](std::error_code ec, std::size_t length) {
      if(!parent) return;

      if(ec) {
         NCore_Log("ReadFailed: " << ec.message() << "\n")
         parent->onError(Error::ReadFailed, ec);
         RemoveFlag(state, SNI_IN_READ);
         RemoveFlag(state, SNI_STOP_READ_R);

         parent->abortHalt();
         return;
      }

      for (int i = 0; i < length; ++i) parent->readQ.push(parent->readBuffer[i]);
      parent->onRead();

      if(HasFlag(state, SNI_STOP_READ_R)) {
         RemoveFlag(state, SNI_IN_READ);
         RemoveFlag(state, SNI_STOP_READ_R);
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
      if(!parent) return;

      if(ec) {
         NCore_Log("WriteFailed: " << ec.message() << "\n")
         parent->onError(Error::WriteFailed, ec);
         RemoveFlag(state, SNI_IN_WRITE);
         RemoveFlag(state, SNI_STOP_WRITE_R);

         parent->abortHalt();
         return;
      }
      parent->onWrite();

      if (parent->writeQ.empty() || HasFlag(state, SNI_STOP_WRITE_R)) {
         RemoveFlag(state, SNI_IN_WRITE);
         RemoveFlag(state, SNI_STOP_WRITE_R);
         return;
      }

      doWrite();
   };

   if(!parent->writeQ.empty()) {
      asio::async_write(socket, asio::buffer(parent->writeQ.front().data(), parent->writeQ.front().size()), writeLambda);
      parent->writeQ.pop();
   }
}

SN::NetStreamAsioW::~NetStreamAsioW() {
   NCore_Log("NetStreamAsioW: got actualy destroyed\n")
}


/*---------------------------NET_STREAM---------------------------*/
SN::NetStream::NetStream(SN::IOContextHandle&& context_, tcp::socket&& socket_) : readBuffer(20 * 1024) {
   workGuard = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(context_->get_executor());
   processHandler = std::make_shared<SN::NetStreamAsioW>(std::move(context_), this, std::move(socket_));
}

SN::NetStream::NetStream(SN::IOContextHandle&& context_) : readBuffer(20 * 1024) {
   workGuard = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(context_->get_executor());
   processHandler = std::make_shared<SN::NetStreamAsioW>(std::move(context_), this);
}

SN::NetStream::NetStream(NetStream&& other) noexcept : 
   processHandler(std::move(other.processHandler)), 
   readBuffer(std::move(other.readBuffer)), writeQ(std::move(other.writeQ)), 
   readQ(std::move(other.readQ)), workGuard(std::move(other.workGuard)) {
   if(!processHandler) return;
   processHandler->parent = this;
}

SN::NetStream& SN::NetStream::operator=(NetStream&& other) noexcept {
   if (this != &other) {
      processHandler = std::move(other.processHandler);
      readBuffer = std::move(other.readBuffer);
      writeQ = std::move(other.writeQ);
      readQ = std::move(other.readQ);
      workGuard = std::move(other.workGuard);
   }

   return *this;
}

void SN::NetStream::startRead() {
   processHandler->startRead();
}

void SN::NetStream::startWrite() {
   processHandler->startWrite();
}

void SN::NetStream::stopRead() {
   processHandler->stopRead();
}

void SN::NetStream::stopWrite() {
   processHandler->stopWrite();
}

void SN::NetStream::send(const std::vector<uint8_t> msg) {
   writeQ.push(msg);
   startWrite();
}

void SN::NetStream::abortHalt() {
   if(!processHandler || !processHandler->parent) return;
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
   if(!processHandler) return;
   processHandler->parent = nullptr;
}

std::optional<std::reference_wrapper<SN::IOContextHandle>> SN::NetStream::getContext() {
   if(!processHandler) return std::nullopt;
   return processHandler->context;
}

std::optional<std::reference_wrapper<tcp::socket>> SN::NetStream::getSocket() {
   if(!processHandler) return std::nullopt;
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
SN::Client::Client() : NetStream(SN::IOContextHandle()) {
   resolver = std::make_shared<tcp::resolver>(*processHandler->context);
}

SN::Client::Client(SN::IOContextHandle&& context_) : NetStream(std::move(context_)) {
   resolver = std::make_shared<tcp::resolver>(*processHandler->context);
}


void SN::Client::resolve(const std::string& host, uint16_t port) {
   if(HasFlag(processHandler->state, SNI_ONLINE) || HasFlag(processHandler->state, SNI_RESOLVEING) || HasFlag(processHandler->state, SNI_CONNECTING)) return;

   auto resolveLambda = [handler = processHandler, res = resolver](const std::error_code& ec, tcp::resolver::results_type resultEndpoints) {
      if(!handler->parent) return;
      SN::Client* parent = dynamic_cast<SN::Client*>(handler->parent);
      if(!parent) return;

      RemoveFlag(handler->state, SNI_RESOLVEING);
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

   AddFlag(processHandler->state, SNI_RESOLVEING);
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
   if(HasFlag(processHandler->state, SNI_ONLINE) || HasFlag(processHandler->state, SNI_CONNECTING)) return;
   if(endpoints.empty()) return;

   auto connectLambda = [handler = processHandler](const std::error_code& ec, const tcp::endpoint& connectedEndpoint) {
      if(!handler->parent) return;
      SN::Client* parent = dynamic_cast<SN::Client*>(handler->parent);
      if(!parent) return;

      RemoveFlag(handler->state, SNI_CONNECTING);

      if(ec) {
         NCore_Log("Connection failed: " << ec.message() << "\n")
         parent->onError(Error::ConnectFailed, ec);
         return;
      } else {
         AddFlag(handler->state, SNI_ONLINE);
         parent->onConnect();
      }
   };

   AddFlag(processHandler->state, SNI_CONNECTING);
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
SN::Connection::Connection(SN::IOContextHandle&& context, Server* server_, tcp::socket& accepted) : NetStream(std::move(context), std::move(accepted)), server(server_) {
   AddFlag(processHandler->state, SNI_ONLINE);
   onConnect();
}

void SN::Connection::start() {
   onStart();
}

void SN::Connection::abortHalt() {
   NetStream::abortHalt();

   server->removeConnection(this);
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
   NCore_Log("server wrapper created\n")
}

void SN::ServerAsioW::startAccept() {
   auto lifeTGuard(shared_from_this());
   if(!parent) return;

   if(HasFlag(state, SNI_IN_ACCEPT)) return;

   asio::post(*context, [this, lifeTGuard]() {
      doAccept();
   });
}

void SN::ServerAsioW::stopAccept() {
   if(HasNoFlag(state, SNI_IN_ACCEPT)) return;
   AddFlag(state, SNI_STOP_ACCEPT_R);
}

void SN::ServerAsioW::abortHalt() {
   auto lifeTGuard(shared_from_this());
   NCore_Log("actual Server abortHalt called\n")
   if(parent) {
      if(HasNoFlag(state, SNI_ONLINE) && (!acceptor || !acceptor->is_open())) return;
      RemoveFlag(state, SNI_ONLINE);
   }

   if(acceptor && acceptor->is_open()) {
      ec = acceptor->cancel(ec);
      if (ec && parent) parent->onError(Error::AcceptorAbortCancelFailed, ec);
      ec = acceptor->close(ec);
      if (ec && parent) parent->onError(Error::AcceptorAbortCloseFailed, ec);
   }

   if(!parent) return;

   while (!parent->connections.empty()) {
      parent->connections.back()->disconnect();
   }

   parent->connections.clear();
   parent->onEvent(Event::Aborted);
}

void SN::ServerAsioW::doAccept() {
   NCore_Log("accept\n")
   auto lifeTGuard(shared_from_this());

   if(!acceptor) {
      RemoveFlag(state, SNI_OFFLINE);
      RemoveFlag(state, SNI_STOP_ACCEPT_R);
      return;
   }

   pendingSocket.emplace(*context);

   auto acceptLambda = [this, lifeTGuard](const std::error_code& ec_) {
      if(!parent) return;

      ec = ec_;
      if(ec) {
         NCore_Log("AcceptFailed: " << ec.message() << "\n")
         
         if(parent) {
            parent->onError(Error::AcceptFailed, ec);
            RemoveFlag(state, SNI_IN_READ);
            RemoveFlag(state, SNI_STOP_READ_R);
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
   NCore_Log("Server got actualy destroyed\n")
}



/*---------------------------SERVER---------------------------*/
SN::Server::Server() {
   processHandler = std::make_shared<SN::ServerAsioW>(SN::IOContextHandle(), this);
}

SN::Server::Server(SN::IOContextHandle&& context_) {
   processHandler = std::make_shared<SN::ServerAsioW>(std::move(context_), this);
}

SN::Server::Server(Server&& other) noexcept : processHandler(std::move(other.processHandler)), 
   connections(std::move(other.connections)), 
   port(std::move(other.port)) {
   if(!processHandler) return;
   processHandler->parent = this;
   for (auto connection : connections) connection->server = this;
}

void SN::Server::start(uint16_t port_) {
   if(HasFlag(processHandler->state, SNI_ONLINE)) {
      return;
   }

   AddFlag(processHandler->state, SNI_ONLINE);
   port = port_;

   processHandler->acceptor.emplace(*processHandler->context, tcp::endpoint(tcp::v4(), port));

   onStart();
}

void SN::Server::startAccept() {
   processHandler->startAccept();
}

void SN::Server::stopAccept() {
   processHandler->stopAccept();
}

void SN::Server::abortHalt() {
   if(!processHandler || !processHandler->parent) return;
   NCore_Log("server abortHalt queued\n")

   asio::post(*processHandler->context, [handler = processHandler]() {
      handler->abortHalt();
   });
}

void SN::Server::close() {
   abortHalt();
}

void SN::Server::removeConnection(Connection* connectionPtr) {
   auto it = std::find_if(connections.begin(), connections.end(), [connectionPtr](const std::shared_ptr<Connection>& conn) {
      return conn.get() == connectionPtr;
   });

   if (it != connections.end()) {
      onDisconnect(*it);
      connections.erase(it);
   }
}

void SN::Server::shutdown() {
   abortHalt();
   if(!processHandler) return;
   processHandler->parent = nullptr;
}

SN::IOContextHandle& SN::Server::getContext() {
   return processHandler->context;
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
   SN::IOContextHandle context(this->getContext().ptr());
   return std::make_shared<Connection>(std::move(context), this, socket);
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