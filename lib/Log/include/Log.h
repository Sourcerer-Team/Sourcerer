#pragma once

#include <LockGuard.h>

#include <spdlog/fmt/fmt.h>

#include <chrono>
#include <string>
#include <exception>
#include <functional>
#include <map>
#include <mutex>

#ifndef LOG_SINGLE_THREAD_APPLICATION

#define LOG_INFO(...)   {Log::Log::getInstance().info(THREAD_ID, __PRETTY_FUNCTION__, __LINE__, __VA_ARGS__);}
#define LOG_DEBUG(...)   {Log::Log::getInstance().debug(THREAD_ID, __PRETTY_FUNCTION__, __LINE__, __VA_ARGS__);}
#define LOG_WARN(...)    {Log::Log::getInstance().warning(THREAD_ID, __PRETTY_FUNCTION__, __LINE__, Log::ErrorId::None, __VA_ARGS__);}
#define LOG_WARN_ID(id, ...)    {Log::Log::getInstance().warning(THREAD_ID, __PRETTY_FUNCTION__, __LINE__, id, __VA_ARGS__);}
#define LOG_PERF(duration, ...)    {Log::Log::getInstance().perf(THREAD_ID, __PRETTY_FUNCTION__, __LINE__, duration, __VA_ARGS__);}
#define LOG_ERROR(id, ...)   {Log::Log::getInstance().error(THREAD_ID, __PRETTY_FUNCTION__, __LINE__, id, nullptr, __VA_ARGS__);}
#define LOG_ERROR_EXC(id, exception, ...)   {Log::Log::getInstance().error(THREAD_ID, __PRETTY_FUNCTION__, __LINE__, id, exception, __VA_ARGS__);}
#define LOG_INIT(...)   {Log::Log::getInstance().init(THREAD_ID, __PRETTY_FUNCTION__, __LINE__, __VA_ARGS__);}

#define LOG_INFO_CORRELATION_ID(corelationId, ...)   {Log::Log::getInstance().infoCorrelationId(THREAD_ID, __PRETTY_FUNCTION__, __LINE__, corelationId, __VA_ARGS__);}
#define LOG_DEBUG_CORRELATION_ID(corelationId, ...)   {Log::Log::getInstance().debugCorrelationId(THREAD_ID, __PRETTY_FUNCTION__, __LINE__, corelationId, __VA_ARGS__);}
#define LOG_WARN_CORRELATION_ID(corelationId, ...)    {Log::Log::getInstance().warningCorrelationId(THREAD_ID, __PRETTY_FUNCTION__, __LINE__, Log::ErrorId::None, corelationId, __VA_ARGS__);}
#define LOG_WARN_ID_CORRELATION_ID(id, corelationId, ...)    {Log::Log::getInstance().warningCorrelationId(THREAD_ID, __PRETTY_FUNCTION__, __LINE__, id, corelationId, __VA_ARGS__);}
#define LOG_PERF_CORRELATION_ID(duration, corelationId, ...)    {Log::Log::getInstance().perfCorrelationId(THREAD_ID, __PRETTY_FUNCTION__, __LINE__, duration, corelationId, __VA_ARGS__);}
#define LOG_ERROR_CORRELATION_ID(id, corelationId, ...)   {Log::Log::getInstance().errorCorrelationId(THREAD_ID, __PRETTY_FUNCTION__, __LINE__, id, nullptr, corelationId, __VA_ARGS__);}
#define LOG_ERROR_EXC_CORRELATION_ID(id, exception, corelationId, ...)   {Log::Log::getInstance().errorCorrelationId(THREAD_ID, __PRETTY_FUNCTION__, __LINE__, id, exception, corelationId, __VA_ARGS__);}
#define LOG_INIT_CORRELATION_ID(corelationId, ...)   {Log::Log::getInstance().initCorrelationId(THREAD_ID, __PRETTY_FUNCTION__, __LINE__, corelationId, __VA_ARGS__);}

#else

