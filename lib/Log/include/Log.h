#pragma once

#include "Formatters.h"

#include <ConfigManager.h>
#include <LockGuard.h>

#include <spdlog/fmt/fmt.h>

#include <chrono>
#include <exception>
#include <functional>
#include <map>
#include <mutex>
#include <string>

#ifndef LOG_SINGLE_THREAD_APPLICATION

#define LOG_INFO(...)                                                          \
  {                                                                            \
    Log::Log::getInstance().info(THREAD_ID, __PRETTY_FUNCTION__, __LINE__,     \
                                 __VA_ARGS__);                                 \
  }
#define LOG_DEBUG(...)                                                         \
  {                                                                            \
    Log::Log::getInstance().debug(THREAD_ID, __PRETTY_FUNCTION__, __LINE__,    \
                                  __VA_ARGS__);                                \
  }
#define LOG_WARN(...)                                                          \
  {                                                                            \
    Log::Log::getInstance().warning(THREAD_ID, __PRETTY_FUNCTION__, __LINE__,  \
                                    Log::ErrorId::None, __VA_ARGS__);          \
  }
#define LOG_WARN_ID(id, ...)                                                   \
  {                                                                            \
    Log::Log::getInstance().warning(THREAD_ID, __PRETTY_FUNCTION__, __LINE__,  \
                                    id, __VA_ARGS__);                          \
  }
#define LOG_PERF(duration, ...)                                                \
  {                                                                            \
    Log::Log::getInstance().perf(THREAD_ID, __PRETTY_FUNCTION__, __LINE__,     \
                                 duration, __VA_ARGS__);                       \
  }
#define LOG_ERROR(id, ...)                                                     \
  {                                                                            \
    Log::Log::getInstance().error(THREAD_ID, __PRETTY_FUNCTION__, __LINE__,    \
                                  id, nullptr, __VA_ARGS__);                   \
  }
#define LOG_ERROR_EXC(id, exception, ...)                                      \
  {                                                                            \
    Log::Log::getInstance().error(THREAD_ID, __PRETTY_FUNCTION__, __LINE__,    \
                                  id, exception, __VA_ARGS__);                 \
  }
#define LOG_INIT(...)                                                          \
  {                                                                            \
    Log::Log::getInstance().init(THREAD_ID, __PRETTY_FUNCTION__, __LINE__,     \
                                 __VA_ARGS__);                                 \
  }

#define LOG_INFO_CORRELATION_ID(corelationId, ...)                             \
  {                                                                            \
    Log::Log::getInstance().infoCorrelationId(                                 \
        THREAD_ID, __PRETTY_FUNCTION__, __LINE__, corelationId, __VA_ARGS__);  \
  }
#define LOG_DEBUG_CORRELATION_ID(corelationId, ...)                            \
  {                                                                            \
    Log::Log::getInstance().debugCorrelationId(                                \
        THREAD_ID, __PRETTY_FUNCTION__, __LINE__, corelationId, __VA_ARGS__);  \
  }
#define LOG_WARN_CORRELATION_ID(corelationId, ...)                             \
  {                                                                            \
    Log::Log::getInstance().warningCorrelationId(                              \
        THREAD_ID, __PRETTY_FUNCTION__, __LINE__, Log::ErrorId::None,          \
        corelationId, __VA_ARGS__);                                            \
  }
#define LOG_WARN_ID_CORRELATION_ID(id, corelationId, ...)                      \
  {                                                                            \
    Log::Log::getInstance().warningCorrelationId(                              \
        THREAD_ID, __PRETTY_FUNCTION__, __LINE__, id, corelationId,            \
        __VA_ARGS__);                                                          \
  }
#define LOG_PERF_CORRELATION_ID(duration, corelationId, ...)                   \
  {                                                                            \
    Log::Log::getInstance().perfCorrelationId(THREAD_ID, __PRETTY_FUNCTION__,  \
                                              __LINE__, duration,              \
                                              corelationId, __VA_ARGS__);      \
  }
#define LOG_ERROR_CORRELATION_ID(id, corelationId, ...)                        \
  {                                                                            \
    Log::Log::getInstance().errorCorrelationId(THREAD_ID, __PRETTY_FUNCTION__, \
                                               __LINE__, id, nullptr,          \
                                               corelationId, __VA_ARGS__);     \
  }
#define LOG_ERROR_EXC_CORRELATION_ID(id, exception, corelationId, ...)         \
  {                                                                            \
    Log::Log::getInstance().errorCorrelationId(THREAD_ID, __PRETTY_FUNCTION__, \
                                               __LINE__, id, exception,        \
                                               corelationId, __VA_ARGS__);     \
  }
#define LOG_INIT_CORRELATION_ID(corelationId, ...)                             \
  {                                                                            \
    Log::Log::getInstance().initCorrelationId(                                 \
        THREAD_ID, __PRETTY_FUNCTION__, __LINE__, corelationId, __VA_ARGS__);  \
  }

#else

#define LOG_INFO(...)                                                          \
  {                                                                            \
    Log::Log::getInstance().info("", __PRETTY_FUNCTION__, __LINE__,            \
                                 __VA_ARGS__);                                 \
  }
#define LOG_DEBUG(...)                                                         \
  {                                                                            \
    Log::Log::getInstance().debug("", __PRETTY_FUNCTION__, __LINE__,           \
                                  __VA_ARGS__);                                \
  }
#define LOG_WARN(...)                                                          \
  {                                                                            \
    Log::Log::getInstance().warning("", __PRETTY_FUNCTION__, __LINE__,         \
                                    Log::ErrorId::None, __VA_ARGS__);          \
  }
#define LOG_WARN_ID(id, ...)                                                   \
  {                                                                            \
    Log::Log::getInstance().warning("", __PRETTY_FUNCTION__, __LINE__, id,     \
                                    __VA_ARGS__);                              \
  }
#define LOG_PERF(duration, ...)                                                \
  {                                                                            \
    Log::Log::getInstance().perf("", __PRETTY_FUNCTION__, __LINE__, duration,  \
                                 __VA_ARGS__);                                 \
  }
#define LOG_ERROR(id, ...)                                                     \
  {                                                                            \
    Log::Log::getInstance().error("", __PRETTY_FUNCTION__, __LINE__, id,       \
                                  nullptr, __VA_ARGS__);                       \
  }
#define LOG_ERROR_EXC(id, exception, ...)                                      \
  {                                                                            \
    Log::Log::getInstance().error("", __PRETTY_FUNCTION__, __LINE__, id,       \
                                  exception, __VA_ARGS__);                     \
  }
#define LOG_INIT(...)                                                          \
  {                                                                            \
    Log::Log::getInstance().init("", __PRETTY_FUNCTION__, __LINE__,            \
                                 __VA_ARGS__);                                 \
  }

#define LOG_INFO_CORRELATION_ID(corelationId, ...)                             \
  {                                                                            \
    Log::Log::getInstance().infoCorrelationId(                                 \
        "", __PRETTY_FUNCTION__, __LINE__, corelationId, __VA_ARGS__);         \
  }
#define LOG_DEBUG_CORRELATION_ID(corelationId, ...)                            \
  {                                                                            \
    Log::Log::getInstance().debugCorrelationId(                                \
        "", __PRETTY_FUNCTION__, __LINE__, corelationId, __VA_ARGS__);         \
  }
#define LOG_WARN_CORRELATION_ID(corelationId, ...)                             \
  {                                                                            \
    Log::Log::getInstance().warningCorrelationId("", __PRETTY_FUNCTION__,      \
                                                 __LINE__, Log::ErrorId::None, \
                                                 corelationId, __VA_ARGS__);   \
  }
#define LOG_WARN_ID_CORRELATION_ID(id, corelationId, ...)                      \
  {                                                                            \
    Log::Log::getInstance().warningCorrelationId(                              \
        "", __PRETTY_FUNCTION__, __LINE__, id, corelationId, __VA_ARGS__);     \
  }
#define LOG_PERF_CORRELATION_ID(duration, corelationId, ...)                   \
  {                                                                            \
    Log::Log::getInstance().perfCorrelationId("", __PRETTY_FUNCTION__,         \
                                              __LINE__, duration,              \
                                              corelationId, __VA_ARGS__);      \
  }
#define LOG_ERROR_CORRELATION_ID(id, corelationId, ...)                        \
  {                                                                            \
    Log::Log::getInstance().errorCorrelationId("", __PRETTY_FUNCTION__,        \
                                               __LINE__, id, nullptr,          \
                                               corelationId, __VA_ARGS__);     \
  }
#define LOG_ERROR_EXC_CORRELATION_ID(id, exception, corelationId, ...)         \
  {                                                                            \
    Log::Log::getInstance().errorCorrelationId("", __PRETTY_FUNCTION__,        \
                                               __LINE__, id, exception,        \
                                               corelationId, __VA_ARGS__);     \
  }
#define LOG_INIT_CORRELATION_ID(corelationId, ...)                             \
  {                                                                            \
    Log::Log::getInstance().initCorrelationId(                                 \
        "", __PRETTY_FUNCTION__, __LINE__, corelationId, __VA_ARGS__);         \
  }

#endif

namespace Log {

enum ErrorId {
  None = 0,
  Generic = -1,
  FileSystem = -2,
  Network = -3,
  Database = -4,
  Gui = -5,
  DateTime = -6,
  Logic = -7,
  Config = -8,
  Git = -9
};

class Log {
  Log();

public:
  static Log &getInstance();

  // Log writing
  void info(QString threadId, QString function, int line, QString message);
  void error(QString threadId, QString function, int line, ErrorId idError,
             const std::exception *exception, QString message);
  void perf(QString threadId, QString function, int line, int duration,
            QString message);
  void warning(QString threadId, QString function, int line, ErrorId warningId,
               QString message);
  void debug(QString threadId, QString function, int line, QString message);
  void init(QString threadId, QString function, int line, QString message);

  void infoCorrelation(QString threadId, QString function, int line,
                       QString message, QString corelationId);
  void errorCorrelation(QString threadId, QString function, int line,
                        ErrorId idError, const std::exception *exception,
                        QString message, QString corelationId);
  void perfCorrelation(QString threadId, QString function, int line,
                       int duration, QString message, QString corelationId);
  void warningCorrelation(QString threadId, QString function, int line,
                          ErrorId warningId, QString message,
                          QString corelationId);
  void debugCorrelation(QString threadId, QString function, int line,
                        QString message, QString corelationId);
  void initCorrelation(QString threadId, QString function, int line,
                       QString message, QString corelationId);

  template <typename... Args>
  inline void info(QString threadId, QString function, int line,
                   QString message, const Args &...args) {
    info(threadId, function, line,
         QString::fromStdString(fmt::format(message.toStdString(), args...)));
  }

  template <typename... Args>
  inline void error(QString threadId, QString function, int line,
                    ErrorId idError, const std::exception *exception,
                    QString message, const Args &...args) {
    error(threadId, function, line, idError, exception,
          QString::fromStdString(fmt::format(message.toStdString(), args...)));
  }

  template <typename... Args>
  inline void perf(QString threadId, QString function, int line, int duration,
                   QString message, const Args &...args) {
    perf(threadId, function, line, duration,
         QString::fromStdString(fmt::format(message.toStdString(), args...)));
  }

  template <typename... Args>
  inline void warning(QString threadId, QString function, int line,
                      ErrorId warningId, QString message, const Args &...args) {
    warning(
        threadId, function, line, warningId,
        QString::fromStdString(fmt::format(message.toStdString(), args...)));
  }

  template <typename... Args>
  inline void debug(QString threadId, QString function, int line,
                    QString message, const Args &...args) {
    debug(threadId, function, line,
          QString::fromStdString(fmt::format(message.toStdString(), args...)));
  }

  template <typename... Args>
  inline void init(QString threadId, QString function, int line,
                   QString message, const Args &...args) {
    init(threadId, function, line,
         QString::fromStdString(fmt::format(message.toStdString(), args...)));
  }

  template <typename... Args>
  inline void infoCorrelationId(QString threadId, QString function, int line,
                                ErrorId idError,
                                const std::exception *exception,
                                QString corelationId, QString message,
                                const Args &...args) {
    infoCorrelation(
        threadId, function, line, idError, exception,
        QString::fromStdString(fmt::format(message.toStdString(), args...)),
        corelationId);
  }

  template <typename... Args>
  inline void errorCorrelationId(QString threadId, QString function, int line,
                                 ErrorId idError,
                                 const std::exception *exception,
                                 QString corelationId, QString message,
                                 const Args &...args) {
    errorCorrelation(
        threadId, function, line, idError, exception,
        QString::fromStdString(fmt::format(message.toStdString(), args...)),
        corelationId);
  }

  template <typename... Args>
  inline void perfCorrelationId(QString threadId, QString function, int line,
                                int duration, QString corelationId,
                                QString message, const Args &...args) {
    perfCorrelation(
        threadId, function, line, duration,
        QString::fromStdString(fmt::format(message.toStdString(), args...)),
        corelationId);
  }

  template <typename... Args>
  inline void warningCorrelationId(QString threadId, QString function, int line,
                                   ErrorId warningId, QString corelationId,
                                   QString message, const Args &...args) {
    warningCorrelation(
        threadId, function, line, warningId,
        QString::fromStdString(fmt::format(message.toStdString(), args...)),
        corelationId);
  }

  template <typename... Args>
  inline void debugCorrelationId(QString threadId, QString function, int line,
                                 QString corelationId, QString message,
                                 const Args &...args) {
    debugCorrelation(
        threadId, function, line,
        QString::fromStdString(fmt::format(message.toStdString(), args...)),
        corelationId);
  }

  template <typename... Args>
  inline void initCorrelationId(QString threadId, QString function, int line,
                                QString corelationId, QString message,
                                const Args &...args) {
    initCorrelation(
        threadId, function, line,
        QString::fromStdString(fmt::format(message.toStdString(), args...)),
        corelationId);
  }

  enum LogLevel {
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

  void
  startLogging(::Log::Log::LogLevel logLevel = ::Log::Log::LogLevel::Default,
               unsigned int logMaxFileSize = 0);
  void dumpBacktrace();

  QString getCurrentLogName(const QString &logFolder);

  void restart();
  void shutdown();

private:
  void addEscapeCharacters(QString &string);

private:
  LogLevel _currentLogLevel;

  // Max size of a log file in kb
  unsigned int _logFileMaxSize;

  unsigned int _maxLogFiles;

  QString _logDirectory;

  bool _shutdown;

  Util::Mutex _timesMutex;

  std::map<QString, long long int> _times;

  ConfigManager::ConfigManager &_configManager;
};

} // namespace Log
