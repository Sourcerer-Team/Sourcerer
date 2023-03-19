#include "ConfigManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QMap>
#include <QStandardPaths>

namespace ConfigManager {

const char *APPLICATION_NAME = "Sourcerer";
const char *ORGANIZATION_NAME = APPLICATION_NAME;
const char *APPLICATION_DIR = APPLICATION_NAME;
const char *LOGS_DIR = "Logs";

QMap<ConfigKey, QString> KEYNAMES = {{ConfigKey::Unknown, ""}};
QMap<ConfigKey, QDateTime> DEFAULT_DATETIMES = {};
QMap<ConfigKey, qlonglong> DEFAULT_LONGS = {};
QMap<ConfigKey, QString> DEFAULT_STRINGS = {};

ConfigManager::ConfigManager()
    : _settings(ORGANIZATION_NAME, APPLICATION_NAME) {
  _logsDir = getWritableDir() + QDir::separator() + APPLICATION_DIR +
             QDir::separator() + LOGS_DIR + QDir::separator();
  QDir dir;
  dir.mkpath(_logsDir);
}

ConfigManager::~ConfigManager() {}

ConfigManager &ConfigManager::getInstance() {
  static ConfigManager instance;
  return instance;
}

QDateTime ConfigManager::getDateTime(ConfigKey key) const {
  LOCKGUARD(_mutex);
  if (_settings.contains(KEYNAMES[key]))
    return _settings.value(KEYNAMES[key]).toDateTime();
  else
    return DEFAULT_DATETIMES[key];
}

qlonglong ConfigManager::getLong(ConfigKey key) const {
  LOCKGUARD(_mutex);
  if (_settings.contains(KEYNAMES[key]))
    return _settings.value(KEYNAMES[key]).toLongLong();
  else
    return DEFAULT_LONGS[key];
}

QString ConfigManager::getString(ConfigKey key) const {
  LOCKGUARD(_mutex);
  if (_settings.contains(KEYNAMES[key]))
    return _settings.value(KEYNAMES[key]).toString();
  else
    return DEFAULT_STRINGS[key];
}

QString ConfigManager::getLogsDir() const { return _logsDir; }

QString ConfigManager::getWritableDir() const {
  return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

} // namespace ConfigManager
