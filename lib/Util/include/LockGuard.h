#ifndef LOCKGUARD_H
#define LOCKGUARD_H

#include <QString>

#include <mutex>

using namespace std::chrono_literals;

namespace Util {

constexpr size_t MAX_LOCK_WAIT_TIME = 60000; // 60 secs

class Mutex : public std::timed_mutex {
public:
  Mutex();

  ~Mutex();

  uint64_t mutexId() const { return reinterpret_cast<uint64_t>(this); }
};

class RecursiveMutex : public std::recursive_timed_mutex {
public:
  RecursiveMutex();

  ~RecursiveMutex();

  uint64_t mutexId() const { return reinterpret_cast<uint64_t>(this); }
};

void printDetailsError(const QString &function, const int line,
                       const uint64_t mutexId, const size_t waitTime);

void printDetailsSuccess(const QString &function, const int line,
                         const uint64_t mutexId, const size_t waitTime);

template <class MutexType> class LockGuard {
public:
  LockGuard(MutexType &mutex, const size_t timeout, const QString &function,
            const int line)
      : _mutex(mutex), _timeout(timeout), _function(function), _line(line) {
    static const char *const THREAD_ID = "LockGuard";

    size_t totalWaitTime = 0;
    bool lockedAfterSomeTime = false;

    while (!_mutex.try_lock_for(std::chrono::milliseconds(_timeout))) {
      totalWaitTime += _timeout / 1000;

      if (!lockedAfterSomeTime) {
        printDetailsError(_function, _line, _mutex.mutexId(), totalWaitTime);
      }

      lockedAfterSomeTime = true;
    }

    if (lockedAfterSomeTime) {
      printDetailsSuccess(_function, _line, _mutex.mutexId(), totalWaitTime);
    }
  }

  ~LockGuard() { _mutex.unlock(); }

private:
  MutexType &_mutex;
  size_t _timeout;
  const QString &_function;
  int _line;
};

#define LOCKGUARD(x)                                                           \
  const Util::LockGuard __guard__(x, Util::MAX_LOCK_WAIT_TIME,                 \
                                  __PRETTY_FUNCTION__, __LINE__)

} // namespace Util

#endif // LOCKGUARD_H
