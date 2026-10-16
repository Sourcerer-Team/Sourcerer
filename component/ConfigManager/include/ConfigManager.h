#pragma once

#include "ConfigGroup.h"
#include "ConfigKey.h"

#include <LockGuard.h>

#include <QDateTime>
#include <QSettings>
#include <QString>

class QColor;

namespace ConfigManager {

extern const char *APPLICATION_NAME;
extern const char *ORGANIZATION_NAME;

class ConfigManager {
public:
  static ConfigManager &getInstance();

  bool getBoolean(ConfigKey key) const;

  QColor getColor(ConfigKey key) const;

  QDateTime getDateTime(ConfigKey key) const;

  qlonglong getLong(ConfigKey key) const;

  QString getString(ConfigKey key) const;

  QString getString(const QString &key) const;

  void setBoolean(ConfigKey key, bool value);

  void setColor(ConfigKey key, const QColor &value);

  void setDateTime(ConfigKey key, const QDateTime &value);

  void setLong(ConfigKey key, qlonglong value);

  void setString(ConfigKey key, const QString &value);

  void setString(const QString &key, const QString &value);

  void removeString(const QString &key);

  void save();

  ConfigKey getKey(const QString &keyPath) const;

  QString getLogsDir() const;

  QString getAppVersion() const;

  ConfigGroup getGroupped() const;

private:
  ConfigManager();

  virtual ~ConfigManager();

  static QString getWritableDir();

private:
  mutable Util::Mutex _mutex;
  QSettings _settings;
  QString _logsDir;
};

} // namespace ConfigManager
