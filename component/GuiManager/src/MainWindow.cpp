#include "MainWindow.h"
#include "containers/HomeContainer.h"
#include "containers/PleaseWaitContainer.h"
#include "widgets/TitlebarWidget.h"

#include <ConfigKey.h>
#include <Log.h>

#define THREAD_ID "MainWindow"

namespace GuiManager {

using namespace Containers;

MainWindow::MainWindow(QWidget *parent)
    : QGoodWindow(parent), _current(0), _titleBarWidget(new TitlebarWidget),
      _centralWidget(new QGoodCentralWidget(this)),
      _configManager(ConfigManager::ConfigManager::getInstance()) {
  LOG_PERF(0, "Enter");

  const qlonglong titleBarHeight =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarHeight);

  LOG_INFO("TitleBar height is %1 pixels.", titleBarHeight);

  _centralWidget->setIconVisible(false);
  _centralWidget->setTitleVisible(false);
  _centralWidget->setTitleBarHeight(titleBarHeight);
  _centralWidget->setUnifiedTitleBarAndCentralWidget(true);
  _centralWidget->setLeftTitleBarWidget(_titleBarWidget);
  setCentralWidget(_centralWidget);

  HomeContainer *homeContainer = new HomeContainer;

  _containers += homeContainer;

  PleaseWaitContainer *pleaseWaitContainer = new PleaseWaitContainer;

  _containers += pleaseWaitContainer;

  _centralWidget->setCentralWidget(homeContainer);

  _titleBarWidget->addTab(tr("Home"));

  LOG_PERF(0, "Exit");
}

MainWindow::~MainWindow() {
  LOG_PERF(0, "Enter");

  for (const auto &container : _containers) {
    delete container;
  }

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager
