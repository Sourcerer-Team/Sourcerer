#include "ConfigManager.h"
#include "../../GitVersion.h"

#include <QColor>
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
    {ConfigKey::CloudJwtExpirationMinutes, "cloud/jwt/expirationMinutes"},
    {ConfigKey::CloudJwtPublicKeyPath, "cloud/jwt/publicKeyPath"},
    {ConfigKey::CloudSigninUrl, "cloud/signin/url"},
    {ConfigKey::CloudSignupUrl, "cloud/signup/url"},
    {ConfigKey::CloudSignoutUrl, "cloud/signout/url"},
    {ConfigKey::CloudUploadBugReportUrl, "cloud/uploadBugReport/url"},
    {ConfigKey::NewsTrackerUrl, "network/newsTracker/url"},
    {ConfigKey::NewsTrackerPollInterval, "network/newsTracker/pollInterval"},
    {ConfigKey::NewsTrackerSeenNewsIds, "network/newsTracker/seenNewsIds"},
    {ConfigKey::GitRepoSplitterPos, "window/gitrepo/splitterPos"},
    {ConfigKey::GitRepoBranchRecyclerViewItemsSplitterPos,
     "window/gitrepo/branchRecyclerViewItemsSplitterPos"},
    {ConfigKey::GitRepoHistoryViewColumnWidths,
     "window/gitrepo/historyViewColumnWidths"},
    {ConfigKey::GitRepoHistoryViewHiddenColumns,
     "window/gitrepo/historyViewHiddenColumns"},
    {ConfigKey::GitRepoGitCredentialsExpirationTime,
     "gitrepo/credentials/credentialsExpirationTime"},
    {ConfigKey::GitRepoServerConnectTimeout, "gitrepo/server/connectTimeout"},
    {ConfigKey::GitRepoServerTimeout, "gitrepo/server/timeout"},
    {ConfigKey::GitRepoSigningEnabled, "signing/enabled"},
    {ConfigKey::GitRepoSigningFormat, "signing/format"},
    {ConfigKey::GitRepoSigningKey, "signing/key"},
    {ConfigKey::GitRepoSigningProgram, "signing/program"},
    {ConfigKey::GitRepoSpinnerOverlayTimerTimeout,
     "gitrepo/spinnerOverlay/timer"},
    {ConfigKey::GitRepoDirWatcherHoldbackTimeout,
     "gitrepo/dirWatcher/holdbackTimeout"},
    {ConfigKey::GitRepoMaxDiffFileSize, "gitrepo/maxDiffFileSize"},
    {ConfigKey::GitRepoMonospaceFontName, "UI/monospace/fontName"},
    {ConfigKey::GitRepoMonospaceFontSize, "UI/monospace/fontSize"},
    {ConfigKey::GitRepoDiffViewType, "gitrepo/diffViewType"},
    {ConfigKey::HttpClientCaCertFile, "network/httpClient/caCertFile"},
    {ConfigKey::HttpClientTimeout, "network/httpClient/timeout"},
    {ConfigKey::InfoToastDisplayTime, "window/toasts/infoToastDisplayTime"},
    {ConfigKey::MainWindowBorderWidth, "window/borderWidth"},
    {ConfigKey::MainWindowGeometry, "window/geometry"},
    {ConfigKey::MainWindowMinimumWidth, "window/minimumWidth"},
    {ConfigKey::MainWindowMinimumHeight, "window/minimumHeight"},
    {ConfigKey::RecentlyOpenedRepos, "workspace/recentlyOpened"},
    {ConfigKey::UiTheme, "window/uiTheme"},
    {ConfigKey::SettingsSplitterPos, "window/settings/splitterPos"},
    {ConfigKey::TitleBarHeight, "window/titleBarHeight"},
    {ConfigKey::TitleBarMaxTabWidth, "window/titleBarMaxTabWidth"},
    {ConfigKey::TitleBarMenuMinimumSize, "window/titleBarMenuMinimumWidth"},
    {ConfigKey::TestColor, "test/testColor"},
    {ConfigKey::TestDateTime, "test/testDateTime"},
    {ConfigKey::ToastAnimationDuration, "window/toasts/toastAnimationDuration"},
    {ConfigKey::ToastSpacing, "window/toasts/toastSpacing"},
    {ConfigKey::ToastWidth, "window/toasts/toastWidth"},
    {ConfigKey::TooltipDisplayTime, "window/tooltips/tooltipDisplayTime"},
    {ConfigKey::TooltipHistoryTimeout,
     "window/tooltips/tooltipHistoryTimeout"}};

QMap<ConfigKey, KeyDescription> KEY_DESCRIPTIONS = {
    {ConfigKey::Unknown, {"", ""}},
    {ConfigKey::GitRepoSigningEnabled,
     {"Commit signing enabled",
      "Show the Sign commit checkbox in commit panels"}},
    {ConfigKey::GitRepoSigningFormat, {"Commit signing format", "gpg or ssh"}},
    {ConfigKey::GitRepoSigningKey,
     {"Commit signing key", "Key ID/fingerprint or SSH private key path"}},
    {ConfigKey::GitRepoSigningProgram,
     {"Commit signing program", "Optional path to the signing executable"}},
    {ConfigKey::GitRepoMonospaceFontName, {"Monospace font name", ""}},
    {ConfigKey::GitRepoMonospaceFontSize, {"Monospace font size in pixels"}},
    {ConfigKey::NewsTrackerUrl,
     {"News feed URL", "URL polled for application news"}},
    {ConfigKey::NewsTrackerPollInterval,
     {"News feed poll interval", "Polling interval in milliseconds"}},
    {ConfigKey::NewsTrackerSeenNewsIds,
     {"Seen news ids", "Comma-separated ids already shown to the user"}}};

QMap<ConfigKey, bool> DEFAULT_BOOLEANS = {
    {ConfigKey::GitRepoDiffViewType,
     false}, // false = unified, true = side-by-side
    {ConfigKey::GitRepoSigningEnabled, true}};

QMap<ConfigKey, QColor> DEFAULT_COLORS = {
    {ConfigKey::TestColor, QColor(10, 10, 10)},
};

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
    {ConfigKey::NewsTrackerPollInterval, 300000},
    {ConfigKey::ToastAnimationDuration, 500},
    {ConfigKey::ToastHeight, 150},
    {ConfigKey::ToastSpacing, 4},
    {ConfigKey::ToastWidth, 400},
    {ConfigKey::TooltipDisplayTime, 5000},
    {ConfigKey::TooltipHistoryTimeout, 3000}};

QMap<ConfigKey, QString> DEFAULT_STRINGS = {
    {ConfigKey::CloudJwtPublicKeyPath, "keys/cloud_jwt_public_key.pem"},
    {ConfigKey::GitRepoSigningFormat, "ssh"},
    {ConfigKey::GitRepoSigningProgram, "ssh-keygen"},
#ifdef QT_DEBUG
    {ConfigKey::CloudSigninUrl, "https://www.sourcererapp.com/api/v1/signin"},
    {ConfigKey::CloudSignupUrl, "https://www.sourcererapp.com/#/signup"},
    {ConfigKey::CloudSignoutUrl, "https://www.sourcererapp.com/api/v1/logout"},
    {ConfigKey::CloudUploadBugReportUrl,
     "https://www.sourcererapp.com/api/v1/upload-bug-report"},
    {ConfigKey::NewsTrackerUrl, "https://www.sourcererapp.com/api/v1/news"},
#else
    {ConfigKey::CloudSigninUrl, "https://www.sourcererapp.com/api/v1/signin"},
    {ConfigKey::CloudSignupUrl, "https://www.sourcererapp.com/#/signup"},
    {ConfigKey::CloudSignoutUrl, "https://www.sourcererapp.com/api/v1/logout"},
    {ConfigKey::CloudUploadBugReportUrl,
     "https://www.sourcererapp.com/api/v1/upload-bug-report"},
    {ConfigKey::NewsTrackerUrl, "https://www.sourcererapp.com/api/v1/news"},
#endif
    {ConfigKey::NewsTrackerSeenNewsIds, ""},
    {ConfigKey::GitRepoSplitterPos,
     "000000ff0000000100000003000000c0000003d30000014101ffffffff010000000100"},
    {ConfigKey::GitRepoBranchRecyclerViewItemsSplitterPos, ""},
    {ConfigKey::GitRepoHistoryViewColumnWidths, ""},
    {ConfigKey::GitRepoHistoryViewHiddenColumns, "2,3"},
    {ConfigKey::GitRepoMonospaceFontName, "Lucida Console"},

    {ConfigKey::HttpClientCaCertFile, "ca-certificates.crt"},
    {ConfigKey::RecentlyOpenedRepos, ""},
    {ConfigKey::UiTheme, "SourcererDark"},
    {ConfigKey::MainWindowGeometry, ""},
    {ConfigKey::GitRepoSigningKey, ""},
    {ConfigKey::SettingsSplitterPos, ""}};

static QString resolveSigningProgramDefault(const QString &signingFormat) {
  if (signingFormat.trimmed().compare(QStringLiteral("ssh"),
                                      Qt::CaseInsensitive) == 0) {
    return QStringLiteral("ssh-keygen");
  }

  return QStringLiteral("gpg");
}

static QString defaultStringForKey(ConfigKey key, const QSettings *settings) {
  if (key == ConfigKey::GitRepoSigningProgram) {
    const QString formatKey = KEY_NAMES.value(ConfigKey::GitRepoSigningFormat);
    const QString signingFormat =
        settings && settings->contains(formatKey)
            ? settings->value(formatKey).toString()
            : DEFAULT_STRINGS.value(ConfigKey::GitRepoSigningFormat);
    return resolveSigningProgramDefault(signingFormat);
  }

  return DEFAULT_STRINGS.value(key);
}

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
  const QString keyName = KEY_NAMES.value(key);
  if (_settings.contains(keyName)) {
    const QString value = _settings.value(keyName).toString();
    if (key == ConfigKey::GitRepoSigningProgram && value.trimmed().isEmpty()) {
      return defaultStringForKey(key, &_settings);
    }

    return value;
  }

  return defaultStringForKey(key, &_settings);
}

QString ConfigManager::getString(const QString &key) const {
  LOCKGUARD(_mutex);
  if (_settings.contains(key)) {
    const QString value = _settings.value(key).toString();
    if (getKey(key) == ConfigKey::GitRepoSigningProgram &&
        value.trimmed().isEmpty()) {
      return defaultStringForKey(ConfigKey::GitRepoSigningProgram, &_settings);
    }

    return value;
  }

  if (getKey(key) == ConfigKey::GitRepoSigningProgram) {
    return defaultStringForKey(ConfigKey::GitRepoSigningProgram, &_settings);
  }

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
    const int orderNumber = static_cast<int>(key);
    if (name.isEmpty())
      return ConfigGroup();

    ConfigItem item;
    item.setKey(path);
    item.setName(name);
    item.setDescription(description);
    item.setOrderNumber(orderNumber);

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
