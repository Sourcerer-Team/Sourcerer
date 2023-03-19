#include "LockGuard.h"

#include <Log.h>

#include <chrono>

namespace Util {

Mutex::Mutex() : std::timed_mutex() {}

Mutex::~Mutex() {}

LockGuard::LockGuard(Mutex &mutex, size_t timeout, const QString &function,
                     int line)
    : _mutex(mutex), _timeout(timeout), _function(function), _line(line) {
  const static QString THREAD_ID = "LockGuard";

  size_t totalWaitTime = 0;
  bool lockedAfterSomeTime = false;

  while (!_mutex.try_lock_for(std::chrono::milliseconds(_timeout))) {
    totalWaitTime += _timeout / 1000;

    if (!lockedAfterSomeTime) {
      LOG_ERROR(Log::ErrorId::Generic,
                "Could not lock the mutex in %1 seconds.", _timeout / 1000);
      LOG_ERROR(Log::ErrorId::Generic, "Function: '%1'.", _function);
      LOG_ERROR(Log::ErrorId::Generic, "Line: %1.", _line);
    }

    lockedAfterSomeTime = true;
  }

  if (lockedAfterSomeTime) {
    LOG_ERROR(Log::ErrorId::Generic,
              "The mutex has been sucesfully locked after %1 seconds.",
              totalWaitTime);
    LOG_ERROR(Log::ErrorId::Generic, "Function: '%1'.", _function);
    LOG_ERROR(Log::ErrorId::Generic, "Line: %1.", _line);
  }
}

LockGuard::~LockGuard() { _mutex.unlock(); }

} // namespace Util
