#ifndef NCORE_ONE_WAY_LOCK_TEMPLATED_H
#define NCORE_ONE_WAY_LOCK_TEMPLATED_H

#include <atomic>
#include <thread>

namespace SN {
   class MoveGuard {
   public:
      MoveGuard() = default;
      
      MoveGuard(MoveGuard&& other) noexcept : owns(other.owns) {
         other.owns = false; 
      }

      MoveGuard& operator=(MoveGuard&& other) noexcept {
         if (this != &other) {
            owns = other.owns;
            other.owns = false;
         }
         return *this;
      }

      void set(bool moved) {
         owns = !moved;
      }

      MoveGuard(const MoveGuard&) = delete;
      MoveGuard& operator=(const MoveGuard&) = delete;

      explicit operator bool() const noexcept { return owns; }
   private:
      bool owns = true;
   };
}

#endif //~NCORE_ONE_WAY_LOCK_TEMPLATED_H