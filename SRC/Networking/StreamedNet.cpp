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
   
   Context::UsageGuard::UsageGuard(Context context) : context(std::move(context)) {
      this->context.use();
   }

   Context::UsageGuard& Context::UsageGuard::operator=(UsageGuard&& other) noexcept {
      if (this != &other) {
         release();
         context = std::move(other.context);
         active = std::exchange(other.active, false);
      }
      return *this;
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

   Context::UsageGuard::UsageGuard(UsageGuard&& other) noexcept : context(std::move(other.context)), active(std::exchange(other.active, false)) {}

   Context::Callback::Callback(Context context, std::function<void()> callback) : context(std::move(context)) {
      handle = this->context.addCallback(
         std::move(callback)
      );
   }

   Context::Callback::~Callback() {
      remove();
   }

   Context::Callback::Callback(Callback&& other) noexcept : context(std::move(other.context)), handle(std::move(other.handle)), active(std::exchange(other.active, false)) {}

   Context::Callback& Context::Callback::operator=(Callback&& other) noexcept {
      if (this != &other) {
         remove();
         context = std::move(other.context);
         handle = std::move(other.handle);
         active = std::exchange(other.active, false);
      }
      return *this;
   }

   void Context::Callback::remove() {
      if (active) {
         context.removeCallback(handle);
         active = false;
      }
   }
   /*~Context*/


   /*Resolver*/
   Resolver::Resolver() {}

   Resolver::Resolver(Context context_) {
      setContext(context_);
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

   Resolver::Resolver(Resolver&& other) noexcept : context(std::move(other.context)), state(std::move(other.state)) {
      if(!state) return;
      std::lock_guard guard(state->mutex);
      state->reference = this;
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
         
         st->reference->onTcpResolve(resultEndpoints);
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

         st->reference->onUdpResolve(resultEndpoints);
      };

      state->udpResolver.async_resolve(host, std::to_string(port), resolveLambda);
   }
   
   void Resolver::doTick() {
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
}