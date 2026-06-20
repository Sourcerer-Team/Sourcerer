#include "GuiManager.h"
#include "DarkStyle.h"
#include "MainWindow.h"
#include "MousePositionCorrection.h"

#include <ConfigManager.h>
#include <EventManager.h>
#include <Log.h>
#include <QStyle>
#include <StylesheetManager.h>

#include <QApplication>
#include <QTranslator>

#define THREAD_ID "GuiManager"

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

  int argc = 1;
  auto argv = const_cast<char *>(ConfigManager::APPLICATION_NAME);
  QApplication app(argc, &argv);

  MousePositionCorrection::instance().install(app);

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
  StylesheetManager::StylesheetManager::applyStylesheet(&app);

  const auto w = QSharedPointer<MainWindow>::create();
  w->showMaximized();

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
