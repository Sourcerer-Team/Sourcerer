#pragma once

#include "ConfigKey.h"

#include <LockGuard.h>

#include <QDateTime>
#include <QSettings>
#include <QString>

namespace ConfigManager {

extern const char *APPLICATION_NAME;
extern const char *ORGANIZATION_NAME;

class ConfigManager {
public:
  static ConfigManager &getInstance();

  QDateTime getDateTime(ConfigKey key) const;

  qlonglong getLong(ConfigKey key) const;

  QString getString(ConfigKey key) const;

  QString getLogsDir() const;

private:
  ConfigManager();

  virtual ~ConfigManager();

  QString getWritableDir() const;

private:
  mutable Util::Mutex _mutex;
  QSettings _settings;
  QString _logsDir;
};

} // namespace ConfigManager
