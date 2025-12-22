#ifndef NETWORK_NET_IOCONTEXT_CONTROLLER_H
#define NETWORK_NET_IOCONTEXT_CONTROLLER_H

#include <thread>
#include <memory>

#ifdef _WIN32
   #undef WINAPI_FAMILY
   #define WIN32_WINNT 0x0A00
#endif

#include <asio.hpp>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>

namespace SN {
   class IOContextController {
   public:
      enum class Mode {
         ExternalShared,
         ExternalRaw,
         InternalOwned
      };

      IOContextController(IOContextController&&) = default;
      IOContextController& operator=(IOContextController&&) = default;

      IOContextController(const IOContextController&) = delete;
      IOContextController& operator=(const IOContextController&) = delete;

      IOContextController(std::shared_ptr<asio::io_context> ctx) : mode(Mode::ExternalShared), shared(ctx), raw(ctx.get()) {}

      IOContextController(asio::io_context* ctx) : mode(Mode::ExternalRaw), raw(ctx) {}

      IOContextController(asio::io_context& ctx) : mode(Mode::ExternalRaw), raw(&ctx) {}

      IOContextController() : mode(Mode::InternalOwned), owned(std::make_unique<asio::io_context>()), raw(owned.get()) {}

      asio::io_context& get() { return *raw; }
      asio::io_context& operator*() { return *raw; }
      asio::io_context* operator->() { return raw; }
      const asio::io_context* operator->() const { return raw; }
      asio::io_context* ptr() { return raw; }
      const asio::io_context* ptr() const { return raw; }

      void startThread() {
         if (!threadRunning) {
            threadRunning = true;
            thread = std::thread([&]() {
               get().run();
            });
         }
      }

      void stopThread() {
         if (threadRunning) {
            get().stop();
            if (thread.joinable()) thread.join();
            threadRunning = false;
         }
      }

      ~IOContextController() {
         // stopThread();
      }

   private:
      Mode mode;

      std::shared_ptr<asio::io_context> shared;
      std::unique_ptr<asio::io_context> owned;
      asio::io_context* raw = nullptr;

      std::thread thread;
      bool threadRunning = false;
   };
}
#endif//~NETWORK_NET_IOCONTEXT_CONTROLLER_H