#include "Log.h"

#include <ConfigManager.h>

#include <spdlog/sinks/septa_file_sink.h>
#include <spdlog/spdlog.h>

#include <experimental/filesystem>

#include <algorithm>
#include <chrono>
#include <experimental/filesystem>
#include <pthread.h>

namespace Log {

const char *ENTER_GUARD = "ENTER";
const char *EXIT_GUARD = "EXIT";

const size_t LOGGER_MAX_FILE_SIZE_IN_MB = 10;

std::shared_ptr<spdlog::logger> file_logger;

Log &Log::getInstance() {
  static Log instance;
  return instance;
}

Log::Log()
    : _configManager(ConfigManager::ConfigManager::getInstance()),
      _logFileMaxSize(LOGGER_MAX_FILE_SIZE_IN_MB), _maxLogFiles(5),
      _shutdown(false) {
  _logDirectory = _configManager.getLogsDir();

  spdlog::enable_backtrace(50);

  setLogLevel(LogLevel::Error);
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

    std::experimental::filesystem::create_directories(logDirectory);
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
    error("Log", __PRETTY_FUNCTION__, __LINE__, ErrorId::Logic, &lastException,
          "Could not create default log directory " + _logDirectory);
  }
}

void Log::dumpBacktrace() { spdlog::dump_backtrace(); }

QString Log::getCurrentLogName(const QString &logFolder) {
  std::string result;
  const std::string logFolderStd = logFolder.toStdString();

  time_t newestTime = 0;

  using std::experimental::filesystem::directory_iterator;

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
  trimThreadId(threadId);

  const QString logMessage = QString("%1;INFO;[%2:%3].%4")
                                 .arg(threadId)
                                 .arg(function)
                                 .arg(line)
                                 .arg(message);

  spdlog::warn(logMessage.toStdString());
}

void Log::infoCorrelation(QString threadId, QString function, int line,
                          QString message, QString corelationId) {
  if (_shutdown) {
    return;
  }

  addEscapeCharacters(message);
  trimThreadId(threadId);

  const QString logMessage = QString("%1;INFO;[%2:%3] %4;%5")
                                 .arg(threadId)
                                 .arg(function)
                                 .arg(line)
                                 .arg(message)
                                 .arg(corelationId);

  spdlog::warn(logMessage.toStdString());
}

