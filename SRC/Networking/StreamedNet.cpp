#include "StreamedNet.h"
#include <memory>
#include <string>

namespace SN {
   /*CONTEXT*/
   Context::Context() : state(std::make_shared<State>()) {}

   void Context::use() {
      if (!state) return;
      state->usageCount++;
   }

   void Context::release() {
      if (!state) return;
      if (state->usageCount == 0) {
         std::cerr << "usage cant be dicreased already 0" << std::endl;
         return;
      }
      state->usageCount--;
   }

   size_t Context::usage() const {
      if (!state) return 0;
      return state->usageCount.load();
   }

   Context::CallbackHandle Context::addCallback(std::function<void()> callback) {
      if (!state) return {};
      CallbackPtr callbackPtr = std::make_shared<std::function<void()>>(std::move(callback));
      std::lock_guard lock(state->callbackMutex);
      state->callbacks.emplace_back(callbackPtr);
      return callbackPtr;
   }

   void Context::removeCallback(const CallbackHandle& handle) {
      if (!state) return;

      auto callback = handle.lock();

      if (!callback) return;

      std::lock_guard lock(state->callbackMutex);
      std::erase_if(state->callbacks, [&](const CallbackPtr& entry) {
         return entry == callback;
      });
   }

   void Context::poll() {
      if (!state) {
         std::cerr << "failed to poll state is null ptr" << std::endl;
         return;
      }
      std::vector<CallbackPtr> tempCallbacks;
      {
         std::lock_guard lock(state->callbackMutex);
         tempCallbacks = state->callbacks;
      }
      state->io.poll();
      for (CallbackPtr& callback : tempCallbacks) {
         if (!callback) continue;
         (*callback)();
      }
   }
   
   //UseGuard
   Context::UsageGuard::UsageGuard(Context context) : context(std::move(context)) {
      this->context.use();
   }

   Context::UsageGuard& Context::UsageGuard::operator=(UsageGuard&& other) noexcept {
      if (this != &other) {
         release();
         context = std::move(other.context);
         active = other.active;
         other.active = false;
      }
      return *this;
   }

   Context::UsageGuard::UsageGuard(UsageGuard&& other) noexcept : context(std::move(other.context)), active(other.active) {
      other.active = false;
   }

   void Context::UsageGuard::release() {
      if (active) {
         context.release();
         active = false;
      }
   }

   Context::UsageGuard::~UsageGuard() {
      release();
   }

   //Callback
   Context::Callback::Callback(Context context, std::function<void()> callback) : context(std::move(context)) {
      handle = this->context.addCallback(
         std::move(callback)
      );
   }

   Context::Callback::Callback(Callback&& other) noexcept : context(std::move(other.context)), handle(std::move(other.handle)), active(other.active) {
      other.active = false;
   }

   Context::Callback& Context::Callback::operator=(Callback&& other) noexcept {
      if (this != &other) {
         remove();
         context = std::move(other.context);
         handle = std::move(other.handle);
         active = other.active;
         other.active = false;
      }
      return *this;
   }

   void Context::Callback::remove() {
      if (active) {
         context.removeCallback(handle);
         active = false;
      }
   }
   
   Context::Callback::~Callback() {
      remove();
   }
   /*~Context*/


   /*Resolver*/
   Resolver::Resolver() {}

   Resolver::Resolver(Context context_) {
      setContext(context_);
   }

   Resolver::Resolver(Resolver&& other) noexcept : context(std::move(other.context)), state(std::move(other.state)) {
      if(!state) return;
      std::lock_guard guard(state->mutex);
      state->reference = this;
   }

   Resolver& Resolver::operator=(Resolver&& other) noexcept {
      if (this != &other) {
         context = std::move(other.context);
         state = std::move(other.state);
         if(!state) return *this;
         std::lock_guard guard(state->mutex);
         state->reference = this;
      }

      return *this;
   }

   void Resolver::shutdown() {
      if (!state) return;
   
      stopTick();
      std::lock_guard guard(state->mutex);
      state->reference = nullptr;
      state->usageGuard.release();
   }

   Resolver::~Resolver() {
      shutdown();
   }

   void Resolver::setContext(Context context_) {
      if (!context_.state) {
         std::cerr << "failed to assign context (context state) its a null ptr" << std::endl;
         return;
      }
      context = context_;
      state = std::make_shared<State>(this, context);
   }

   Context Resolver::getContext() const {
      return context;
   }

   template<>
   void Resolver::resolve<NetworkMode::TCP>(const std::string& host, uint16_t port) {
      if (!state || !context.state) {
         std::cerr << "failed to start resolve tcp (state/context) is a null ptr" << std::endl;
         return;
      }

      auto resolveLambda = [st = state](const std::error_code& ec, tcp::resolver::results_type resultEndpoints) {
         if(!st) return;
         if (ec) {
            std::cerr << "resolve failed erc: " << ec.message() << std::endl;
            return;
         }
         std::lock_guard guard(st->mutex);
         if (!st->reference) return;
         
         st->reference->onTcpResolve(std::vector<tcp::endpoint>(resultEndpoints.begin(), resultEndpoints.end()));
      };

      state->tcpResolver.async_resolve(host, std::to_string(port), resolveLambda);
   }

