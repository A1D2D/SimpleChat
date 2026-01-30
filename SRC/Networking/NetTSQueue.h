#ifndef NCORE_THREAD_SAFE_QUEUE_TEMPLATED_H
#define NCORE_THREAD_SAFE_QUEUE_TEMPLATED_H

#include <mutex>
#include <queue>

namespace SN {
   template<typename T>
   class TSQueue {
   public:
      TSQueue() : queueMutex(std::make_unique<std::mutex>()) {}
      TSQueue(const TSQueue&) = delete;
      TSQueue& operator=(const TSQueue&) = delete;

      TSQueue(TSQueue&&) noexcept = default;
      TSQueue& operator=(TSQueue&&) noexcept = default;

      bool empty() const {
         std::scoped_lock lock(*queueMutex);
         return queueData.empty();
      }

      unsigned int size() const {
         std::scoped_lock lock(*queueMutex);
         return queueData.size();
      }

      const T& front() {
         std::scoped_lock lock(*queueMutex);
         return queueData.front();
      }

      void push(const T& val) {
         std::scoped_lock lock(*queueMutex);
         queueData.push(val);
      }

      void push(const T&& val) {
         std::scoped_lock lock(*queueMutex);
         queueData.push(std::move(val));
      }

      void pop() {
         std::scoped_lock lock(*queueMutex);
         queueData.pop();
      }
      
   protected:
      mutable std::unique_ptr<std::mutex> queueMutex;
      std::queue<T> queueData;
   };
}

#endif //~NCORE_THREAD_SAFE_QUEUE_TEMPLATED_H