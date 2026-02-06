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
    {ConfigKey::GitRepoServerConnectTimeout, "gitrepo/server/connectTimeout"},
    {ConfigKey::GitRepoServerTimeout, "gitrepo/server/timeout"},
    {ConfigKey::GitRepoSpinnerOverlayTimerTimeout,
     "gitrepo/spinnerOverlay/timer"},
    {ConfigKey::GitRepoDirWatcherHoldbackTimeout,
     "gitrepo/dirWatcher/holdbackTimeout"},
    {ConfigKey::GitRepoMaxDiffFileSize, "gitrepo/maxDiffFileSize"},
    {ConfigKey::GitRepoMonospaceFontName, "gitrepo/monospace/fontName"},
    {ConfigKey::GitRepoMonospaceFontSize, "gitrepo/monospace/fontSize"},
    {ConfigKey::GitRepoDiffViewType, "gitrepo/diffViewType"},
    {ConfigKey::HttpClientCaCertFile, "network/httpClient/caCertFile"},
    {ConfigKey::HttpClientTimeout, "network/httpClient/timeout"},
    {ConfigKey::CloudJwtExpirationMinutes, "cloud/jwt/expirationMinutes"},
    {ConfigKey::CloudSigninUrl, "cloud/signin/url"},
    {ConfigKey::InfoToastDisplayTime, "window/toasts/infoToastDisplayTime"},
    {ConfigKey::MainWindowBorderWidth, "window/borderWidth"},
    {ConfigKey::MainWindowMinimumWidth, "window/minimumWidth"},
    {ConfigKey::MainWindowMinimumHeight, "window/minimumHeight"},
    {ConfigKey::RecentlyOpenedRepos, "workspace/recentlyOpened"},
    {ConfigKey::SettingsSplitterPos, "window/settings/splitterPos"},
    {ConfigKey::TitleBarCloseButtonColor, "window/titlebar/closeButtonColor"},
    {ConfigKey::TitleBarHeight, "window/titleBarHeight"},
    {ConfigKey::TitleBarMinimizeButtonColor,
     "window/titlebar/minimizeButtonColor"},
    {ConfigKey::TitleBarMaximizeButtonColor,
     "window/titlebar/maximizeButtonColor"},
    {ConfigKey::TitleBarMaxTabWidth, "window/titleBarMaxTabWidth"},
    {ConfigKey::TitleBarMenuMinimumSize, "window/titleBarMenuMinimumWidth"},
    {ConfigKey::TestColor, "test/testColor"},
    {ConfigKey::TestDateTime, "test/testDateTime"},
    {ConfigKey::ToastAnimationDuration, "window/toasts/toastAnimationDuration"},
    {ConfigKey::ToastSpacing, "window/toasts/toastSpacing"},
    {ConfigKey::ToastWidth, "window/toasts/toastWidth"},
    {ConfigKey::TooltipDisplayTime, "window/tooltips/tooltipDisplayTime"}};

QMap<ConfigKey, KeyDescription> KEY_DESCRIPTIONS = {
    {ConfigKey::Unknown, {"", ""}},
    {ConfigKey::GitRepoGitCredentialsExpirationTime,
     {"Credentials expiration time", ""}},
    {ConfigKey::GitRepoServerConnectTimeout,
     {"Server connection timeout in milliseconds", ""}},
    {ConfigKey::GitRepoServerTimeout,
     {"Server operation timeout in milliseconds", ""}},
    {ConfigKey::GitRepoMonospaceFontName, {"Monospace font name", ""}},
    {ConfigKey::GitRepoMonospaceFontSize, {"Monospace font size in pixels"}},
    {ConfigKey::RecentlyOpenedRepos, {"Recently opened repositories list", ""}},
    {ConfigKey::TitleBarHeight, {"Maximum title bar height", ""}},
    {ConfigKey::TitleBarMaxTabWidth, {"Maximum width of a title bar tab", ""}},
    {ConfigKey::TitleBarMenuMinimumSize,
     {"Minimum size of the title bar menu", ""}},
    {ConfigKey::CloudJwtExpirationMinutes,
     {"Cloud JWT token expiration time in minutes", ""}},
    {ConfigKey::CloudSigninUrl, {"Cloud service signin URL", ""}},
    {ConfigKey::TestColor, {"Test color", "Test color description"}},
    {ConfigKey::TestDateTime, {"Test dateTime", "Test dateTime description"}}};

QMap<ConfigKey, bool> DEFAULT_BOOLEANS = {
    {ConfigKey::GitRepoDiffViewType, false}}; // false = unified, true = side-by-side

QMap<ConfigKey, QColor> DEFAULT_COLORS = {
    {ConfigKey::TestColor, QColor(10, 10, 10)},
    {ConfigKey::TitleBarCloseButtonColor, QColor(0xff, 0x00, 0x00)},
    {ConfigKey::TitleBarMinimizeButtonColor, QColor(0x4b, 0x4b, 0x4b)},
    {ConfigKey::TitleBarMaximizeButtonColor, QColor(0x4b, 0x4b, 0x4b)}};

QMap<ConfigKey, QDateTime> DEFAULT_DATETIMES = {
    {ConfigKey::TestDateTime, QDateTime::currentDateTime()}};

QMap<ConfigKey, qlonglong> DEFAULT_LONGS = {
    {ConfigKey::CloudJwtExpirationMinutes, 7 * 24 * 60}, // 7 days in minutes
    {ConfigKey::GitRepoGitCredentialsExpirationTime,
     2592000000LL}, // 30 days in milliseconds
    {ConfigKey::GitRepoServerConnectTimeout, 30000LL}, // 30 seconds
    {ConfigKey::GitRepoServerTimeout, 30000LL},        // 30 seconds
    {ConfigKey::GitRepoSpinnerOverlayTimerTimeout, 1000},
    {ConfigKey::GitRepoDirWatcherHoldbackTimeout, 2000},
    {ConfigKey::GitRepoMaxDiffFileSize, 1 * 1024 * 1024}, // 1 MB
    {ConfigKey::GitRepoMonospaceFontSize, 10},
    {ConfigKey::HttpClientTimeout, 30000}, // 30 seconds
    {ConfigKey::InfoToastDisplayTime, 8000},
    {ConfigKey::MainWindowBorderWidth, 2},
    {ConfigKey::MainWindowMinimumWidth, 800},
    {ConfigKey::MainWindowMinimumHeight, 600},
    {ConfigKey::TitleBarHeight, 40},
    {ConfigKey::TitleBarMaxTabWidth, 285},
    {ConfigKey::TitleBarMenuMinimumSize, 230},
    {ConfigKey::ToastAnimationDuration, 500},
    {ConfigKey::ToastHeight, 150},
    {ConfigKey::ToastSpacing, 4},
    {ConfigKey::ToastWidth, 400},
    {ConfigKey::TooltipDisplayTime, 5000}};

QMap<ConfigKey, QString> DEFAULT_STRINGS = {
#ifdef QT_DEBUG
    {ConfigKey::CloudSigninUrl, "http://localhost:8080/api/v1/signin"},
    {ConfigKey::CloudSignupUrl, "http://localhost:8080/#/signup"},
#else
    {ConfigKey::CloudSigninUrl, "http://localhost:8080/api/v1/signin"},
    {ConfigKey::CloudSignupUrl, "http://localhost:8080/#/signup"},
#endif
    {ConfigKey::GitRepoSplitterPos, ""},
    {ConfigKey::GitRepoBranchRecyclerViewItemsSplitterPos, ""},
    {ConfigKey::GitRepoMonospaceFontName, "Lucida Console"},

    {ConfigKey::HttpClientCaCertFile, "ca-certificates.crt"},
    {ConfigKey::RecentlyOpenedRepos, ""},
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

QString ConfigManager::getString(const QString &key) const {
  LOCKGUARD(_mutex);
  if (_settings.contains(key))
    return _settings.value(key).toString();
  else
    return QString();
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

void ConfigManager::setString(const QString &key, const QString &value) {
  LOCKGUARD(_mutex);
  _settings.setValue(key, value);
}

void ConfigManager::removeString(const QString &key) {
  LOCKGUARD(_mutex);
  _settings.remove(key);
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
  const qsizetype index = path.indexOf('/', from);
  if (index == -1)
    return ConfigGroup();

  if (const qsizetype lastIndex = path.lastIndexOf('/'); index == lastIndex) {
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
    if (const ConfigGroup pathGroup = createPathGroup(path, index + 1);
        !pathGroup.isEmpty())
      group.merge(pathGroup);

    return group;
  }
}

ConfigGroup ConfigManager::getGroupped() const {
  ConfigGroup group;

  for (int key = static_cast<int>(ConfigKey::Unknown);
       key < static_cast<int>(ConfigKey::last); ++key) {
    const QString &path = KEY_NAMES[static_cast<ConfigKey>(key)];
    if (const ConfigGroup pathGroup = createPathGroup(path, 0);
        !pathGroup.isEmpty())
      group.merge(pathGroup);
  }

  return group;
}

QString ConfigManager::getWritableDir() {
  return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

} // namespace ConfigManager
