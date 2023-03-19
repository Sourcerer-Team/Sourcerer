#include "Log.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/septa_file_sink.h>

#include <experimental/filesystem>

#include <algorithm>
#include <chrono>
#include <pthread.h>
#include <experimental/filesystem>

namespace Log
{

const size_t LOGGER_MAX_FILE_SIZE_IN_MB = 10;

const char* APPLICATION_NAME = "Sourcerer";

std::shared_ptr<spdlog::logger> file_logger;


Log& Log::getInstance()
{
   static Log instance;

   return instance;
}

Log::Log()
    : _logFileMaxSize(LOGGER_MAX_FILE_SIZE_IN_MB)
    , _logDirectory("/data/var/log/")
    , _maxLogFiles(5)
    , _shutdown(false)
{
   spdlog::enable_backtrace(50);

   setLogLevel(LogLevel::Error);
}

void Log::startLogging(::Log::Log::LogLevel logLevel, unsigned int logMaxFileSize)
{
   spdlog::set_pattern("%Y/%m/%d %H:%M:%S.%e;%v");

   bool exceptionOccured = false;
   std::exception lastException;

   if (logLevel != ::Log::Log::LogLevel::Default)
   {
      setLogLevel(logLevel);
   }

   if (logMaxFileSize != 0)
   {
      _logFileMaxSize = logMaxFileSize;
   }

   try
   {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));

      std::string loggerName = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::system_clock::now().time_since_epoch()).count());

      std::experimental::filesystem::create_directories(_logDirectory);
      file_logger = spdlog::septa_logger_mt(loggerName, _logDirectory + "/" + APPLICATION_NAME, _logFileMaxSize * 1024 * 1024, _maxLogFiles);
   }
   catch(std::exception& e)
   {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));

      std::string loggerName = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::system_clock::now().time_since_epoch()).count());
      lastException = e;
      exceptionOccured = true;
      file_logger = spdlog::septa_logger_mt(loggerName, APPLICATION_NAME, _logFileMaxSize * 1024 * 1024, _maxLogFiles);
   }

   spdlog::set_default_logger(file_logger);
   spdlog::flush_on(spdlog::level::trace);

   if(exceptionOccured)
   {
      error("Log", __PRETTY_FUNCTION__, __LINE__, ErrorId::Logic, &lastException, "Could not create default log directory " + _logDirectory);
   }
}

void Log::dumpBacktrace()
{
   spdlog::dump_backtrace();
}

std::string Log::getCurrentLogName(const std::string& logFolder)
{
   std::string result;

   time_t newestTime = 0;

   using std::experimental::filesystem::directory_iterator;

   for (const auto& entry : directory_iterator(logFolder))
   {
      if (entry.path().extension() == ".log")
      {
         struct stat buf;

         std::string file = logFolder;
         file += entry.path().filename().string();

         stat(file.c_str(), &buf);

         if (buf.st_mtime > newestTime)
         {
            result = file;
            newestTime = buf.st_mtime;
         }
      }
   }

   return result;
}

void Log::info(std::string threadId, std::string function, int line, std::string message)
{
   if(_shutdown)
   {
      return;
   }

   std::string finalMessage;
   std::string logMessage;
   addEscapeCharacters(message);
   trimThreadId(threadId);

   logMessage = fmt::format("{0};INFO;[{1}:{2}]", threadId, function, line);

   finalMessage = fmt::format("{0} {1}", logMessage, message);

   spdlog::warn(finalMessage);
}

void Log::infoCorrelation(std::string threadId, std::string function, int line, std::string message, std::string corelationId)
{
   if(_shutdown)
   {
      return;
   }

   std::string finalMessage;
   std::string logMessage;
   addEscapeCharacters(message);
   trimThreadId(threadId);

   logMessage = fmt::format("{0};INFO;[{1}:{2}]", threadId, function, line);

   finalMessage = fmt::format("{0} {1}", logMessage, message);
   finalMessage = fmt::format("{0};{1}", finalMessage, corelationId);

   spdlog::warn(finalMessage);
}


void Log::error(std::string threadId, std::string function, int line, ErrorId idError, const std::exception* exception, std::string message)
{
   if(_shutdown)
   {
      return;
   }

   std::string finalMessage;
   std::string logMessage;
   addEscapeCharacters(message);
   trimThreadId(threadId);

   logMessage = fmt::format("{0};ERROR;[{1}:{2}] Err{3}:", threadId, function, line, static_cast<int>(idError));

   if (exception)
   {
      finalMessage = fmt::format("{0} {1};{2}", logMessage, message, exception->what());
   }
   else
   {
      finalMessage = fmt::format("{0} {1}", logMessage, message);
   }

   spdlog::error(finalMessage);
}

void Log::errorCorrelation(std::string threadId, std::string function, int line, ErrorId idError, const std::exception* exception, std::string message, std::string corelationId)
{
   if(_shutdown)
   {
      return;
   }

   std::string finalMessage;
   std::string logMessage;
   addEscapeCharacters(message);
   trimThreadId(threadId);

   logMessage = fmt::format("{0};ERROR;[{1}:{2}] Err{3}:", threadId, function, line, static_cast<int>(idError));

   if (exception)
   {
      finalMessage = fmt::format("{0} {1};{2}", logMessage, message, exception->what());
   }
   else
   {
      finalMessage = fmt::format("{0} {1}", logMessage, message);
   }

   finalMessage = fmt::format("{0};{1}", finalMessage, corelationId);

   spdlog::error(finalMessage);
}

