#pragma once

#include "ConfigKey.h"

#include <LockGuard.h>

#include <QDateTime>
#include <QSettings>
#include <QString>

namespace ConfigManager
{

const char* APPLICATION_NAME = "Sourcerer";
const char* ORGANIZATION_NAME = "Sourcerer";

class ConfigManager
{
public:
    static ConfigManager& getInstance();

    QDateTime getDateTime(ConfigKey key) const;

    qlonglong getLong(ConfigKey key) const;

    QString getString(ConfigKey key) const;

    QString getLogsDir() const;

private:
    ConfigManager();

    ~ConfigManager();

    QString getWritableDir() const;

private:
    mutable Util::Mutex _mutex;
    QSettings _settings;
    QString _logsDir;
};

}