#define LOG_INFO(...)   {Log::Log::getInstance().info("", __PRETTY_FUNCTION__, __LINE__, __VA_ARGS__);}
#define LOG_DEBUG(...)   {Log::Log::getInstance().debug("", __PRETTY_FUNCTION__, __LINE__, __VA_ARGS__);}
#define LOG_WARN(...)    {Log::Log::getInstance().warning("", __PRETTY_FUNCTION__, __LINE__, Log::ErrorId::None, __VA_ARGS__);}
#define LOG_WARN_ID(id, ...)    {Log::Log::getInstance().warning("", __PRETTY_FUNCTION__, __LINE__, id, __VA_ARGS__);}
#define LOG_PERF(duration, ...)    {Log::Log::getInstance().perf("", __PRETTY_FUNCTION__, __LINE__, duration, __VA_ARGS__);}
#define LOG_ERROR(id, ...)   {Log::Log::getInstance().error("", __PRETTY_FUNCTION__, __LINE__, id, nullptr, __VA_ARGS__);}
#define LOG_ERROR_EXC(id, exception, ...)   {Log::Log::getInstance().error("", __PRETTY_FUNCTION__, __LINE__, id, exception, __VA_ARGS__);}
#define LOG_INIT(...)   {Log::Log::getInstance().init("", __PRETTY_FUNCTION__, __LINE__, __VA_ARGS__);}

#define LOG_INFO_CORRELATION_ID(corelationId, ...)   {Log::Log::getInstance().infoCorrelationId("", __PRETTY_FUNCTION__, __LINE__, corelationId, __VA_ARGS__);}
#define LOG_DEBUG_CORRELATION_ID(corelationId, ...)   {Log::Log::getInstance().debugCorrelationId("", __PRETTY_FUNCTION__, __LINE__, corelationId, __VA_ARGS__);}
#define LOG_WARN_CORRELATION_ID(corelationId, ...)    {Log::Log::getInstance().warningCorrelationId("", __PRETTY_FUNCTION__, __LINE__, Log::ErrorId::None, corelationId, __VA_ARGS__);}
#define LOG_WARN_ID_CORRELATION_ID(id, corelationId, ...)    {Log::Log::getInstance().warningCorrelationId("", __PRETTY_FUNCTION__, __LINE__, id, corelationId, __VA_ARGS__);}
#define LOG_PERF_CORRELATION_ID(duration, corelationId, ...)    {Log::Log::getInstance().perfCorrelationId("", __PRETTY_FUNCTION__, __LINE__, duration, corelationId, __VA_ARGS__);}
#define LOG_ERROR_CORRELATION_ID(id, corelationId, ...)   {Log::Log::getInstance().errorCorrelationId("", __PRETTY_FUNCTION__, __LINE__, id, nullptr, corelationId, __VA_ARGS__);}
#define LOG_ERROR_EXC_CORRELATION_ID(id, exception, corelationId, ...)   {Log::Log::getInstance().errorCorrelationId("", __PRETTY_FUNCTION__, __LINE__, id, exception, corelationId, __VA_ARGS__);}
#define LOG_INIT_CORRELATION_ID(corelationId, ...)   {Log::Log::getInstance().initCorrelationId("", __PRETTY_FUNCTION__, __LINE__, corelationId, __VA_ARGS__);}


#endif

namespace Log
{

enum ErrorId
{
   None = 0,
   Generic = -1,
   FileSystem = -2,
   Network = -3,
   Database = -4,
   Gui = -5,
   DateTime = -6,
   Logic = -7,
   Config = -8
};

class Log
{
   Log();

public:

   static Log& getInstance();

   // Log writing
   void info(std::string threadId, std::string function, int line, std::string message);
   void error(std::string threadId, std::string function, int line, ErrorId idError, const std::exception* exception, std::string message);
   void perf(std::string threadId, std::string function, int line, int duration, std::string message);
   void warning(std::string threadId, std::string function, int line, ErrorId warningId, std::string message);
   void debug(std::string threadId, std::string function, int line, std::string message);
   void init(std::string threadId, std::string function, int line, std::string message);

   void infoCorrelation(std::string threadId, std::string function, int line, std::string message, std::string corelationId);
   void errorCorrelation(std::string threadId, std::string function, int line, ErrorId idError, const std::exception* exception, std::string message, std::string corelationId);
   void perfCorrelation(std::string threadId, std::string function, int line, int duration, std::string message, std::string corelationId);
   void warningCorrelation(std::string threadId, std::string function, int line, ErrorId warningId, std::string message, std::string corelationId);
   void debugCorrelation(std::string threadId, std::string function, int line, std::string message, std::string corelationId);
   void initCorrelation(std::string threadId, std::string function, int line, std::string message, std::string corelationId);

