#ifndef NETWORK_NET_ONE_WAY_LOCK_TEMPLATED_H
#define NETWORK_NET_ONE_WAY_LOCK_TEMPLATED_H

#include <atomic>
#include <thread>

namespace SN {
   class OWLock {
   public:
      OWLock() : counter(0), destroying(false) {}

      bool try_acquire() noexcept {
         // Fast path: reentrant acquisition
         if (tl_recursion_count > 0) {
            ++tl_recursion_count;
            return true;
         }

         // Check if destruction started
         if (destroying.load(std::memory_order_acquire)) {
            return false;
         }

         // Increment counter
         counter.fetch_add(1, std::memory_order_acquire);

         // Double-check destruction didn't start
         if (destroying.load(std::memory_order_acquire)) {
            counter.fetch_sub(1, std::memory_order_release);
            return false;
         }

         tl_recursion_count = 1;
         return true;
      }

      void release() noexcept {
         if (tl_recursion_count > 1) {
            --tl_recursion_count;
         } else if (tl_recursion_count == 1) {
            tl_recursion_count = 0;
            counter.fetch_sub(1, std::memory_order_release);
         } else {
            std::terminate();
         }
      }

      void begin_destroy_and_wait() noexcept {
         destroying.store(true, std::memory_order_release);
         
         while (counter.load(std::memory_order_acquire) > 0) {
            std::this_thread::yield();
         }
      }

      OWLock(const OWLock&) = delete;
      OWLock& operator=(const OWLock&) = delete;

   private:
      std::atomic<unsigned int> counter;
      std::atomic<bool> destroying;
      static thread_local unsigned int tl_recursion_count;
   };

   class OWLockGuard {
   public:
      explicit OWLockGuard(OWLock& lock) : lock_(&lock), acquired_(false) {
         acquired_ = lock_->try_acquire();
      }

      ~OWLockGuard() {
         if (acquired_) {
            lock_->release();
         }
      }

      // Check if acquisition succeeded
      explicit operator bool() const noexcept {
         return acquired_;
      }

      bool acquired() const noexcept {
         return acquired_;
      }

      // Non-copyable, non-movable
      OWLockGuard(const OWLockGuard&) = delete;
      OWLockGuard& operator=(const OWLockGuard&) = delete;

   private:
      OWLock* lock_;
      bool acquired_;
   };
}

#endif //~NETWORK_NET_ONE_WAY_LOCK_TEMPLATED_H