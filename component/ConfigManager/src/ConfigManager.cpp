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

struct KeyDescription {
  QString name;
  QString description;
};

QMap<ConfigKey, QString> KEY_NAMES = {
    {ConfigKey::Unknown, ""},
    {ConfigKey::GitRepoSplitterPos, "window/gitrepo/splitterPos"},
    {ConfigKey::GitRepoBranchRecyclerViewItemsSplitterPos,
     "window/gitrepo/branchRecyclerViewItemsSplitterPos"},
    {ConfigKey::GitRepoGitCredentialsExpirationTime,
     "gitrepo/credentials/credentialsExpirationTime"},
    {ConfigKey::InfoToastDisplayTime, "window/toasts/infoToastDisplayTime"},
    {ConfigKey::MainWindowBorderWidth, "window/borderWidth"},
    {ConfigKey::RecentlyOpenedRepos, "workspace/recentlyOpened"},
    {ConfigKey::SettingsSplitterPos, "window/settings/splitterPos"},
    {ConfigKey::TitleBarCloseButtonColor, "window/titlebar/closeButtonColor"},
    {ConfigKey::TitleBarHeight, "window/titleBarHeight"},
    {ConfigKey::TitleBarMinimizeButtonColor,
     "window/titlebar/minimizeButtonColor"},
    {ConfigKey::TitleBarMaximizeButtonColor,
     "window/titlebar/maximizeButtonColor"},
    {ConfigKey::TitleBarMaxTabWidth, "window/titleBarMaxTabWidth"},
    {ConfigKey::TitleBarMenuMinimumSize, "window/xxx/titleBarMenuMinimumWidth"},
    {ConfigKey::TestColor, "test/testColor"},
    {ConfigKey::TestDateTime, "test/testDateTime"},
    {ConfigKey::ToastSpacing, "window/toasts/toastSpacing"},
    {ConfigKey::ToastWidth, "window/toasts/toastWidth"}};

QMap<ConfigKey, KeyDescription> KEY_DESCRIPTIONS = {
    {ConfigKey::Unknown, {"", ""}},
    {ConfigKey::GitRepoGitCredentialsExpirationTime,
     {"Credentials expiration time", ""}},
    {ConfigKey::RecentlyOpenedRepos, {"Recently opened repositories list", ""}},
    {ConfigKey::TitleBarHeight, {"Maximum title bar height", ""}},
    {ConfigKey::TitleBarMaxTabWidth, {"Maximum width of a title bar tab", ""}},
    {ConfigKey::TitleBarMenuMinimumSize,
     {"Minimum size of the title bar menu", ""}},
    {ConfigKey::TestColor, {"Test color", "Test color description"}},
    {ConfigKey::TestDateTime, {"Test dateTime", "Test dateTime description"}}};

QMap<ConfigKey, bool> DEFAULT_BOOLEANS = {};

QMap<ConfigKey, QColor> DEFAULT_COLORS = {
    {ConfigKey::TestColor, QColor(10, 10, 10)},
    {ConfigKey::TitleBarCloseButtonColor, QColor(0xff, 0x00, 0x00)},
    {ConfigKey::TitleBarMinimizeButtonColor, QColor(0x4b, 0x4b, 0x4b)},
    {ConfigKey::TitleBarMaximizeButtonColor, QColor(0x4b, 0x4b, 0x4b)}};

QMap<ConfigKey, QDateTime> DEFAULT_DATETIMES = {
    {ConfigKey::TestDateTime, QDateTime::currentDateTime()}};

QMap<ConfigKey, qlonglong> DEFAULT_LONGS = {
    {ConfigKey::GitRepoGitCredentialsExpirationTime, 30000},
    {ConfigKey::InfoToastDisplayTime, 3000},
    {ConfigKey::MainWindowBorderWidth, 2},
    {ConfigKey::TitleBarHeight, 40},
    {ConfigKey::TitleBarMaxTabWidth, 285},
    {ConfigKey::TitleBarMenuMinimumSize, 230},
    {ConfigKey::ToastSpacing, 4},
    {ConfigKey::ToastWidth, 400}};

