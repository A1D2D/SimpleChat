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
   
      std::lock_guard guard(state->mutex);
      state->reference = nullptr;
      state->usageGuard.release();
   }

   Resolver::~Resolver() {
      shutdown();
   }

   void Resolver::setContext(Context context_) {
      context = context_;
      if (!context.state) {
         std::cerr << "failed to assign context (context state) its a null ptr" << std::endl;
         return;
      }
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
   
   void Resolver::startTick() {
      if(!state || state->callback) return;
      state->callback.emplace(Context::Callback(context, [st = state](){
         if(!st) return;

         std::lock_guard guard(st->mutex);
         if (!st->reference) return;
         st->reference->onTick();
      }));
   }

   void Resolver::stopTick() {
      if(!state || !state->callback) return;
      state->callback.reset();
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
      state->reference = nullptr;
      state->usageGuard.release();
   }

   Client<NetworkMode::TCP>::~Client() {
      shutdown();
   }

   void Client<NetworkMode::TCP>::setContext(Context context_) {
      context = context_;
      if (!context.state) {
         std::cerr << "failed to assign context (context state) its a null ptr" << std::endl;
         return;
      }
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

   void Client<NetworkMode::TCP>::disconnect() { //TODO: more sofisticated closing
      if(!state) return;

      state->reading = false;

      if(state->socket.is_open()) {
         asio::error_code ec;
         ec = state->socket.shutdown(tcp::socket::shutdown_both, ec);
         if(ec) std::cerr << "failed to shutdown socket erc: " << ec.message();
         ec = state->socket.close(ec);
         if(ec) std::cerr << "failed to close socket erc: " << ec.message();

         onDisconnect();
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
            st->reference->onDisconnect();
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

   void Client<NetworkMode::TCP>::startTick() {
      if(!state || state->callback) return;
      state->callback.emplace(Context::Callback(context, [st = state](){
         if(!st) return;

         std::lock_guard guard(st->mutex);
         if (!st->reference) return;
         st->reference->onTick();
      }));
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

   void Client<NetworkMode::TCP>::stopTick() {
      if(!state || !state->callback) return;
      state->callback.reset();
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
      if (!state) return;
   
      std::lock_guard guard(state->mutex);
      state->reference = nullptr;
      state->usageGuard.release();
   }

   Client<NetworkMode::UDP>::~Client() {
      shutdown();
   }

   void Client<NetworkMode::UDP>::setContext(Context context_) {
      context = context_;
      if (!context.state) {
         std::cerr << "failed to assign context (context state) its a null ptr" << std::endl;
         return;
      }
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

   void Client<NetworkMode::UDP>::disconnect() { //TODO: more sofisticated closing
      if(!state) return;

      state->reading = false;

      if(state->socket.is_open()) {
         asio::error_code ec;
         ec = state->socket.shutdown(tcp::socket::shutdown_both, ec);
         if(ec) std::cerr << "failed to shutdown socket erc: " << ec.message();
         ec = state->socket.close(ec);
         if(ec) std::cerr << "failed to close socket erc: " << ec.message();

         onDisconnect();
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
            st->reference->onDisconnect();
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

   void Client<NetworkMode::UDP>::startTick() {
      if(!state || state->callback) return;
      state->callback.emplace(Context::Callback(context, [st = state](){
         if(!st) return;

         std::lock_guard guard(st->mutex);
         if (!st->reference) return;
         st->reference->onTick();
      }));
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

   void Client<NetworkMode::UDP>::stopTick() {
      if(!state || !state->callback) return;
      state->callback.reset();
   }
   /*~UdpClient*/
}