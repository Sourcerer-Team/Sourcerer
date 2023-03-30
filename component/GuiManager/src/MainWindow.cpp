#include "MainWindow.h"
#include <containers/GitRepoContainer.h>
#include "containers/HomeContainer.h"
#include <containers/PleaseWaitContainer.h>
#include "containers/SettingsContainer.h"
#include "dialogs/AboutDialog.h"
#include "widgets/TitlebarWidget.h"

#include <ConfigKey.h>
#include <GitRepo.h>
#include <Log.h>

#include <QTimer>

#define THREAD_ID "MainWindow"

namespace GuiManager {

using namespace Containers;
using namespace Dialogs;
using namespace Util;

const int TOP_MARGIN_OFFSET = 10;

MainWindow::MainWindow(QWidget *parent)
    : QGoodWindow(parent), _current(-1), _titleBarWidget(new TitlebarWidget),
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
  connect(_titleBarWidget, &TitlebarWidget::closedTab, this,
          &MainWindow::onClosedTab);
  connect(_titleBarWidget, &TitlebarWidget::selectedTab, this,
          &MainWindow::onSelectedTab);
  connect(_titleBarWidget, &TitlebarWidget::resizeTabBar, this,
          &MainWindow::onTabBarResize);

  addHomeContainer();

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

  addSettingsContainer();

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

void MainWindow::onClosedTab(int index) {

  LOG_PERF(0, "Enter");
  LOG_DEBUG("index is %1.", index);

  if (index < 0 || index >= _containers.length()) {
    LOG_WARN("Invalid tab index %1.", index);
    LOG_PERF(0, "Exit");
    return;
  }

  delete _containers[index];
  _containers.remove(index);
  _current = -1;

  if (_containers.length() == 0)
    addHomeContainer();

  _titleBarWidget->removeTab(index);

  LOG_PERF(0, "Exit");
}

void MainWindow::onSelectedTab(int index) {
  LOG_PERF(0, "Enter");

  selectContainer(index);

  LOG_PERF(0, "Exit");
}

void MainWindow::onTabBarResize() {
  LOG_PERF(0, "Enter");

  _centralWidget->updateWindow();

  LOG_PERF(0, "Exit");
}

void MainWindow::addGitRepoContainer(const QString &path)
{
  LOG_PERF(0, "Enter");

  const qlonglong titleBarHeight =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarHeight);
  ContainerWidget *container = new GitRepoContainer(path, this);
  container->setContentsMargins(0, titleBarHeight - TOP_MARGIN_OFFSET, 0, 0);
  _containers += container;
  _titleBarWidget->addTab(GitRepo::parseRepoName(path));
  _titleBarWidget->selectTab(_titleBarWidget->tabsCount() - 1);

  LOG_PERF(0, "Exit");
}

void MainWindow::addHomeContainer() {
  LOG_PERF(0, "Enter");

  const qlonglong titleBarHeight =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarHeight);
  ContainerWidget *container = new HomeContainer(this);
  container->setContentsMargins(0, titleBarHeight - TOP_MARGIN_OFFSET, 0, 0);
  _containers += container;
  _titleBarWidget->addTab(tr("Home"));
  _titleBarWidget->selectTab(_titleBarWidget->tabsCount() - 1);

  LOG_PERF(0, "Exit");
}

void MainWindow::addSettingsContainer() {
  LOG_PERF(0, "Enter");

  const qlonglong titleBarHeight =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarHeight);
  ContainerWidget *container = new SettingsContainer(this);
  container->setContentsMargins(0, titleBarHeight - TOP_MARGIN_OFFSET, 0, 0);
  _containers += container;
  _titleBarWidget->addTab(tr("Settings"));
  _titleBarWidget->selectTab(_titleBarWidget->tabsCount() - 1);

  LOG_PERF(0, "Exit");
}

void MainWindow::selectContainer(int index) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("index is %1.", index);

  if (index < 0 || index >= _containers.length()) {
    LOG_WARN("Invalid tab index %1.", index);
    LOG_PERF(0, "Exit");
    return;
  }

  if (_current != -1)
    _containers[_current]->hide();

  _containers[index]->show();
  _centralWidget->setCentralWidget(_containers[index]);
  _current = index;

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager
