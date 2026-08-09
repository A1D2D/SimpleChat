#ifndef NCORE_IOCONTEXT_CONTROLLER_H
#define NCORE_IOCONTEXT_CONTROLLER_H

#include <memory>

#include "AsioInclude.h"

namespace SN {
   class IOContextHandle {
   public:
      enum class Mode {
         ExternalShared,
         ExternalRaw,
         InternalOwned
      };

      IOContextHandle(const IOContextHandle&) = delete;
      IOContextHandle& operator=(const IOContextHandle&) = delete;

      IOContextHandle(std::shared_ptr<asio::io_context> ctx) : mode(Mode::ExternalShared), shared(ctx), raw(ctx.get()) {}

      IOContextHandle(asio::io_context* ctx) : mode(Mode::ExternalRaw), raw(ctx) {}

      IOContextHandle(asio::io_context& ctx) : mode(Mode::ExternalRaw), raw(&ctx) {}

      IOContextHandle() : mode(Mode::InternalOwned), owned(std::make_unique<asio::io_context>()), raw(owned.get()) {}

      IOContextHandle(IOContextHandle&& other) noexcept : mode(other.mode), shared(std::move(other.shared)), owned(std::move(other.owned)), raw(other.raw) {
         other.reset();
      }

      IOContextHandle& operator=(IOContextHandle&& other) noexcept {
         if (this != &other) {
            if (raw) {
               if(mode == Mode::InternalOwned) {
                  raw->stop();
                  NCore_Log("INTER: ")
               }
               NCore_Log("IOContextHandle: handle destroyed\n")
            }

            mode = other.mode;
            shared = std::move(other.shared);
            owned = std::move(other.owned);
            raw = other.raw;

            other.reset();
         }
         return *this;
      }

      asio::io_context& get() { return *raw; }
      asio::io_context& operator*() { return *raw; }
      asio::io_context* operator->() { return raw; }
      const asio::io_context* operator->() const { return raw; }
      asio::io_context* ptr() { return raw; }
      const asio::io_context* ptr() const { return raw; }

      void reset() noexcept {
         raw = nullptr;
         shared.reset();
         owned.reset();
      }

      ~IOContextHandle() {
         if(!raw) return;
         if(mode == Mode::InternalOwned) {
            raw->stop();
            NCore_Log("INTER: ")
         }
         NCore_Log("object handle destroyed\n")
      }

   public:
      Mode mode;

   private:
      std::shared_ptr<asio::io_context> shared;
      std::unique_ptr<asio::io_context> owned;
      asio::io_context* raw = nullptr;
   };

   class ExecutionContext {
   public:
      ExecutionContext() : context(std::make_shared<asio::io_context>()), runningGuard(std::make_shared<int>(0)) {}

      void addGuard() {
         (*runningGuard)++;
      }
      void removeGuard() {
         (*runningGuard)--;
      }
   
      std::shared_ptr<int> runningGuard; // died = 0, died < 0 -> error
      std::shared_ptr<asio::io_context> context;
      std::vector<std::function<void()>> callbacks;
   };
}
#endif //~NCORE_IOCONTEXT_CONTROLLER_H