#include "LockGuard.h"

#include <Log.h>

#include <chrono>

namespace Util
{

Mutex::Mutex()
   : std::timed_mutex()
{

}

Mutex::~Mutex()
{

}

LockGuard::LockGuard(Mutex& mutex, size_t timeout, const std::string& function, int line)
   : _mutex(mutex)
   , _timeout(timeout)
   , _function(function)
   , _line(line)
{
   const static std::string THREAD_ID = "LockGuard";

   size_t totalWaitTime = 0;
   bool lockedAfterSomeTime = false;

   while(!_mutex.try_lock_for(std::chrono::milliseconds(_timeout)))
   {
      totalWaitTime += _timeout / 1000;

      if(!lockedAfterSomeTime)
      {
         LOG_ERROR(Log::ErrorId::Generic, "Could not lock the mutex in {} seconds.", _timeout / 1000);
         LOG_ERROR(Log::ErrorId::Generic, "Function: '{}'.", _function);
         LOG_ERROR(Log::ErrorId::Generic, "Line: {}.", _line);
      }

      lockedAfterSomeTime = true;
   }

   if(lockedAfterSomeTime)
   {
      LOG_ERROR(Log::ErrorId::Generic, "The mutex has been sucesfully locked after {} seconds.", totalWaitTime);
      LOG_ERROR(Log::ErrorId::Generic, "Function: '{}'.", _function);
      LOG_ERROR(Log::ErrorId::Generic, "Line: {}.", _line);
   }
}

LockGuard::~LockGuard()
{
   _mutex.unlock();
}

}
