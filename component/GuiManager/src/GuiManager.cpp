#include "GuiManager.h"
#include "MainWindow.h"

#include <ConfigManager.h>
#include <EventManager.h>

#include <QGoodWindow>

#include <QApplication>


#define THREAD_ID "GuiManager"


namespace GuiManager {

GuiManager::GuiManager()
{

}

GuiManager::~GuiManager()
{

}

void GuiManager::start()
{
  QGoodWindow::setup();


  int argc = 1;
  char* argv = const_cast<char*>(ConfigManager::APPLICATION_NAME);
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

  MainWindow w;
  w.show();

  QGoodWindow::setAppDarkTheme();

  EventManager::Event startedEvent(EventManager::EventType::ComponentStarted);
  EventManager::EventManager& eventManager = EventManager::EventManager::getInstance();
  eventManager.send(startedEvent);

  app.exec();
}

void GuiManager::stop()
{
}

}
