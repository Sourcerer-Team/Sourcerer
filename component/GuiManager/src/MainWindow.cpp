#include "MainWindow.h"
#include "containers/CloneGitRepoContainer.h"
#include "containers/GitRepoContainer.h"
#include "containers/HomeContainer.h"
#include "containers/InitGitRepoContainer.h"
#include "containers/SettingsContainer.h"
#include "dialogs/AboutDialog.h"
#include "widgets/GitCredentialsToast.h"
#include "widgets/GitProgressToast.h"
#include "widgets/InfoToast.h"
#include "widgets/TitlebarWidget.h"

#include <ConfigKey.h>
#include <Dispatch.h>
#include <Git.h>
#include <Log.h>

#include <QFileDialog>
#include <QStandardPaths>
#include <QTimer>

#include <functional>

#define THREAD_ID "MainWindow"

namespace GuiManager {

using namespace Containers;
using namespace Dialogs;
using namespace Util;

const int TOP_MARGIN_OFFSET = 10;

MainWindow::MainWindow(QWidget *parent)
    : QGoodWindow(parent), _current(-1), _titleBarWidget(new TitlebarWidget),
      _toastHost(ToastHostWidget::getInstance()),
      _centralWidget(new QGoodCentralWidget(this)),
      _configManager(ConfigManager::ConfigManager::getInstance()),
      _eventManager(EventManager::EventManager::getInstance()) {
  LOG_PERF(0, "Enter");

  const qlonglong titleBarHeight =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarHeight);

  LOG_DEBUG("TitleBar height is {} pixels.", titleBarHeight);

  _centralWidget->setIconVisible(false);
  _centralWidget->setTitleVisible(false);
  _centralWidget->setTitleBarHeight(titleBarHeight);
  _centralWidget->setUnifiedTitleBarAndCentralWidget(true);
  _centralWidget->setLeftTitleBarWidget(_titleBarWidget);
  setCentralWidget(_centralWidget);

  _toastHost.setParent(this);
  _toastHost.setYOffset(titleBarHeight);

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

  const auto callbackFunc =
      std::bind(&MainWindow::eventCallback, this, std::placeholders::_1);

  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerInitRepoResult, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetCredentials, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCloneRepoError, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCloneRepoResult, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCloneRepoProgress, callbackFunc));

  LOG_PERF(0, "Exit");
}

MainWindow::~MainWindow() {
  LOG_PERF(0, "Enter");

  const auto callbackFunc =
      std::bind(&MainWindow::eventCallback, this, std::placeholders::_1);

  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCloneRepoProgress, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCloneRepoError, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCloneRepoResult, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerInitRepoResult, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetCredentials, callbackFunc));

  _toastHost.setParent(nullptr);

  for (const auto &container : _containers) {
    delete container;
  }

  LOG_PERF(0, "Exit");
}

void MainWindow::resizeEvent(QResizeEvent *event) {
  LOG_PERF(0, "Enter");

  if (!event) {
    LOG_PERF(0, "Exit");
    return;
  }

  QWidget::resizeEvent(event);

  QRect newGeometry(0, 0, event->size().width(), event->size().height());
  _toastHost.setParentSize(newGeometry);

  LOG_PERF(0, "Exit");
}

void MainWindow::onHome() {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

void MainWindow::onOpen() {
  LOG_PERF(0, "Enter");

  const QStringList documentsPaths =
      QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation);
  if (documentsPaths.length() == 0) {
    LOG_PERF(0, "Exit");
    return;
  }

  const QString path = QFileDialog::getExistingDirectory(
      this, tr("Open Git repository"), documentsPaths[0]);
  if (!path.isEmpty())
    addGitRepoContainer(path);

  LOG_PERF(0, "Exit");
}

void MainWindow::onClone() {
  LOG_PERF(0, "Enter");

  addCloneContainer();

  LOG_PERF(0, "Exit");
}

void MainWindow::onCloneFinished(ContainerWidget *sender) {
  LOG_PERF(0, "Enter");

  closeContainer(sender);

  LOG_PERF(0, "Exit");
}

void MainWindow::onInit() {
  LOG_PERF(0, "Enter");

  addInitGitRepoContainer();

  LOG_PERF(0, "Exit");
}

void MainWindow::onInitFinished(ContainerWidget *sender) {
  LOG_PERF(0, "Enter");

  closeContainer(sender);

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
  LOG_DEBUG("index is {}.", index);

  if (index < 0 || index >= _containers.length()) {
    LOG_WARN("Invalid tab index {}.", index);
    LOG_PERF(0, "Exit");
    return;
  }

  closeContainer(_containers[index]);

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

void MainWindow::onToastHostSizeChanged(const QSize &newSize) {
  LOG_PERF(0, "Enter");

  QSize adjustedSize;
  adjustedSize.setWidth(adjustedSize.width());
  adjustedSize.setHeight(newSize.height() + _titleBarWidget->height());

  setMinimumSize(newSize);

  LOG_PERF(0, "Exit");
}

void MainWindow::addCloneContainer() {
  LOG_PERF(0, "Enter");

  const qlonglong titleBarHeight =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarHeight);
  CloneGitRepoContainer *container = new CloneGitRepoContainer(this);
  container->setContentsMargins(0, titleBarHeight - TOP_MARGIN_OFFSET, 0, 0);
  _containers += container;
  _titleBarWidget->addTab(tr("Clone"));
  _titleBarWidget->selectTab(_titleBarWidget->tabsCount() - 1);

  const auto handlerFunc =
      std::bind(&MainWindow::onCloneFinished, this, container);

  connect(container, &CloneGitRepoContainer::finished, this, handlerFunc);

  LOG_PERF(0, "Exit");
}

