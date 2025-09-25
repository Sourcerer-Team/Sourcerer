#include "LockGuard.h"

#include <Log.h>

#include <chrono>

#define THREAD_ID "LockGuard"

namespace Util {

Mutex::Mutex() : std::timed_mutex() {}

Mutex::~Mutex() {}

RecursiveMutex::RecursiveMutex() : std::recursive_timed_mutex() {}

RecursiveMutex::~RecursiveMutex() {}

void printDetailsError(const QString &function, const int line,
                  const uint64_t mutexId, const size_t waitTime) {
  LOG_ERROR(Log::ErrorId::Generic,
            "Could not lock the mutex in {} seconds.", waitTime / 1000);
  LOG_ERROR(Log::ErrorId::Generic, "Function: '{}'.", function);
  LOG_ERROR(Log::ErrorId::Generic, "Line: {}.", line);
  LOG_ERROR(Log::ErrorId::Generic, "mutex id: {}", mutexId);
}

void printDetailsSuccess(const QString &function, const int line,
                         const uint64_t mutexId, const size_t waitTime) {
  LOG_ERROR(Log::ErrorId::Generic,
            "The mutex {} has been successfully locked after {} seconds.",
            mutexId, waitTime);
  LOG_ERROR(Log::ErrorId::Generic, "Function: '{}'.", function);
  LOG_ERROR(Log::ErrorId::Generic, "Line: {}.", line);
}


} // namespace Util