   template<>
   void Resolver::resolve<NetworkMode::UDP>(const std::string& host, uint16_t port) {
      if (!state || !context.state) {
         std::cerr << "failed to start resolve udp (state/context) is a null ptr" << std::endl;
         return;
      }

      auto resolveLambda = [st = state](const std::error_code& ec, udp::resolver::results_type resultEndpoints) {
         if(!st) return;
         if (ec) {
            std::cerr << "resolve failed erc: " << ec.message() << std::endl;
            return;
         }
         std::lock_guard guard(st->mutex);
         if (!st->reference) return;

         st->reference->onUdpResolve(std::vector<udp::endpoint>(resultEndpoints.begin(), resultEndpoints.end()));
      };

      state->udpResolver.async_resolve(host, std::to_string(port), resolveLambda);
   }
   /*~Resolver*/


   /*TcpClient*/
   Client<NetworkMode::TCP>::Client() {}

   Client<NetworkMode::TCP>::Client(Context context_) {
      setContext(context_);
   }

   Client<NetworkMode::TCP>::Client(Client&& other) noexcept : context(std::move(other.context)), state(std::move(other.state)) {
      if(!state) return;
      std::lock_guard guard(state->mutex);
      state->reference = this;
   }

   Client<NetworkMode::TCP>& Client<NetworkMode::TCP>::operator=(Client&& other) noexcept {
      if (this != &other) {
         context = std::move(other.context);
         state = std::move(other.state);
         if(!state) return *this;
         std::lock_guard guard(state->mutex);
         state->reference = this;
      }

      return *this;
   }

   void Client<NetworkMode::TCP>::shutdown() {
      if (!state) return;
   
      std::lock_guard guard(state->mutex);
      close();
      state->reference = nullptr;
      state->usageGuard.release();
   }

   Client<NetworkMode::TCP>::~Client() {
      shutdown();
   }

   void Client<NetworkMode::TCP>::setContext(Context context_) {
      if (!context_.state) {
         std::cerr << "failed to assign context (context state) its a null ptr" << std::endl;
         return;
      }
      context = context_;
      state = std::make_shared<State>(this, context);
   }

   Context Client<NetworkMode::TCP>::getContext() const {
      return context;
   }

   void Client<NetworkMode::TCP>::connect(std::vector<tcp::endpoint> endpoints) {
      if (!state || !context.state) {
         std::cerr << "failed to start tcp connection (state/context) is a null ptr" << std::endl;
         return;
      }
      
      auto connectLambda = [st = state](const std::error_code& ec, const tcp::endpoint& connectedEndpoint) {
         if(!st) return;
         if(ec) {
            std::cerr << "failed to connect erc: " << ec.message() << std::endl;
            return;
         }
         std::lock_guard guard(st->mutex);
         if(!st->reference) return;
         st->reference->onConnect();
      };

      asio::async_connect(state->socket, endpoints, connectLambda);
   }

   void Client<NetworkMode::TCP>::send(const std::vector<uint8_t>& msg) {
      if(!state) return;
      state->writeQueue.push(msg); //TODO: not 100 thread safe
      startWrite();
   }

   void Client<NetworkMode::TCP>::disconnect() {
      close();
      onDisconnect();
   }

   void Client<NetworkMode::TCP>::close() {
      if(!state) return;

      stopRead();
      stopWrite();
      stopTick();

      if(state->socket.is_open()) {
         asio::error_code ec;
         ec = state->socket.shutdown(tcp::socket::shutdown_both, ec);
         if(ec) std::cerr << "failed to shutdown socket erc: " << ec.message();
         ec = state->socket.close(ec);
         if(ec) std::cerr << "failed to close socket erc: " << ec.message();
      }
   }

   void Client<NetworkMode::TCP>::doRead() {
      if(!state || !state->reading) return;

      auto readLambda = [st = state](std::error_code ec, std::size_t length) {
         if(!st || !st->reading) return;
         if(ec) {
            std::cerr << "failed to read erc: " << ec.message() << std::endl;
            st->reading = false;
            std::lock_guard guard(st->mutex);
            if (!st->reference) return;
            st->reference->disconnect();
            return;
         }
         
         std::lock_guard guard(st->mutex);
         if (!st->reference) {
            st->reading = false;
            return;
         }
         st->reference->doRead();
         st->reference->onRead(std::vector<uint8_t>(st->readBuffer.begin(), st->readBuffer.begin() + std::min(length, st->readBuffer.size())));
      };

      state->socket.async_read_some(asio::buffer(state->readBuffer.data(), state->readBuffer.size()), readLambda);
   }

