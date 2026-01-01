#ifndef NCORE_IOCONTEXT_CONTROLLER_H
#define NCORE_IOCONTEXT_CONTROLLER_H

#include <thread>
#include <memory>

#include "../Util/AsioInclude.h"

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
               NCore_Log("object handle destroyed\n")
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

   class IOContextRunner {
   public:
      enum class Mode {
         ExternalShared,
         ExternalRaw,
         InternalOwned
      };

      IOContextRunner(const IOContextRunner&) = delete;
      IOContextRunner& operator=(const IOContextRunner&) = delete;

      IOContextRunner(std::shared_ptr<std::thread> ctx) : mode(Mode::ExternalShared), shared(ctx), raw(ctx.get()) {}

      IOContextRunner(std::thread* ctx) : mode(Mode::ExternalRaw), raw(ctx) {}

      IOContextRunner(std::thread& ctx) : mode(Mode::ExternalRaw), raw(&ctx) {}

      IOContextRunner() : mode(Mode::InternalOwned), owned(std::make_unique<std::thread>()), raw(owned.get()) {}
      
      IOContextRunner(IOContextRunner&& other) noexcept : mode(other.mode), shared(std::move(other.shared)), owned(std::move(other.owned)), raw(other.raw) {
         other.reset();
      }

      IOContextRunner& operator=(IOContextRunner&& other) noexcept {
         if (this != &other) {
            if (raw) {
               if(mode == Mode::InternalOwned) {
                  stopThread();
                  NCore_Log("INTER: ")
               }
               NCore_Log("object runner destroyed\n")
            }

            mode = other.mode;
            shared = std::move(other.shared);
            owned = std::move(other.owned);
            raw = other.raw;

            other.reset();
         }
         return *this;
      }

      std::thread& get() { return *raw; }
      std::thread& operator*() { return *raw; }
      std::thread* operator->() { return raw; }
      const std::thread* operator->() const { return raw; }
      std::thread* ptr() { return raw; }
      const std::thread* ptr() const { return raw; }

      void startThread(asio::io_context* context) {
         if (!threadRunning) {
            threadRunning = true;
            get() = std::thread([](asio::io_context* context){
               context->run();
               NCore_Log("work done\n")
            }, context);
         }
      }

      void stopThread() {
         if (threadRunning) {
            if (get().joinable()) get().join();
            threadRunning = false;
         }
      }

      void reset() noexcept {
         raw = nullptr;
         shared.reset();
         owned.reset();
      }

      ~IOContextRunner() {
         if (raw) {
            if(mode == Mode::InternalOwned) {
               stopThread();
               NCore_Log("INTER: ")
            }
            NCore_Log("object runner destroyed\n")
         }
      }

   public:
      Mode mode;

   private:
      std::shared_ptr<std::thread> shared;
      std::unique_ptr<std::thread> owned;
      std::thread* raw = nullptr;
      bool threadRunning = false;
   };

   class IOContextController {
   public:
      IOContextController(const IOContextController&) = delete;
      IOContextController& operator=(const IOContextController&) = delete;

      IOContextController(IOContextController&&) = default;
      IOContextController& operator=(IOContextController&&) = default;

      IOContextController(SN::IOContextHandle&& handle, SN::IOContextRunner&& runner);
      IOContextController(SN::IOContextHandle&& handle);
      IOContextController(SN::IOContextRunner&& runner);
      IOContextController();

      SN::IOContextHandle handle;
      SN::IOContextRunner runner;
   };

   inline SN::IOContextController::IOContextController(SN::IOContextHandle&& handle_, SN::IOContextRunner&& runner_) : handle(std::move(handle_)), runner(std::move(runner_)) {}

   inline SN::IOContextController::IOContextController(SN::IOContextHandle&& handle_) : handle(std::move(handle_)), runner(SN::IOContextRunner()) {}

   inline SN::IOContextController::IOContextController(SN::IOContextRunner&& runner_) : handle(SN::IOContextHandle()), runner(std::move(runner_)) {}

   inline SN::IOContextController::IOContextController() : handle(SN::IOContextHandle()), runner(SN::IOContextRunner()) {}
}
#endif //~NCORE_IOCONTEXT_CONTROLLER_H