void Log::perf(std::string threadId, std::string function, int line, int duration, std::string message)
{
   if(_shutdown)
   {
      return;
   }

   LOCKGUARD(_timesMutex);

   std::transform(message.begin(), message.end(), message.begin(), toupper);

   using namespace std::chrono;
   const long long int epochTime = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

   if (message == "ENTER")
   {
      _times[function] = epochTime;
      duration = 0;
   }
   else if(message == "EXIT")
   {
      const auto it = _times.find(function);

      if(it != _times.end())
      {
         duration = epochTime - it->second;
         _times.erase(it);
      }
      else
      {
         duration = 0;
      }
   }

   addEscapeCharacters(message);
   trimThreadId(threadId);

   // log on warning level

   std::string finalMessage = fmt::format("{0};PERF ;[{1}:{2}] {3}-{4}", threadId, function, line, duration, message);

   spdlog::trace(finalMessage);
}

void Log::perfCorrelation(std::string threadId, std::string function, int line, int duration, std::string message, std::string corelationId)
{
   if(_shutdown)
   {
      return;
   }

   addEscapeCharacters(message);
   trimThreadId(threadId);

   // log on warning level

   std::string finalMessage = fmt::format("{0};PREF ;[{1}:{2}] {3}-{4};{5}", threadId, function, line, duration, message, corelationId);

   spdlog::trace(finalMessage);
}

void Log::warning(std::string threadId, std::string function, int line, ErrorId warningId, std::string message)
{
   if(_shutdown)
   {
      return;
   }

   addEscapeCharacters(message);
   trimThreadId(threadId);

   std::string finalMessage = fmt::format("{0};WARN ;[{1}:{2}] Warn{3}:{4}", threadId, function, line, static_cast<int>(warningId), message);

   spdlog::info(finalMessage);
}

void Log::warningCorrelation(std::string threadId, std::string function, int line, ErrorId warningId, std::string message, std::string corelationId)
{
   if(_shutdown)
   {
      return;
   }

   addEscapeCharacters(message);
   trimThreadId(threadId);

   std::string finalMessage = fmt::format("{0};WARN ;[{1}:{2}] Warn{3}:{4};{5}", threadId, function, line, static_cast<int>(warningId), message, corelationId);

   spdlog::info(finalMessage);
}

void Log::debug(std::string threadId, std::string function, int line, std::string message)
{
   if(_shutdown)
   {
      return;
   }

   addEscapeCharacters(message);
   trimThreadId(threadId);

   std::string finalMessage = fmt::format("{0};DEBUG;[{1}:{2}] {3}", threadId, function, line, message);

   spdlog::debug(finalMessage);
}

void Log::debugCorrelation(std::string threadId, std::string function, int line, std::string message, std::string corelationId)
{
   if(_shutdown)
   {
      return;
   }

   addEscapeCharacters(message);
   trimThreadId(threadId);

   std::string finalMessage = fmt::format("{0};DEBUG;[{1}:{2}] {3};{4}", threadId, function, line, message, corelationId);

   spdlog::debug(finalMessage);
}

void Log::init(std::string threadId, std::string function, int line, std::string message)
{
   if(_shutdown)
   {
      return;
   }

   addEscapeCharacters(message);
   trimThreadId(threadId);

   std::string finalMessage = fmt::format("{0};INIT;[{1}:{2}] {3}", threadId, function, line, message);

   spdlog::critical(finalMessage);
}

void Log::initCorrelation(std::string threadId, std::string function, int line, std::string message, std::string corelationId)
{
   if(_shutdown)
   {
      return;
   }

   addEscapeCharacters(message);
   trimThreadId(threadId);

   std::string finalMessage = fmt::format("{0};INIT;[{1}:{2}] {3};{4}", threadId, function, line, message, corelationId);

   spdlog::critical(finalMessage);
}


void Log::setLogLevel(LogLevel logLevel)
{
   _currentLogLevel = logLevel;

   spdlog::level::level_enum spdlogLevel;

   switch(logLevel)
   {
   case LogLevel::Perf:
   {
      spdlogLevel = spdlog::level::level_enum::trace;
      break;
   }
   case LogLevel::Debug:
   {
      spdlogLevel = spdlog::level::level_enum::debug;
      break;
   }
   case LogLevel::Warning:
   {
      spdlogLevel = spdlog::level::level_enum::info;
      break;
   }
   case LogLevel::Info:
   {
      spdlogLevel = spdlog::level::level_enum::warn;
      break;
   }
   case LogLevel::Error:
   {
      spdlogLevel = spdlog::level::level_enum::err;
      break;
   }
   case LogLevel::Init:
   {
      spdlogLevel = spdlog::level::level_enum::critical;
      break;
   }
   case LogLevel::Default:
   default:
   {
      spdlogLevel = spdlog::level::level_enum::err;
      break;
   }
   }

   spdlog::set_level(spdlogLevel);
}

void Log::addEscapeCharacters(std::string& string)
{
   const char escapeCharacter = '\\';

   std::size_t position = string.find(";", 0);

   while (position != std::string::npos)
   {
      string.insert(position, &escapeCharacter, 1);

      position = string.find(";", position + 2);
   }
}

void Log::trimThreadId(std::string& threadId)
{
   if (threadId.size() == 0)
   {
      getParentProcessName(threadId);
   }

   if (threadId.size() > 10)
   {
      threadId = threadId.erase(10, std::string::npos);
   }

   threadId = fmt::format("{:<{}}", threadId, 10);
}

void Log::restart()
{
   startLogging();
}

void Log::shutdown()
{
   if(_shutdown)
   {
      return;
   }

   _shutdown = true;
   spdlog::shutdown();
}

}