void MainWindow::addGitRepoContainer(const QString &path) {
  LOG_PERF(0, "Enter");

  const qlonglong titleBarHeight =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarHeight);
  ContainerWidget *container = new GitRepoContainer(path, this);
  container->setContentsMargins(0, titleBarHeight - TOP_MARGIN_OFFSET, 0, 0);
  _containers += container;
  _titleBarWidget->addTab(Git::parseRepoName(path));
  _titleBarWidget->selectTab(_titleBarWidget->tabsCount() - 1);

  LOG_PERF(0, "Exit");
}

void MainWindow::addHomeContainer() {
  LOG_PERF(0, "Enter");

  const qlonglong titleBarHeight =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarHeight);
  HomeContainer *container = new HomeContainer(this);
  container->setContentsMargins(0, titleBarHeight - TOP_MARGIN_OFFSET, 0, 0);
  _containers += container;
  _titleBarWidget->addTab(tr("Home"));
  _titleBarWidget->selectTab(_titleBarWidget->tabsCount() - 1);

  connect(container, &HomeContainer::clone, this, &MainWindow::onClone);
  connect(container, &HomeContainer::init, this, &MainWindow::onInit);
  connect(container, &HomeContainer::open, this, &MainWindow::onOpen);

  LOG_PERF(0, "Exit");
}

void MainWindow::addInitGitRepoContainer() {
  LOG_PERF(0, "Enter");

  const qlonglong titleBarHeight =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarHeight);
  InitGitRepoContainer *container = new InitGitRepoContainer(this);
  container->setContentsMargins(0, titleBarHeight - TOP_MARGIN_OFFSET, 0, 0);
  _containers += container;
  _titleBarWidget->addTab(tr("Init"));
  _titleBarWidget->selectTab(_titleBarWidget->tabsCount() - 1);

  const auto handlerFunc =
      std::bind(&MainWindow::onInitFinished, this, container);

  connect(container, &InitGitRepoContainer::initialized, this, handlerFunc);

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

void MainWindow::closeContainer(ContainerWidget *container) {
  LOG_PERF(0, "Enter");

  int index = -1;

  for (int i = 0; i < _containers.length(); ++i) {
    if (_containers[i] != container)
      continue;

    index = i;
    break;
  }

  if (index == -1) {
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

void MainWindow::eventCallback(EventManager::Event event) {
  LOG_PERF(0, "Enter");

  const EventManager::EventType eventType = event.type();
  LOG_DEBUG("Processing event of type {}.", (int)eventType);

  switch (eventType) {
  case EventManager::EventType::GitRepoManagerGetCredentials: {
    const auto request = event.data<GitCredentialsRequest>();
    Dispatch::dispatchToMainThread([=]() { showCredentialsToast(request); });
    break;
  }
  case EventManager::EventType::GitRepoManagerCloneRepoError: {
    Dispatch::dispatchToMainThread(
        [=]() { showToast(tr("Unable to clone Git repository")); });
    break;
  }
  case EventManager::EventType::GitRepoManagerCloneRepoResult: {
    QString uuid;
    QString repoPath;
    std::tie(uuid, repoPath) = event.data<std::tuple<QString, QString>>();
    Dispatch::dispatchToMainThread([=]() { addGitRepoContainer(repoPath); });
    break;
  }
  case EventManager::EventType::GitRepoManagerCloneRepoProgress: {
    const CloneProgress data = event.data<CloneProgress>();
    if (!_progressing.contains(data.uuid)) {
      _progressing += data.uuid;
      Dispatch::dispatchToMainThread([=]() { showCloneProgressToast(data); });
    } else if (data.finished)
      _progressing.remove(data.uuid);

    break;
  }
  case EventManager::EventType::GitRepoManagerInitRepoResult: {
    const auto &[_, repoPath] = event.data<std::tuple<QString, QString>>();
    Dispatch::dispatchToMainThread([=]() { addGitRepoContainer(repoPath); });
    break;
  }
  default: {
    LOG_WARN("Unhandled event.");
    break;
  }
  }

  LOG_PERF(0, "Exit");
}

void MainWindow::selectContainer(int index) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("index is {}.", index);

  if (index < 0 || index >= _containers.length()) {
    LOG_WARN("Invalid tab index {}.", index);
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

void MainWindow::showCredentialsToast(const GitCredentialsRequest &request) {
  LOG_PERF(0, "Enter");

  _toastHost.addToast(new GitCredentialsToast(request));

  LOG_PERF(0, "Exit");
}

void MainWindow::showCloneProgressToast(const CloneProgress &data) {
  LOG_PERF(0, "Enter");

  _toastHost.addToast(new GitProgressToast(data));

  LOG_PERF(0, "Exit");
}

void MainWindow::showToast(const QString &text) {
  LOG_PERF(0, "Enter");

  _toastHost.addToast(new InfoToast(text));

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager
