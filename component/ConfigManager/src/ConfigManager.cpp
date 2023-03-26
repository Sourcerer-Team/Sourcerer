#include "ConfigManager.h"
#include "../../GitVersion.h"

#include <QCoreApplication>
#include <QDir>
#include <QMap>
#include <QStandardPaths>

namespace ConfigManager {

const char *APPLICATION_NAME = "Sourcerer";
const char *ORGANIZATION_NAME = APPLICATION_NAME;
const char *APPLICATION_DIR = APPLICATION_NAME;
const char *LOGS_DIR = "Logs";

QMap<ConfigKey, QString> KEY_NAMES = {
    {ConfigKey::Unknown, ""},
    {ConfigKey::RecentlyOpenedRepos, "workspace/recentlyOpened"},
    {ConfigKey::TitleBarHeight, "window/titleBarHeight"},
    {ConfigKey::TitleBarMaxTabWidth, "window/titleBarMaxTabWidth"},
    {ConfigKey::TitleBarMenuMinimumSize, "window/titleBarMenuMinimumWidth"}};

QMap<ConfigKey, QString> KEY_DESCRIPTIONS = {
    {ConfigKey::Unknown, ""},
    {ConfigKey::RecentlyOpenedRepos, ""},
    {ConfigKey::TitleBarHeight, ""},
    {ConfigKey::TitleBarMenuMinimumSize, ""}};

QMap<ConfigKey, QDateTime> DEFAULT_DATETIMES = {};

QMap<ConfigKey, qlonglong> DEFAULT_LONGS = {
    {ConfigKey::TitleBarHeight, 40},
    {ConfigKey::TitleBarMaxTabWidth, 285},
    {ConfigKey::TitleBarMenuMinimumSize, 230}};

QMap<ConfigKey, QString> DEFAULT_STRINGS = {
    {ConfigKey::RecentlyOpenedRepos,
     "git@github.com:levi9-embedded/"
     "components.git,git@github.com:levi9-embedded/arduino-projects.git"}};

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
  if (_settings.contains(KEY_NAMES[key]))
    return _settings.value(KEY_NAMES[key]).toDateTime();
  else
    return DEFAULT_DATETIMES[key];
}

qlonglong ConfigManager::getLong(ConfigKey key) const {
  LOCKGUARD(_mutex);
  if (_settings.contains(KEY_NAMES[key]))
    return _settings.value(KEY_NAMES[key]).toLongLong();
  else
    return DEFAULT_LONGS[key];
}

QString ConfigManager::getString(ConfigKey key) const {
  LOCKGUARD(_mutex);
  if (_settings.contains(KEY_NAMES[key]))
    return _settings.value(KEY_NAMES[key]).toString();
  else
    return DEFAULT_STRINGS[key];
}

QString ConfigManager::getLogsDir() const { return _logsDir; }

QString ConfigManager::getAppVersion() const { return APPLICATION_VERSION; }

static ConfigGroup createPathGroup(const QString &path, size_t from) {
  const int index = path.indexOf('/', from);
  if (index == -1)
    return ConfigGroup();

  const int lastIndex = path.lastIndexOf('/');

  if (index == lastIndex) {
    const ConfigKey key = KEY_NAMES.key(path);
    const QString &description = KEY_DESCRIPTIONS[key];
    if (description.isEmpty())
      return ConfigGroup();

    ConfigItem item;
    item.setKey(path);
    item.setDescription(description);

    if (DEFAULT_STRINGS.contains(key)) {
      item.setType(ConfigKeyType::String);
      item.setValue(DEFAULT_STRINGS[key]);
    } else if (DEFAULT_LONGS.contains(key)) {
      item.setType(ConfigKeyType::Long);
      item.setValue(DEFAULT_LONGS[key]);
    } else if (DEFAULT_DATETIMES.contains(key)) {
      item.setType(ConfigKeyType::DateTime);
      item.setValue(DEFAULT_DATETIMES[key]);
    } else
      return ConfigGroup();

    ConfigGroup group(path.mid(from, index - from));
    group.addItem(item);

    return group;
  } else {
    ConfigGroup group(path.mid(from, index - from));
    const ConfigGroup pathGroup = createPathGroup(path, index + 1);
    if (!pathGroup.isEmpty())
      group.merge(pathGroup);

    return group;
  }
}

ConfigGroup ConfigManager::getGroupped() const {
  ConfigGroup group;

  for (int key = (int)ConfigKey::Unknown; key < (int)ConfigKey::last; ++key) {
    const QString &path = KEY_NAMES[(ConfigKey)key];
    const ConfigGroup pathGroup = createPathGroup(path, 0);
    if (!pathGroup.isEmpty())
      group.merge(pathGroup);
  }

  return group;
}

QString ConfigManager::getWritableDir() const {
  return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

} // namespace ConfigManager
