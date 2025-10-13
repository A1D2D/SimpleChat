#ifndef NETWORK_NET_ONE_WAY_LOCK_TEMPLATED_H
#define NETWORK_NET_ONE_WAY_LOCK_TEMPLATED_H

#include <atomic>
#include <thread>

namespace SN {
   class OWLock {
   //0 noLock
   //1 lockedByBlocking
   //1> lockedByContext
   public:
      std::atomic<unsigned int> lockID = 0;

      void lockHold() {
         if(lockID != 0) {
         }
      }

      bool lockSkip() {
         return 0;
      }

      void release() {

      }
   };
}

#endif //~NETWORK_NET_ONE_WAY_LOCK_TEMPLATED_H