   void Client<NetworkMode::TCP>::doWrite() {
      if(!state || !state->writing) return;
      if(state->writeQueue.empty()) {
         state->writing = false;
         return;
      }

      auto writeLambda = [st = state](std::error_code ec, std::size_t length) {
         if(!st || !st->writing) return;
         if(ec) {
            std::cerr << "failed to write erc: " << ec.message() << std::endl;
            st->writing = false;
            return;
         }
         
         std::lock_guard guard(st->mutex);
         if (!st->reference) {
            return;
            st->reading = false;
         }
         st->reference->doWrite();
         st->reference->onWrite();
      };

      asio::async_write(state->socket, asio::buffer(state->writeQueue.front().data(), state->writeQueue.front().size()), writeLambda);//TODO: ERROR prob starts lambda and pops idk
      state->writeQueue.pop();
   }

   void Client<NetworkMode::TCP>::startRead() {
      if(!state) return;

      asio::post(context.state->io, [st = state]() {
         std::lock_guard guard(st->mutex);
         if (!st->reference || st->reading) return;
         st->reading = true;
         st->reference->doRead();
      });
   }

   void Client<NetworkMode::TCP>::startWrite() {
      if(!state) return;

      asio::post(context.state->io, [st = state]() {
         if(!st) return;
         std::lock_guard guard(st->mutex);
         if (!st->reference || st->writing) return;
         st->writing = true;
         st->reference->doWrite();
      });
   }

   void Client<NetworkMode::TCP>::stopRead() {
      if(!state) return;

      asio::post(context.state->io, [st = state]() {
         if(!st) return;
         st->reading = false;
      });
   }

   void Client<NetworkMode::TCP>::stopWrite() {
      if(!state) return;

      asio::post(context.state->io, [st = state]() {
         if(!st) return;
         st->writing = false;
      });
   }
   /*~TcpClient*/


   /*UdpClient*/
   Client<NetworkMode::UDP>::Client() {}

   Client<NetworkMode::UDP>::Client(Context context_) {
      setContext(context_);
   }

   Client<NetworkMode::UDP>::Client(Client&& other) noexcept : context(std::move(other.context)), state(std::move(other.state)) {
      if(!state) return;
      std::lock_guard guard(state->mutex);
      state->reference = this;
   }

   Client<NetworkMode::UDP>& Client<NetworkMode::UDP>::operator=(Client&& other) noexcept {
      if (this != &other) {
         context = std::move(other.context);
         state = std::move(other.state);
         if(!state) return *this;
         std::lock_guard guard(state->mutex);
         state->reference = this;
      }

      return *this;
   }

   void Client<NetworkMode::UDP>::shutdown() {
      if(!state) return;
   
      std::lock_guard guard(state->mutex);
      close();
      state->reference = nullptr;
      state->usageGuard.release();
   }

   Client<NetworkMode::UDP>::~Client() {
      shutdown();
   }

   void Client<NetworkMode::UDP>::setContext(Context context_) {
      if (!context_.state) {
         std::cerr << "failed to assign context (context state) its a null ptr" << std::endl;
         return;
      }
      context = context_;
      state = std::make_shared<State>(this, context);
   }

   Context Client<NetworkMode::UDP>::getContext() const {
      return context;
   }

   void Client<NetworkMode::UDP>::connect(std::vector<udp::endpoint> endpoints) {
      if (!state || !context.state) {
         std::cerr << "failed to start tcp connection (state/context) is a null ptr" << std::endl;
         return;
      }
      
      auto connectLambda = [st = state](const std::error_code& ec, const udp::endpoint& connectedEndpoint) {
         if(!st) return;
         if(ec) {
            std::cerr << "failed to connect erc: " << ec.message() << std::endl;
            return;
         }
         std::lock_guard guard(st->mutex);
         if(!st->reference) return;
         st->reference->onConnect();
      };

      asio::async_connect(state->socket, endpoints, connectLambda);
   }

   void Client<NetworkMode::UDP>::send(const std::vector<uint8_t>& msg) {
      if(!state) return;
      state->writeQueue.push(msg); //TODO: not 100 thread safe
      startWrite();
   }

   void Client<NetworkMode::UDP>::disconnect() {
      close();
      onDisconnect();
   }

   void Client<NetworkMode::UDP>::close() {
      if(!state) return;

      stopRead();
      stopWrite();
      stopTick();

      if(state->socket.is_open()) {
         asio::error_code ec;
         ec = state->socket.shutdown(tcp::socket::shutdown_both, ec);
         if(ec) std::cerr << "failed to shutdown socket erc: " << ec.message();
         ec = state->socket.close(ec);
         if(ec) std::cerr << "failed to close socket erc: " << ec.message();
      }
   }

