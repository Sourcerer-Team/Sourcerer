#ifndef LOCKGUARD_H
#define LOCKGUARD_H

#include <mutex>
#include <string>

using namespace std::chrono_literals;

namespace Util
{

const size_t MAX_LOCK_WAIT_TIME = 60000; // 60 secs

class Mutex : public std::timed_mutex
{
public:
   Mutex();

   ~Mutex();
};

class LockGuard
{
public:
   LockGuard(Mutex& mutex, size_t timeout, const std::string& function, int line);

   ~LockGuard();

private:
   Mutex& _mutex;
   size_t _timeout;
   const std::string& _function;
   int _line;
};

#define LOCKGUARD(x) const Util::LockGuard __guard__(x, Util::MAX_LOCK_WAIT_TIME, __PRETTY_FUNCTION__, __LINE__)

}

#endif // LOCKGUARD_H
