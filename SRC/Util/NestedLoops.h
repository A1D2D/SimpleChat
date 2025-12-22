#ifndef NESTEDLOOPS_H
#define NESTEDLOOPS_H

#include <cstdint>
#define NL_BREAK(CTX, N) \
CTX.breakLoop(N); \
break

#define NL_CONTINUE(CTX, N) \
CTX.continueLoop(N); \
break

#define NL_CHECK(CTX, N) \
if (CTX.shouldBreak(N)) break; \
if (CTX.shouldContinue(N)) continue

namespace SN {
   struct NestedLoop {
      char action = 0;
      uint32_t state = 0;
      bool shouldBreak(uint32_t level) {
         if (action == 1 && state <= level) return state == level ? action = 0, true : true;
         if (action == 2 && state < level) return true;
         if (state == level) action = 0;
         return false;
      }
      bool shouldContinue(uint32_t level) {
         return state == level && action == 2 && (action = 0, true);
      }
      void breakLoop(uint32_t level) { state = level; action = 1; }
      void continueLoop(uint32_t level) { state = level; action = 2; }
   };
}


#endif //NESTEDLOOPS_H