   void Client<NetworkMode::UDP>::doRead() {
      if(!state || !state->reading) return;

      auto readLambda = [st = state](std::error_code ec, std::size_t length) {
         if(!st || !st->reading) return;
         if(ec) {
            std::cerr << "failed to read erc: " << ec.message() << std::endl;
            st->reading = false;
            std::lock_guard guard(st->mutex);
            if (!st->reference) return;
            st->reference->disconnect();
            return;
         }
         
         std::lock_guard guard(st->mutex);
         if (!st->reference) {
            return;
            st->reading = false;
         }
         st->reference->doRead();
         st->reference->onRead(std::vector<uint8_t>(st->readBuffer.begin(), st->readBuffer.begin() + std::min(length, st->readBuffer.size())));
      };

      state->socket.async_receive(asio::buffer(state->readBuffer.data(), state->readBuffer.size()), readLambda);
   }

   void Client<NetworkMode::UDP>::doWrite() {
      if(!state || !state->writing) return;
      if(state->writeQueue.empty()) {
         state->writing = false;
         return;
      }

      auto writeLambda = [st = state](std::error_code ec, std::size_t length) {
         if(!st || !st->writing) return;
         if(ec) {
            std::cerr << "failed to write erc: " << ec.message() << std::endl;
            st->writing = false;
            return;
         }
         
         std::lock_guard guard(st->mutex);
         if (!st->reference) {
            return;
            st->reading = false;
         }
         st->reference->doWrite();
         st->reference->onWrite();
      };

      state->socket.async_send(asio::buffer(state->writeQueue.front().data(), state->writeQueue.front().size()), writeLambda);//TODO: ERROR prob starts lambda and pops idk
      state->writeQueue.pop();
   }

   void Client<NetworkMode::UDP>::startRead() {
      if(!state) return;

      asio::post(context.state->io, [st = state]() {
         std::lock_guard guard(st->mutex);
         if (!st->reference || st->reading) return;
         st->reading = true;
         st->reference->doRead();
      });
   }

   void Client<NetworkMode::UDP>::startWrite() {
      if(!state) return;

      asio::post(context.state->io, [st = state]() {
         if(!st) return;
         std::lock_guard guard(st->mutex);
         if (!st->reference || st->writing) return;
         st->writing = true;
         st->reference->doWrite();
      });
   }

   void Client<NetworkMode::UDP>::stopRead() {
      if(!state) return;

      asio::post(context.state->io, [st = state]() {
         if(!st) return;
         st->reading = false;
      });
   }

   void Client<NetworkMode::UDP>::stopWrite() {
      if(!state) return;

      asio::post(context.state->io, [st = state]() {
         if(!st) return;
         st->writing = false;
      });
   }
   /*~UdpClient*/


   /*TcpServer*/
   Server<NetworkMode::TCP>::Server() {}

   Server<NetworkMode::TCP>::Server(Context context_) {
      setContext(context_);
   }

   Server<NetworkMode::TCP>::Server(Server&& other) noexcept : context(std::move(other.context)), state(std::move(other.state)) {
      if(!state) return;
      std::lock_guard guard(state->mutex);
      for (auto& c : state->connections) {//TODO: need mutext guard for server
         c->setServer(this);
      }
      state->reference = this;
   }

   Server<NetworkMode::TCP>& Server<NetworkMode::TCP>::operator=(Server&& other) noexcept {
      if (this != &other) {
         context = std::move(other.context);
         state = std::move(other.state);
         if(!state) return *this;
         std::lock_guard guard(state->mutex);
         for (auto& c : state->connections) {//TODO: need mutext guard for server
            c->setServer(this);
         }
         state->reference = this;
      }

      return *this;
   }

   void Server<NetworkMode::TCP>::shutdown() {
      if (!state) return;
   
      std::lock_guard guard(state->mutex);
      state->reference = nullptr;
      for (auto& c : state->connections) {//TODO: need mutext guard for server
         c->setServer(nullptr);
      }
      state->usageGuard.release();
   }

   Server<NetworkMode::TCP>::~Server() {
      shutdown();
   }

   void Server<NetworkMode::TCP>::setContext(Context context_) {
      if (!context_.state) {
         std::cerr << "failed to assign context (context state) its a null ptr" << std::endl;
         return;
      }
      context = context_;
      state = std::make_shared<State>(this, context);
   }

   Context Server<NetworkMode::TCP>::getContext() const {
      return context;
   }

   std::vector<std::shared_ptr<Connection<NetworkMode::TCP>>> Server<NetworkMode::TCP>::getConnections() {
      if(!state) std::vector<std::shared_ptr<Connection<NetworkMode::TCP>>>();
      return state->connections;
   }

   void Server<NetworkMode::TCP>::start(std::vector<tcp::endpoint> endpoints) {
      if(!state) return;
      
      for (auto endpoint: endpoints) {
         state->acceptors.push_back(std::make_shared<tcp::acceptor>(context.state->io, endpoint));
      }

      onStart();
   }

