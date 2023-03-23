#include "MainWindow.h"
#include "containers/HomeContainer.h"
#include "containers/PleaseWaitContainer.h"
#include "dialogs/AboutDialog.h"
#include "widgets/TitlebarWidget.h"

#include <ConfigKey.h>
#include <Log.h>

#define THREAD_ID "MainWindow"

namespace GuiManager {

using namespace Containers;
using namespace Dialogs;

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

  connect(_titleBarWidget, &TitlebarWidget::home, this, &MainWindow::onHome);
  connect(_titleBarWidget, &TitlebarWidget::open, this, &MainWindow::onOpen);
  connect(_titleBarWidget, &TitlebarWidget::clone, this, &MainWindow::onClone);
  connect(_titleBarWidget, &TitlebarWidget::init, this, &MainWindow::onInit);
  connect(_titleBarWidget, &TitlebarWidget::settings, this,
          &MainWindow::onSettings);
  connect(_titleBarWidget, &TitlebarWidget::about, this, &MainWindow::onAbout);

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

void MainWindow::onHome() {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

void MainWindow::onOpen() {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

void MainWindow::onClone() {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

void MainWindow::onInit() {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

void MainWindow::onSettings() {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

void MainWindow::onAbout() {
  LOG_PERF(0, "Enter");

  AboutDialog aboutDlg;
  aboutDlg.move(x() + (width() - aboutDlg.width()) / 2,
                y() + (height() - aboutDlg.height()) / 2);
  aboutDlg.exec();

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager
