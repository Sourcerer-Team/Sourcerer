// MIT License
// Copyright (c) 2026 Sourcerer

#include "Log.h"

#include <ConfigManager.h>

#include <spdlog/sinks/septa_file_sink.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <filesystem>

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
#include <pthread.h>
#endif

namespace Log {

const char *ENTER_GUARD = "ENTER";
const char *EXIT_GUARD = "EXIT";

const size_t LOGGER_MAX_FILE_SIZE_IN_MB = 10;
const size_t LOGGER_MAX_FILES = 10;

std::shared_ptr<spdlog::logger> file_logger;

Log &Log::getInstance() {
  static Log instance;
  return instance;
}

Log::Log()
    : _configManager(ConfigManager::ConfigManager::getInstance()),
      _logFileMaxSize(LOGGER_MAX_FILE_SIZE_IN_MB),
      _maxLogFiles(LOGGER_MAX_FILES), _shutdown(false) {
  _logDirectory = _configManager.getLogsDir();

  spdlog::enable_backtrace(50);

  startLogging(LogLevel::Perf, _logFileMaxSize);
}

void Log::startLogging(::Log::Log::LogLevel logLevel,
                       unsigned int logMaxFileSize) {
  spdlog::set_pattern("%Y/%m/%d %H:%M:%S.%e;%v");

  bool exceptionOccured = false;
  std::exception lastException;

  if (logLevel != ::Log::Log::LogLevel::Default)
    setLogLevel(logLevel);

  if (logMaxFileSize != 0)
    _logFileMaxSize = logMaxFileSize;

  try {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    const std::string loggerName =
        std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count());
    const std::string logDirectory = _logDirectory.toStdString();

    std::filesystem::create_directories(logDirectory);
    file_logger = spdlog::septa_logger_mt(
        loggerName, logDirectory + "/" + ConfigManager::APPLICATION_NAME,
        _logFileMaxSize * 1024 * 1024, _maxLogFiles);
  } catch (std::exception &e) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    const std::string loggerName =
        std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count());

    lastException = e;
    exceptionOccured = true;
    file_logger =
        spdlog::septa_logger_mt(loggerName, ConfigManager::APPLICATION_NAME,
                                _logFileMaxSize * 1024 * 1024, _maxLogFiles);
  }

  spdlog::set_default_logger(file_logger);
  spdlog::flush_on(spdlog::level::trace);

  if (exceptionOccured) {
    error("Log", LOG_FUNCTION, __LINE__, ErrorId::Logic, &lastException,
          "Could not create default log directory " + _logDirectory);
  }
}

void Log::dumpBacktrace() {
  spdlog::dump_backtrace();
}

QString Log::getCurrentLogName(const QString &logFolder) {
  std::string result;
  const std::string logFolderStd = logFolder.toStdString();

  time_t newestTime = 0;

  using std::filesystem::directory_iterator;

  for (const auto &entry : directory_iterator(logFolderStd)) {
    if (entry.path().extension() == ".log") {
      struct stat buf;

      std::string file = logFolderStd;
      file += entry.path().filename().string();

      stat(file.c_str(), &buf);

      if (buf.st_mtime > newestTime) {
        result = file;
        newestTime = buf.st_mtime;
      }
    }
  }

  return QString::fromStdString(result);
}

void Log::info(QString threadId, QString function, int line, QString message) {
  if (_shutdown) {
    return;
  }

  addEscapeCharacters(message);

  const QString logMessage =
      QString("%1;INFO;[%2:%3].%4")
          .arg(threadId, function, QString::number(line), message);

  spdlog::warn(logMessage.toStdString());
}

void Log::infoCorrelation(QString threadId, QString function, int line,
                          QString message, QString corelationId) {
  if (_shutdown) {
    return;
  }

  addEscapeCharacters(message);

  const QString logMessage = QString("%1;INFO;[%2:%3] %4;%5")
                                 .arg(threadId, function, QString::number(line),
                                      message, corelationId);

  spdlog::warn(logMessage.toStdString());
}

void Log::error(QString threadId, QString function, int line, ErrorId idError,
                const std::exception *exception, QString message) {
  if (_shutdown) {
    return;
  }

  addEscapeCharacters(message);

  if (exception) {
    const QString logMessage =
        QString("%1;ERROR;[%2:%3] Err%4: %5;%6")
            .arg(threadId, function, QString::number(line),
                 QString::number(static_cast<int>(idError)), message,
                 QString::fromStdString(exception->what()));

    spdlog::error(logMessage.toStdString());
  } else {
    const QString logMessage =
        QString("%1;ERROR;[%2:%3] Err%4: %5")
            .arg(threadId, function, QString::number(line),
                 QString::number(static_cast<int>(idError)), message);

    spdlog::error(logMessage.toStdString());
  }
}

void Log::errorCorrelation(QString threadId, QString function, int line,
                           ErrorId idError, const std::exception *exception,
                           QString message, QString corelationId) {
  if (_shutdown) {
    return;
  }

  addEscapeCharacters(message);

  if (exception) {
    const QString logMessage =
        QString("%1;ERROR;[%2:%3] Err%4: %5;%6;%7")
            .arg(threadId, function, QString::number(line),
                 QString::number(static_cast<int>(idError)), message,
                 QString::fromStdString(exception->what()), corelationId);

    spdlog::error(logMessage.toStdString());
  } else {
    const QString logMessage =
        QString("%1;ERROR;[%2:%3] Err%4: %5;%6")
            .arg(threadId, function, QString::number(line),
                 QString::number(static_cast<int>(idError)), message,
                 corelationId);

    spdlog::error(logMessage.toStdString());
  }
}

