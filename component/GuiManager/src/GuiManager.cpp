#include "GuiManager.h"
#include "DarkStyle.h"
#include "MainWindow.h"
#include "MousePositionCorrection.h"
#include "WebEngineProfile.h"

#include <ConfigManager.h>
#include <EventManager.h>
#include <Log.h>
#include <QStyle>
#include <StylesheetManager.h>

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QSharedPointer>
#include <QStandardPaths>
#include <QTimer>
#include <QTranslator>
#if defined(QT_WEBENGINECORE_LIB)
#include <QtWebEngineCore/QWebEngineCookieStore>
#include <QtWebEngineCore/QWebEngineProfile>
#endif

#define THREAD_ID "GuiManager"

namespace GuiManager {

#if defined(QT_WEBENGINECORE_LIB)
QWebEngineProfile *sharedWebEngineProfile() {
  static QSharedPointer<QWebEngineProfile> profile;
  if (profile) {
    return profile.get();
  }

  const QString storageBaseDir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (storageBaseDir.isEmpty()) {
    return nullptr;
  }

  const QString storagePath =
      QDir(storageBaseDir).absoluteFilePath(QStringLiteral("webengine"));
  QDir().mkpath(storagePath);

  profile = QSharedPointer<QWebEngineProfile>::create(
      QStringLiteral("sourcerer"), qApp);
  profile->setPersistentStoragePath(storagePath);
  profile->setCachePath(
      QDir(storagePath).absoluteFilePath(QStringLiteral("cache")));
  profile->setPersistentCookiesPolicy(
      QWebEngineProfile::PersistentCookiesPolicy::ForcePersistentCookies);
  profile->setHttpCacheType(QWebEngineProfile::HttpCacheType::DiskHttpCache);

  return profile.get();
}
#endif

} // namespace GuiManager

namespace GuiManager {

GuiManager::GuiManager() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

GuiManager::~GuiManager() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

void GuiManager::start() {
  LOG_PERF(0, "Enter");

  /*
  int argc = 1;
  auto argv = const_cast<char *>(ConfigManager::APPLICATION_NAME);
  QApplication app(argc, &argv);
*/
  const QString appDir = QCoreApplication::applicationDirPath();
  QCoreApplication::addLibraryPath(appDir);
  QCoreApplication::addLibraryPath(appDir + "/plugins");

#if defined(QT_WEBENGINECORE_LIB)
  if (QWebEngineProfile *profile = sharedWebEngineProfile()) {
    if (profile->cookieStore()) {
      QEventLoop loop;
      QTimer timer;
      timer.setSingleShot(true);

      QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
      profile->cookieStore()->loadAllCookies();
      timer.start(1500);
      loop.exec();
    }
  }
#endif

  MousePositionCorrection::instance().install(*qApp);

  QTranslator translator;
  const QStringList uiLanguages = QLocale::system().uiLanguages();
  for (const QString &locale : uiLanguages) {
    if (const QString baseName = "Sourcerer_" + QLocale(locale).name();
        translator.load(":/i18n/" + baseName)) {
      QApplication::installTranslator(&translator);
      break;
    }
  }

  QApplication::setWindowIcon(QIcon(":/icons/sorcerer-hat.png"));
  QApplication::setAttribute(Qt::AA_DontShowIconsInMenus, false);

  QApplication::setStyle(new DarkStyle());
  QApplication::style()->setObjectName("fusion");

  QApplication::setAttribute(Qt::AA_UseStyleSheetPropagationInWidgetStyles,
                             true);

  ConfigManager::ConfigManager &configManager =
      ConfigManager::ConfigManager::getInstance();
  const QString themeName =
      configManager.getString(ConfigManager::ConfigKey::UiTheme);
  const StylesheetManager::StylesheetKey themeKey =
      StylesheetManager::StylesheetManager::stylesheetKeyFromName(themeName);
  StylesheetManager::StylesheetManager::setCurrentStylesheet(themeKey);
  StylesheetManager::StylesheetManager::applyStylesheet(qApp);

  const auto w = QSharedPointer<MainWindow>::create();
  const QString savedGeometry =
      configManager.getString(ConfigManager::ConfigKey::MainWindowGeometry);
  if (savedGeometry.isEmpty()) {
    w->showMaximized();
  } else {
    w->show();
  }

  EventManager::Event startedEvent(EventManager::EventType::ComponentStarted);
  EventManager::EventManager &eventManager =
      EventManager::EventManager::getInstance();
  eventManager.send(startedEvent);

  QApplication::exec();

  LOG_PERF(0, "Exit");
}

void GuiManager::stop() {
  LOG_PERF(0, "Enter");

  EventManager::Event stoppedEvent(EventManager::EventType::ComponentStopped);
  EventManager::EventManager &eventManager =
      EventManager::EventManager::getInstance();
  eventManager.send(stoppedEvent);

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager
