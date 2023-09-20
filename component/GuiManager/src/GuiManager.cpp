#include "GuiManager.h"
#include "MainWindow.h"
#include "DarkStyle.h"

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
  char *argv = const_cast<char *>(ConfigManager::APPLICATION_NAME);
  QApplication app(argc, &argv);

  QTranslator translator;
  const QStringList uiLanguages = QLocale::system().uiLanguages();
  for (const QString &locale : uiLanguages) {
    const QString baseName = "Sourcerer_" + QLocale(locale).name();
    if (translator.load(":/i18n/" + baseName)) {
      app.installTranslator(&translator);
      break;
    }
  }

  app.setStyle(new DarkStyle());
  app.style()->setObjectName("fusion");

  app.setAttribute(Qt::AA_UseStyleSheetPropagationInWidgetStyles, true);

  const StylesheetManager::StylesheetManager &stylesheetManager =
      StylesheetManager::StylesheetManager::getInstance();

  const QString &stylesheet =
      stylesheetManager.stylesheet(StylesheetManager::StylesheetKey::Default);

  app.setStyleSheet(stylesheet);

  MainWindow w;
  w.show();

  EventManager::Event startedEvent(EventManager::EventType::ComponentStarted);
  EventManager::EventManager &eventManager =
      EventManager::EventManager::getInstance();
  eventManager.send(startedEvent);

  app.exec();

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