   void Server<NetworkMode::TCP>::start(tcp::endpoint endpoint) {
      start(std::vector<tcp::endpoint>{endpoint});
   }

   void Server<NetworkMode::TCP>::send(const std::vector<uint8_t>& msg) {
      if(!state) return;
      for(auto connection : state->connections) {
         if(!connection) continue;
         connection->send(msg);
      }
   }

   void Server<NetworkMode::TCP>::disconnect() {
      if(!state) return;
      state->connections.clear();
      stopAccept();
      stopTick();
      for (auto& a : state->acceptors) {
         if(!a) continue;
         if(a->is_open()) {
            asio::error_code ec;
            ec = a->cancel(ec);
            if(ec) std::cerr << "failed to shutdown socket erc: " << ec.message();
            ec = a->close(ec);
            if(ec) std::cerr << "failed to close socket erc: " << ec.message();
         }
      }      
      state->acceptors.clear();
   }

   void Server<NetworkMode::TCP>::removeConnection(Connection<NetworkMode::TCP>* connection) {
      if(!state || !connection) return;
      state->connections.erase(std::remove_if(state->connections.begin(), state->connections.end(), [connection](const auto& c) {
         if(!c) return true;
         return c.get() == connection;
      }), state->connections.end());
   }

   void Server<NetworkMode::TCP>::doAccept(std::shared_ptr<tcp::acceptor> acceptor) {
      if(!state || !acceptor || !state->accepting) return;

      std::shared_ptr<tcp::socket> socket = std::make_shared<tcp::socket>(context.state->io);

      auto accpetLambda = [st = state, socket, acceptor](std::error_code ec) {
         if(!st || !socket || !st->accepting) return;
         if(ec) {
            st->accepting = false;
            std::cerr << "failed to accept erc:" << ec.message() << std::endl;
            return;
         }

         std::lock_guard guard(st->mutex);
         if(!st->reference) {
            st->accepting = false;
            return;
         }
         st->reference->doAccept(acceptor);
         std::shared_ptr<Connection<NetworkMode::TCP>> connection = st->reference->onAccept(std::move(*socket));
         if(connection) {
            st->connections.emplace_back(connection);
            connection->start();
         }
      };

      acceptor->async_accept(*socket, accpetLambda);
   }

   void Server<NetworkMode::TCP>::startRead() {
      if(!state) return;

      asio::post(context.state->io, [st = state]() {
         if(!st) return;
         std::lock_guard guard(st->mutex);
         if(!st->reference) return;
         if (!st->reference || st->accepting) return;
         st->accepting = true;
         for (auto acceptor : st->acceptors) {
            st->reference->doAccept(acceptor);
         }
      });
   }

   void Server<NetworkMode::TCP>::stopAccept() {
      if(!state) return;

      asio::post(context.state->io, [st = state]() {
         if(!st) return;
         st->accepting = false;
      });
   }

   std::shared_ptr<Connection<NetworkMode::TCP>> Server<NetworkMode::TCP>::onAccept(tcp::socket acceptedSocket) {
      return std::make_shared<Connection<NetworkMode::TCP>>(this, std::move(acceptedSocket));
   }
   /*~TcpServer*/


   /*TcpConnection*/
   Connection<NetworkMode::TCP>::Connection() {}

   Connection<NetworkMode::TCP>::Connection(Server<NetworkMode::TCP>* server, tcp::socket socket) {
      if(!server) return;
      setEnv(server->getContext(), std::move(socket));
      setServer(server);
   }

   Connection<NetworkMode::TCP>::Connection(Connection&& other) noexcept : context(std::move(other.context)), state(std::move(other.state)) {
      if(!state) return;
      std::lock_guard guard(state->mutex);
      state->reference = this;
   }

   Connection<NetworkMode::TCP>& Connection<NetworkMode::TCP>::operator=(Connection&& other) noexcept {
      if (this != &other) {
         context = std::move(other.context);
         state = std::move(other.state);
         if(!state) return *this;
         std::lock_guard guard(state->mutex);
         state->reference = this;
      }

      return *this;
   }

   void Connection<NetworkMode::TCP>::shutdown() {
      if(!state) return;
   
      std::lock_guard guard(state->mutex);
      close();
      state->reference = nullptr;
      state->usageGuard.release();
   }
   
   Connection<NetworkMode::TCP>::~Connection() {
      shutdown();
   }
   
   void Connection<NetworkMode::TCP>::setServer(Server<NetworkMode::TCP>* server_) {
      if(!state || !server_) return;
      state->server = server_;
   }

   void Connection<NetworkMode::TCP>::setEnv(Context context_, tcp::socket socket) {
      if (!context_.state) {
         std::cerr << "failed to assign context (context state) its a null ptr" << std::endl;
         return;
      }
      context = context_;
      state = std::make_shared<State>(this, context, std::move(socket));
   }

