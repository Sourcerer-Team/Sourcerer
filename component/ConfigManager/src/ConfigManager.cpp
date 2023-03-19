#include "ConfigManager.h"

#include <QCoreApplication>
#include <QMap>
#include <QStandardPaths>

namespace ConfigManager {

static QMap<ConfigKey, QString> KEYNAMES = {{ConfigKey::Unknown, ""}};

ConfigManager::ConfigManager()
    : _settings(ORGANIZATION_NAME, APPLICATION_NAME) {
  _logsDir = getWritableDir();
}

ConfigManager::~ConfigManager() {}

ConfigManager &ConfigManager::getInstance() {
  static ConfigManager _instance;
  return _instance;
}

QDateTime ConfigManager::getDateTime(ConfigKey key) const {
  LOCKGUARD(_mutex);
  return _settings.value(KEYNAMES[key]).toDateTime();
}

qlonglong ConfigManager::getLong(ConfigKey key) const {
  LOCKGUARD(_mutex);
  return _settings.value(KEYNAMES[key]).toLongLong();
}

QString ConfigManager::getString(ConfigKey key) const {
  LOCKGUARD(_mutex);
  return _settings.value(KEYNAMES[key]).toString();
}

QString ConfigManager::getLogsDir() const { return _logsDir; }

#ifdef Q_OS_WIN32

#define CURRENT_DIR_SHORTCUT ".\\"

#else

#define CURRENT_DIR_SHORTCUT "./"

#endif

QString ConfigManager::getWritableDir() const {
  QStringList locations =
      QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
  locations.removeOne(
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
  locations.removeOne(QCoreApplication::applicationDirPath());
  locations.removeOne(QCoreApplication::applicationDirPath() +
                      QLatin1String("/data"));

  return locations.length() > 0 ? locations[0] : CURRENT_DIR_SHORTCUT;
}

} // namespace ConfigManager
