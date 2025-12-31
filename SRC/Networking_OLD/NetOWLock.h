#ifndef NCORE_ONE_WAY_LOCK_TEMPLATED_H
#define NCORE_ONE_WAY_LOCK_TEMPLATED_H

#include <atomic>
#include <thread>

namespace SN {
   class OWLock {
   public:
      OWLock() : counter(0), destroying(false) {}

      bool try_acquire() noexcept {
         if (tl_recursion_count > 0) {
            ++tl_recursion_count;
            return true;
         }

         if (destroying.load(std::memory_order_acquire)) {
            return false;
         }

         counter.fetch_add(1, std::memory_order_acquire);

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
         
         while (counter.load(std::memory_order_acquire) > tl_recursion_count) {
            std::this_thread::yield();
         }
      }

      OWLock(const OWLock&) = delete;
      OWLock& operator=(const OWLock&) = delete;

   private:
      std::atomic<unsigned int> counter;
      std::atomic<bool> destroying;
      inline static thread_local unsigned int tl_recursion_count = 0;
   };

   class OWLockGuard {
   public:
      explicit OWLockGuard(OWLock& lock) : lock_(&lock), acquired_(false) {
         acquired_ = lock_->try_acquire();
      }

      OWLockGuard(OWLockGuard&& other) noexcept : lock_(other.lock_), acquired_(other.acquired_) {
         other.lock_ = nullptr;
         other.acquired_ = false;
      }

      ~OWLockGuard() {
         if (acquired_) {
            lock_->release();
         }
      }

      OWLockGuard& operator=(OWLockGuard&& other) noexcept {
         if (this != &other) {
            if (acquired_) {
               lock_->release();
            }

            lock_ = other.lock_;
            acquired_ = other.acquired_;

            other.lock_ = nullptr;
            other.acquired_ = false;
         }
         return *this;
      }

      explicit operator bool() const noexcept { return acquired_; }
      bool acquired() const noexcept { return acquired_; }

      OWLockGuard(const OWLockGuard&) = delete;

   private:
      OWLock* lock_ = nullptr;
      bool acquired_ = false;
   };

   class OWLockRelease {
   public:
      explicit OWLockRelease(OWLock& lock) : lock_(&lock) {}

      OWLockRelease(OWLockRelease&& other) noexcept : lock_(other.lock_) {
         other.lock_ = nullptr;
      }

      ~OWLockRelease() {
         lock_->release();
      }

      OWLockRelease& operator=(OWLockRelease&& other) noexcept {
         if (this != &other) {
            lock_->release();

            lock_ = other.lock_;

            other.lock_ = nullptr;
         }
         return *this;
      }

      OWLockRelease(const OWLockRelease&) = delete;

   private:
      OWLock* lock_ = nullptr;
   };
}

#endif //~NCORE_ONE_WAY_LOCK_TEMPLATED_H