   Context Connection<NetworkMode::TCP>::getContext() const {
      return context;
   }

   void Connection<NetworkMode::TCP>::start() {
      onStart();
   }

   void Connection<NetworkMode::TCP>::send(const std::vector<uint8_t>& msg) {
      if(!state) return;
      state->writeQueue.push(msg); //TODO: not 100 thread safe
      startWrite();
   }

   void Connection<NetworkMode::TCP>::disconnect() {
      close();
      std::shared_ptr<State> st = state;
      onDisconnect();
      if(!st || !st->server) return;
      st->server->removeConnection(this);
   }

   void Connection<NetworkMode::TCP>::close() {
      if(!state) return;

      stopRead();
      stopWrite();
      stopTick();

      if(state->socket.is_open()) {
         asio::error_code ec;
         ec = state->socket.shutdown(tcp::socket::shutdown_both, ec);
         if(ec) std::cerr << "failed to shutdown socket erc: " << ec.message();
         ec = state->socket.close(ec);
         if(ec) std::cerr << "failed to close socket erc: " << ec.message();
      }
   }

   void Connection<NetworkMode::TCP>::doRead() {
      if(!state || !state->reading) return;

      auto readLambda = [st = state](std::error_code ec, std::size_t length) {
         if(!st || !st->reading) return;
         if(ec) {
            std::cerr << "failed to read erc: " << ec.message() << std::endl;
            st->reading = false;
            std::lock_guard guard(st->mutex);
            if (!st->reference) return;
            st->reference->disconnect();
            return;
         }
         
         std::lock_guard guard(st->mutex);
         if (!st->reference) {
            st->reading = false;
            return;
         }
         st->reference->doRead();
         st->reference->onRead(std::vector<uint8_t>(st->readBuffer.begin(), st->readBuffer.begin() + std::min(length, st->readBuffer.size())));
      };

      state->socket.async_read_some(asio::buffer(state->readBuffer.data(), state->readBuffer.size()), readLambda);
   }
   
   void Connection<NetworkMode::TCP>::doWrite() {
      if(!state || !state->writing) return;
      if(state->writeQueue.empty()) {
         state->writing = false;
         return;
      }

      auto writeLambda = [st = state](std::error_code ec, std::size_t length) {
         if(!st || !st->writing) return;
         if(ec) {
            std::cerr << "failed to write erc: " << ec.message() << std::endl;
            st->writing = false;
            return;
         }
         
         std::lock_guard guard(st->mutex);
         if (!st->reference) {
            return;
            st->writing = false;
         }
         st->reference->doWrite();
         st->reference->onWrite();
      };

      asio::async_write(state->socket, asio::buffer(state->writeQueue.front().data(), state->writeQueue.front().size()), writeLambda);//TODO: ERROR prob starts lambda and pops idk
      state->writeQueue.pop();
   }
   
   void Connection<NetworkMode::TCP>::startRead() {
      if(!state) return;

      asio::post(context.state->io, [st = state]() {
         std::lock_guard guard(st->mutex);
         if (!st->reference || st->reading) return;
         st->reading = true;
         st->reference->doRead();
      });
   }

   void Connection<NetworkMode::TCP>::startWrite() {
      if(!state) return;

      asio::post(context.state->io, [st = state]() {
         if(!st) return;
         std::lock_guard guard(st->mutex);
         if (!st->reference || st->writing) return;
         st->writing = true;
         st->reference->doWrite();
      });
   }

   void Connection<NetworkMode::TCP>::stopRead() {
      if(!state) return;

      asio::post(context.state->io, [st = state]() {
         if(!st) return;
         st->reading = false;
      });
   }

   void Connection<NetworkMode::TCP>::stopWrite() {
      if(!state) return;

      asio::post(context.state->io, [st = state]() {
         if(!st) return;
         st->writing = false;
      });
   }
   /*~TcpConnection*/


   /*UdpServer*/
   Server<NetworkMode::UDP>::Server() {}
   
   Server<NetworkMode::UDP>::Server(Context context_) {
      setContext(context_);
   }

   Server<NetworkMode::UDP>::Server(Server&& other) noexcept : context(std::move(other.context)), state(std::move(other.state)) {
      if(!state) return;
      std::lock_guard guard(state->mutex);
      for (auto& c : state->connections) {//TODO: need mutext guard for server
         c->setServer(this);
      }
      state->reference = this;
   }

   Server<NetworkMode::UDP>& Server<NetworkMode::UDP>::operator=(Server&& other) noexcept {
      if (this != &other) {
         context = std::move(other.context);
         state = std::move(other.state);
         if(!state) return *this;
         std::lock_guard guard(state->mutex);
         for (auto& c : state->connections) {//TODO: need mutext guard for server
            c->setServer(this);
         }
         state->reference = this;
      }

      return *this;
   }
   