void Log::error(QString threadId, QString function, int line, ErrorId idError,
                const std::exception *exception, QString message) {
  if (_shutdown) {
    return;
  }

  addEscapeCharacters(message);
  trimThreadId(threadId);

  if (exception) {
    const QString logMessage =
        QString("%1;ERROR;[%2:%3] Err%4: %5;%6")
            .arg(threadId)
            .arg(function)
            .arg(line)
            .arg(static_cast<int>(idError))
            .arg(message)
            .arg(QString::fromStdString(exception->what()));

    spdlog::error(logMessage.toStdString());
  } else {
    const QString logMessage = QString("%1;ERROR;[%2:%3] Err%4: %5")
                                   .arg(threadId)
                                   .arg(function)
                                   .arg(line)
                                   .arg(static_cast<int>(idError))
                                   .arg(message);

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
  trimThreadId(threadId);

  if (exception) {
    const QString logMessage =
        QString("%1;ERROR;[%2:%3] Err%4: %5;%6;%7")
            .arg(threadId)
            .arg(function)
            .arg(line)
            .arg(static_cast<int>(idError))
            .arg(message)
            .arg(QString::fromStdString(exception->what()))
            .arg(corelationId);

    spdlog::error(logMessage.toStdString());
  } else {
    const QString logMessage = QString("%1;ERROR;[%2:%3] Err%4: %5;%6")
                                   .arg(threadId)
                                   .arg(function)
                                   .arg(line)
                                   .arg(static_cast<int>(idError))
                                   .arg(message)
                                   .arg(corelationId);

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
  trimThreadId(threadId);

  // log on warning level

  const QString logMessage = QString("%1;PERF ;[%2:%3] %4-%5")
                                 .arg(threadId)
                                 .arg(function)
                                 .arg(line)
                                 .arg(duration)
                                 .arg(message);

  spdlog::trace(logMessage.toStdString());
}

void Log::perfCorrelation(QString threadId, QString function, int line,
                          int duration, QString message, QString corelationId) {
  if (_shutdown) {
    return;
  }

  addEscapeCharacters(message);
  trimThreadId(threadId);

  // log on warning level

  const QString logMessage = QString("%1;PREF ;[%2:%3] %4-%5;%6")
                                 .arg(threadId)
                                 .arg(function)
                                 .arg(line)
                                 .arg(duration)
                                 .arg(message)
                                 .arg(corelationId);

  spdlog::trace(logMessage.toStdString());
}

void Log::warning(QString threadId, QString function, int line,
                  ErrorId warningId, QString message) {
  if (_shutdown) {
    return;
  }

  addEscapeCharacters(message);
  trimThreadId(threadId);

  const QString logMessage = QString("%1;WARN ;[%2:%3] Warn%4:%5")
                                 .arg(threadId)
                                 .arg(function)
                                 .arg(line)
                                 .arg(static_cast<int>(warningId))
                                 .arg(message);

  spdlog::info(logMessage.toStdString());
}

void Log::warningCorrelation(QString threadId, QString function, int line,
                             ErrorId warningId, QString message,
                             QString corelationId) {
  if (_shutdown) {
    return;
  }

  addEscapeCharacters(message);
  trimThreadId(threadId);

  const QString logMessage = QString("%1;WARN ;[%2:%3] Warn%4:%5;%6")
                                 .arg(threadId)
                                 .arg(function)
                                 .arg(line)
                                 .arg(static_cast<int>(warningId))
                                 .arg(message)
                                 .arg(corelationId);

  spdlog::info(logMessage.toStdString());
}

void Log::debug(QString threadId, QString function, int line, QString message) {
  if (_shutdown) {
    return;
  }

  addEscapeCharacters(message);
  trimThreadId(threadId);

  const QString logMessage = QString("%1;DEBUG;[%2:%3] %4")
                                 .arg(threadId)
                                 .arg(function)
                                 .arg(line)
                                 .arg(message);

  spdlog::debug(logMessage.toStdString());
}

void Log::debugCorrelation(QString threadId, QString function, int line,
                           QString message, QString corelationId) {
  if (_shutdown) {
    return;
  }

  addEscapeCharacters(message);
  trimThreadId(threadId);

  const QString logMessage = QString("%1;DEBUG;[%2:%3] %4;%5")
                                 .arg(threadId)
                                 .arg(function)
                                 .arg(line)
                                 .arg(message)
                                 .arg(corelationId);

  spdlog::debug(logMessage.toStdString());
}

void Log::init(QString threadId, QString function, int line, QString message) {
  if (_shutdown) {
    return;
  }

  addEscapeCharacters(message);
  trimThreadId(threadId);

  const QString logMessage = QString("%1;INIT;[%2:%3] %4")
                                 .arg(threadId)
                                 .arg(function)
                                 .arg(line)
                                 .arg(message);

  spdlog::critical(logMessage.toStdString());
}

void Log::initCorrelation(QString threadId, QString function, int line,
                          QString message, QString corelationId) {
  if (_shutdown) {
    return;
  }

  addEscapeCharacters(message);
  trimThreadId(threadId);

  const QString logMessage = QString("%1;INIT;[%2:%3] %4;%5")
                                 .arg(threadId)
                                 .arg(function)
                                 .arg(line)
                                 .arg(message)
                                 .arg(corelationId);

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
  const char escapeCharacter = '\\';

  std::size_t position = string.indexOf(";", 0);

  while (position != -1) {
    string.insert(position, &escapeCharacter);
    position = string.indexOf(";", position + 2);
  }
}

void Log::trimThreadId(QString &threadId) {
  threadId = QString("{:<%1}").arg(threadId.left(10));
}

void Log::restart() { startLogging(); }

void Log::shutdown() {
  if (_shutdown) {
    return;
  }

  _shutdown = true;
  spdlog::shutdown();
}

} // namespace Log