   template<typename... Args>
   inline void info(std::string threadId, std::string function, int line, fmt::basic_string_view<char> message, const Args &... args)
   {
      info(threadId, function, line, fmt::format(message, args...));
   }

   template<typename... Args>
   inline void error(std::string threadId, std::string function, int line, ErrorId idError, const std::exception* exception, fmt::basic_string_view<char> message, const Args &... args)
   {
      error(threadId, function, line, idError, exception, fmt::format(message, args...));
   }

   template<typename... Args>
   inline void perf(std::string threadId, std::string function, int line, int duration, fmt::basic_string_view<char> message, const Args &... args)
   {
      perf(threadId, function, line, duration, fmt::format(message, args...));
   }

   template<typename... Args>
   inline void warning(std::string threadId, std::string function, int line, ErrorId warningId, fmt::basic_string_view<char> message, const Args &... args)
   {
      warning(threadId, function, line, warningId, fmt::format(message, args...));
   }

   template<typename... Args>
   inline void debug(std::string threadId, std::string function, int line, fmt::basic_string_view<char> message, const Args &... args)
   {
      debug(threadId, function, line, fmt::format(message, args...));
   }

   template<typename... Args>
   inline void init(std::string threadId, std::string function, int line, fmt::basic_string_view<char> message, const Args &... args)
   {
      init(threadId, function, line, fmt::format(message, args...));
   }

   template<typename... Args>
   inline void infoCorrelationId(std::string threadId, std::string function, int line, ErrorId idError, const std::exception* exception, std::string corelationId, fmt::basic_string_view<char> message, const Args &... args)
   {
      infoCorrelation(threadId, function, line, idError, exception, fmt::format(message, args...), corelationId);
   }

   template<typename... Args>
   inline void errorCorrelationId(std::string threadId, std::string function, int line, ErrorId idError, const std::exception* exception, std::string corelationId, fmt::basic_string_view<char> message, const Args &... args)
   {
      errorCorrelation(threadId, function, line, idError, exception, fmt::format(message, args...), corelationId);
   }

   template<typename... Args>
   inline void perfCorrelationId(std::string threadId, std::string function, int line, int duration, std::string corelationId, fmt::basic_string_view<char> message, const Args &... args)
   {
      perfCorrelation(threadId, function, line, duration, fmt::format(message, args...), corelationId);
   }

   template<typename... Args>
   inline void warningCorrelationId(std::string threadId, std::string function, int line, ErrorId warningId, std::string corelationId, fmt::basic_string_view<char> message, const Args &... args)
   {
      warningCorrelation(threadId, function, line, warningId, fmt::format(message, args...), corelationId);
   }

   template<typename... Args>
   inline void debugCorrelationId(std::string threadId, std::string function, int line, std::string corelationId, fmt::basic_string_view<char> message, const Args &... args)
   {
      debugCorrelation(threadId, function, line, fmt::format(message, args...), corelationId);
   }

   template<typename... Args>
   inline void initCorrelationId(std::string threadId, std::string function, int line, std::string corelationId, fmt::basic_string_view<char> message, const Args &... args)
   {
      initCorrelation(threadId, function, line, fmt::format(message, args...), corelationId);
   }


   enum LogLevel
   {
      Perf = 1,
      Debug = 2,
      Warning = 3,
      Info = 4,
      Error = 5,
      Init = 6,
      Default
   };

   // Log settings
   void setLogLevel(LogLevel logLevel);

   void startLogging(::Log::Log::LogLevel logLevel = ::Log::Log::LogLevel::Default, unsigned int logMaxFileSize = 0);
   void dumpBacktrace();

   std::string getCurrentLogName(const std::string& logFolder);

   void restart();
   void shutdown();

private:

   LogLevel _currentLogLevel;

   // Max size of a log file in kb
   unsigned int _logFileMaxSize;

   unsigned int _maxLogFiles;

   std::string _logDirectory;

   void addEscapeCharacters(std::string& string);
   void trimThreadId(std::string& threadId);
   void getParentProcessName(std::string& parentProcessName);
   void getProcessId(const pid_t pid, char* name);

   std::function<void(const std::string&)> _sendLogData;

   bool _shutdown;

   Util::Mutex _timesMutex;

   std::map<std::string, long long int> _times;
};


}