   void Server<NetworkMode::UDP>::shutdown() {
      if (!state) return;
   
      std::lock_guard guard(state->mutex);
      state->reference = nullptr;
      for (auto& c : state->connections) {//TODO: need mutext guard for server
         c->setServer(nullptr);
      }
      state->usageGuard.release();
   }

   Server<NetworkMode::UDP>::~Server() {
      shutdown();
   }
   
   void Server<NetworkMode::UDP>::setContext(Context context_) {
      if (!context_.state) {
         std::cerr << "failed to assign context (context state) its a null ptr" << std::endl;
         return;
      }
      context = context_;
      state = std::make_shared<State>(this, context);
   }

   Context Server<NetworkMode::UDP>::getContext() const {
      return context;
   }

   std::vector<std::shared_ptr<Connection<NetworkMode::UDP>>> Server<NetworkMode::UDP>::getConnections() {
      if(!state) std::vector<std::shared_ptr<Connection<NetworkMode::UDP>>>();
      return state->connections;
   }
   
   void Server<NetworkMode::UDP>::start(std::vector<udp::endpoint> endpoints) {
      if(!state) return;
      
      for (auto endpoint: endpoints) {
         state->sockets.push_back(std::make_shared<udp::socket>(context.state->io, endpoint));
      }

      onStart();
   }

   void Server<NetworkMode::UDP>::start(udp::endpoint endpoint) {
      start(std::vector<udp::endpoint>{endpoint});
   }

   void Server<NetworkMode::UDP>::send(const std::vector<uint8_t>& msg) {
      if(!state) return;
      for(auto connection : state->connections) {
         if(!connection) continue;
         connection->send(msg);
      }
   }

   std::shared_ptr<SN::Connection<SN::NetworkMode::UDP>> Server<NetworkMode::UDP>::connect(UdpHandle handle, const std::vector<uint8_t>& msg) {
      if(!state) return nullptr;

      std::shared_ptr<SN::Connection<SN::NetworkMode::UDP>> connection = nullptr;
      for(auto& cIt : state->connections) {
         if(!cIt || !cIt->state) continue;
         if(cIt->state->handle.endpoint == handle.endpoint && cIt->state->handle.socket == handle.socket) {
            connection = cIt;
            break;
         }
      }

      if(!connection) {
         connection = onConnect(handle, msg);
         if(!connection) return nullptr;
         state->connections.emplace_back(connection);
         connection->start();
      }
      return connection;
   }

   void Server<NetworkMode::UDP>::disconnect() {
      if(!state) return;
      state->connections.clear();
      stopRead();
      stopTick();
      state->sockets.clear();
   }

   void Server<NetworkMode::UDP>::removeConnection(Connection<NetworkMode::UDP>* connection) {
      if(!state || !connection) return;
      state->connections.erase(std::remove_if(state->connections.begin(), state->connections.end(), [connection](const auto& c) {
         if(!c) return true;
         return c.get() == connection;
      }), state->connections.end());
   }

   void Server<NetworkMode::UDP>::doRead(std::shared_ptr<udp::socket> socket) {
      if(!state || !socket || !state->reading) return;

      struct Input {
         Input() = delete;
         Input(int size) : readBuffer(size) {}
         udp::endpoint endpoint;
         std::vector<uint8_t> readBuffer;
      };

      auto input = std::make_shared<Input>(4096);

      auto readLambda = [st = state, socket, input](std::error_code ec, std::size_t length) {
         if(!st || !input || !st->reading) return;

         std::shared_ptr<SN::Connection<SN::NetworkMode::UDP>> connection = nullptr;
         for(auto& cIt : st->connections) {
            if(!cIt || !cIt->state) continue;
            if(cIt->state->handle.endpoint == input->endpoint && cIt->state->handle.socket == socket.get()) {
               connection = cIt;
               break;
            }
         }

         if(ec) {
            std::cerr << "failed to read erc: " << ec.message() << std::endl;
            st->reading = false;
            std::lock_guard guard(st->mutex);
            if (!st->reference) return;
            if(connection) connection->disconnect();
            //st->reference->doRead(socket); TODO: ?
            return;
         }
         
         std::lock_guard guard(st->mutex);
         if (!st->reference) {
            st->reading = false;
            return;
         }
         st->reference->doRead(socket);

         std::vector<uint8_t> msg(input->readBuffer.begin(), input->readBuffer.begin() + std::min(length, input->readBuffer.size())); 

         if(!connection) {
            UdpHandle handle(input->endpoint, socket);
            connection = st->reference->onAccept(handle, msg);
            if(!connection) return;
            st->connections.emplace_back(connection);
            connection->start();
         }
         connection->onRead(msg);
      };

      socket->async_receive_from(asio::buffer(input->readBuffer.data(), input->readBuffer.size()), input->endpoint, readLambda);
   }
   
   void Server<NetworkMode::UDP>::startRead() {
      if(!state) return;

      asio::post(context.state->io, [st = state]() {
         if(!st) return;
         std::lock_guard guard(st->mutex);
         if(!st->reference) return;
         if (!st->reference || st->reading) return;
         st->reading = true;
         for (auto socket : st->sockets) {
            st->reference->doRead(socket);
         }
      });
   }

   std::shared_ptr<SN::Connection<SN::NetworkMode::UDP>> Server<NetworkMode::UDP>::onAccept(UdpHandle handle, std::vector<uint8_t> msg) {
      return std::make_shared<SN::Connection<SN::NetworkMode::UDP>>(this, handle);
   }
   
   void Server<NetworkMode::UDP>::stopRead() {
      if(!state) return;

      asio::post(context.state->io, [st = state]() {
         if(!st) return;
         st->reading = false;
      });
   }
   /*~UdpServer*/


   /*UdpConnection*/
   Connection<NetworkMode::UDP>::Connection() {}
   
   Connection<NetworkMode::UDP>::Connection(Server<NetworkMode::UDP>* server, Server<NetworkMode::UDP>::UdpHandle handle) {
      if(!server) return;
      setEnv(server->getContext(), std::move(handle));
      setServer(server);
   }

   Connection<NetworkMode::UDP>::Connection(Connection&& other) noexcept : context(std::move(other.context)), state(std::move(other.state)) {
      if(!state) return;
      std::lock_guard guard(state->mutex);
      state->reference = this;
   }

   Connection<NetworkMode::UDP>& Connection<NetworkMode::UDP>::operator=(Connection&& other) noexcept {
      if (this != &other) {
         context = std::move(other.context);
         state = std::move(other.state);
         if(!state) return *this;
         std::lock_guard guard(state->mutex);
         state->reference = this;
      }

      return *this;
   }

   void Connection<NetworkMode::UDP>::shutdown() {
      if(!state) return;
   
      std::lock_guard guard(state->mutex);
      close();
      state->reference = nullptr;
      state->usageGuard.release();
   }

   Connection<NetworkMode::UDP>::~Connection() {
      shutdown();
   }

   void Connection<NetworkMode::UDP>::setServer(Server<NetworkMode::UDP>* server_) {
      if(!state || !server_) return;
      state->server = server_;
   }
   
   void Connection<NetworkMode::UDP>::setEnv(Context context_, Server<NetworkMode::UDP>::UdpHandle handle) {
      if (!context_.state) {
         std::cerr << "failed to assign context (context state) its a null ptr" << std::endl;
         return;
      }
      context = context_;
      state = std::make_shared<State>(this, context, std::move(handle));
   }
   
   Context Connection<NetworkMode::UDP>::getContext() const {
      return context;
   }

   void Connection<NetworkMode::UDP>::start() {
      onStart();
   }

   void Connection<NetworkMode::UDP>::send(const std::vector<uint8_t>& msg) {
      if(!state) return;
      state->writeQueue.push(msg); //TODO: not 100 thread safe
      startWrite();
   }

   void Connection<NetworkMode::UDP>::disconnect() {
      close();
      std::shared_ptr<State> st = state;
      onDisconnect();
      if(!st || !st->server) return;
      st->server->removeConnection(this);
   }

   void Connection<NetworkMode::UDP>::close() {
      stopWrite();
      stopTick();
   }

   void Connection<NetworkMode::UDP>::doWrite() {
      if(!state || !state->writing || !state->handle.socket) return;

      if(state->writeQueue.empty()) {
         state->writing = false;
         return;
      }

      auto writeLambda = [st = state](std::error_code ec, std::size_t length){
         if(!st || !st->writing) return;
         if(ec) {
            std::cerr << "failed to write erc: " << ec.message() << std::endl;
            st->writing = false;
            return;
         }
         
         std::lock_guard guard(st->mutex);
         if (!st->reference) {
            return;
            st->writing = false;
         }
         st->reference->doWrite();
         st->reference->onWrite();
      };

      state->handle.socket->async_send_to(asio::buffer(state->writeQueue.front().data(), state->writeQueue.front().size()), state->handle.endpoint, writeLambda);
      state->writeQueue.pop();
   }
   
   void Connection<NetworkMode::UDP>::startWrite() {
      if(!state) return;

      asio::post(context.state->io, [st = state]() {
         if(!st) return;
         std::lock_guard guard(st->mutex);
         if (!st->reference || st->writing) return;
         st->writing = true;
         st->reference->doWrite();
      });
   }

   void Connection<NetworkMode::UDP>::stopWrite() {
      if(!state) return;

      asio::post(context.state->io, [st = state]() {
         if(!st) return;
         st->writing = false;
      });
   }
   /*~UdpConnection*/
}
//future dirary