void Log::perf(QString threadId, QString function, int line, int duration,
               QString message) {
  if (_shutdown) {
    return;
  }

  LOCKGUARD(_timesMutex);

  message = message.toUpper();

  using namespace std::chrono;
  const long long int epochTime =
      duration_cast<milliseconds>(system_clock::now().time_since_epoch())
          .count();

  if (message == ENTER_GUARD) {
    _times[function] = epochTime;
    duration = 0;
  } else if (message == EXIT_GUARD) {
    const auto it = _times.find(function);

    if (it != _times.end()) {
      duration = epochTime - it->second;
      _times.erase(it);
    } else {
      duration = 0;
    }
  }

  addEscapeCharacters(message);

  // log on warning level

  const QString logMessage = QString("%1;PERF ;[%2:%3] %4-%5")
                                 .arg(threadId, function, QString::number(line),
                                      QString::number(duration), message);

  spdlog::trace(logMessage.toStdString());
}

void Log::perfCorrelation(QString threadId, QString function, int line,
                          int duration, QString message, QString corelationId) {
  if (_shutdown) {
    return;
  }

  addEscapeCharacters(message);

  // log on warning level

  const QString logMessage =
      QString("%1;PREF ;[%2:%3] %4-%5;%6")
          .arg(threadId, function, QString::number(line),
               QString::number(duration), message, corelationId);

  spdlog::trace(logMessage.toStdString());
}

void Log::warning(QString threadId, QString function, int line,
                  ErrorId warningId, QString message) {
  if (_shutdown) {
    return;
  }

  addEscapeCharacters(message);

  const QString logMessage =
      QString("%1;WARN ;[%2:%3] Warn%4:%5")
          .arg(threadId, function, QString::number(line),
               QString::number(static_cast<int>(warningId)), message);

  spdlog::info(logMessage.toStdString());
}

void Log::warningCorrelation(QString threadId, QString function, int line,
                             ErrorId warningId, QString message,
                             QString corelationId) {
  if (_shutdown) {
    return;
  }

  addEscapeCharacters(message);

  const QString logMessage =
      QString("%1;WARN ;[%2:%3] Warn%4:%5;%6")
          .arg(threadId, function, QString::number(line),
               QString::number(static_cast<int>(warningId)), message,
               corelationId);

  spdlog::info(logMessage.toStdString());
}

void Log::debug(QString threadId, QString function, int line, QString message) {
  if (_shutdown) {
    return;
  }

  addEscapeCharacters(message);

  const QString logMessage =
      QString("%1;DEBUG;[%2:%3] %4")
          .arg(threadId, function, QString::number(line), message);

  spdlog::debug(logMessage.toStdString());
}

void Log::debugCorrelation(QString threadId, QString function, int line,
                           QString message, QString corelationId) {
  if (_shutdown) {
    return;
  }

  addEscapeCharacters(message);

  const QString logMessage = QString("%1;DEBUG;[%2:%3] %4;%5")
                                 .arg(threadId, function, QString::number(line),
                                      message, corelationId);

  spdlog::debug(logMessage.toStdString());
}

void Log::init(QString threadId, QString function, int line, QString message) {
  if (_shutdown) {
    return;
  }

  addEscapeCharacters(message);

  const QString logMessage =
      QString("%1;INIT;[%2:%3] %4")
          .arg(threadId, function, QString::number(line), message);

  spdlog::critical(logMessage.toStdString());
}

void Log::initCorrelation(const QString &threadId, QString function, int line,
                          QString message, QString corelationId) const {
  if (_shutdown) {
    return;
  }

  addEscapeCharacters(message);

  const QString logMessage = QString("%1;INIT;[%2:%3] %4;%5")
                                 .arg(threadId, function, QString::number(line),
                                      message, corelationId);

  spdlog::critical(logMessage.toStdString());
}

void Log::setLogLevel(LogLevel logLevel) {
  _currentLogLevel = logLevel;

  spdlog::level::level_enum spdlogLevel;

  switch (logLevel) {
  case LogLevel::Perf: {
    spdlogLevel = spdlog::level::level_enum::trace;
    break;
  }
  case LogLevel::Debug: {
    spdlogLevel = spdlog::level::level_enum::debug;
    break;
  }
  case LogLevel::Warning: {
    spdlogLevel = spdlog::level::level_enum::info;
    break;
  }
  case LogLevel::Info: {
    spdlogLevel = spdlog::level::level_enum::warn;
    break;
  }
  case LogLevel::Error: {
    spdlogLevel = spdlog::level::level_enum::err;
    break;
  }
  case LogLevel::Init: {
    spdlogLevel = spdlog::level::level_enum::critical;
    break;
  }
  case LogLevel::Default:
  default: {
    spdlogLevel = spdlog::level::level_enum::err;
    break;
  }
  }

  spdlog::set_level(spdlogLevel);
}

void Log::addEscapeCharacters(QString &string) {
  const auto escapeCharacter = "\\";

  std::size_t position = string.indexOf(";", 0);

  while (position != -1) {
    string.insert(static_cast<qsizetype>(position), &escapeCharacter[0]);
    position = string.indexOf(";", static_cast<qsizetype>(position) + 2);
  }
}

void Log::restart() {
  startLogging();
}

void Log::shutdown() {
  if (_shutdown) {
    return;
  }

  _shutdown = true;
  spdlog::shutdown();
}

} // namespace Log