QMap<ConfigKey, QString> DEFAULT_STRINGS = {
    {ConfigKey::GitRepoSplitterPos, ""},
    {ConfigKey::GitRepoBranchRecyclerViewItemsSplitterPos, ""},
    {ConfigKey::RecentlyOpenedRepos,
     "git@github.com:levi9-embedded/"
     "components.git,git@github.com:levi9-embedded/arduino-projects.git"},
    {ConfigKey::SettingsSplitterPos, ""}};

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

bool ConfigManager::getBoolean(ConfigKey key) const {
  LOCKGUARD(_mutex);
  if (_settings.contains(KEY_NAMES[key]))
    return _settings.value(KEY_NAMES[key]).toBool();
  else
    return DEFAULT_BOOLEANS[key];
}

QColor ConfigManager::getColor(ConfigKey key) const {
  LOCKGUARD(_mutex);
  if (_settings.contains(KEY_NAMES[key]))
    return QColor::fromString(_settings.value(KEY_NAMES[key]).toString());
  else
    return DEFAULT_COLORS[key];
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

void ConfigManager::setBoolean(ConfigKey key, bool value) {
  LOCKGUARD(_mutex);
  _settings.setValue(KEY_NAMES[key], value);
}

void ConfigManager::setColor(ConfigKey key, const QColor &value) {
  LOCKGUARD(_mutex);
  _settings.setValue(KEY_NAMES[key], value.name());
}

void ConfigManager::setDateTime(ConfigKey key, const QDateTime &value) {
  LOCKGUARD(_mutex);
  _settings.setValue(KEY_NAMES[key], value);
}

void ConfigManager::setLong(ConfigKey key, qlonglong value) {
  LOCKGUARD(_mutex);
  _settings.setValue(KEY_NAMES[key], value);
}

void ConfigManager::setString(ConfigKey key, const QString &value) {
  LOCKGUARD(_mutex);
  _settings.setValue(KEY_NAMES[key], value);
}

void ConfigManager::save() {
  LOCKGUARD(_mutex);
  _settings.sync();
}

ConfigKey ConfigManager::getKey(const QString &keyPath) const {
  return KEY_NAMES.key(keyPath);
}

QString ConfigManager::getLogsDir() const {
  return _logsDir;
}

QString ConfigManager::getAppVersion() const {
  return APPLICATION_VERSION;
}

static ConfigGroup createPathGroup(const QString &path, size_t from) {
  const int index = path.indexOf('/', from);
  if (index == -1)
    return ConfigGroup();

  const int lastIndex = path.lastIndexOf('/');

  if (index == lastIndex) {
    const ConfigKey key = KEY_NAMES.key(path);
    const QString &name = KEY_DESCRIPTIONS[key].name;
    const QString &description = KEY_DESCRIPTIONS[key].description;
    if (name.isEmpty())
      return ConfigGroup();

    ConfigItem item;
    item.setKey(path);
    item.setName(name);
    item.setDescription(description);

    if (DEFAULT_BOOLEANS.contains(key)) {
      item.setType(ConfigKeyType::Boolean);
      item.setValue(DEFAULT_BOOLEANS[key]);
    } else if (DEFAULT_COLORS.contains(key)) {
      item.setType(ConfigKeyType::Color);
      item.setValue(DEFAULT_COLORS[key]);
    } else if (DEFAULT_DATETIMES.contains(key)) {
      item.setType(ConfigKeyType::DateTime);
      item.setValue(DEFAULT_DATETIMES[key]);
    } else if (DEFAULT_LONGS.contains(key)) {
      item.setType(ConfigKeyType::Long);
      item.setValue(DEFAULT_LONGS[key]);
    } else if (DEFAULT_STRINGS.contains(key)) {
      item.setType(ConfigKeyType::String);
      item.setValue(DEFAULT_STRINGS[key]);
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
