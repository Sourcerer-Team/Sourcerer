#include "MainWindow.h"

#include "Constants.h"
#include "GitRepoManagerStructs.h"
#include "containers/BugReportContainer.h"
#include "containers/CloneGitRepoContainer.h"
#include "containers/GitCredentialsContainer.h"
#include "containers/GitKeysContainer.h"
#include "containers/GitRepoContainer.h"
#include "containers/HomeContainer.h"
#include "containers/InitGitRepoContainer.h"
#include "containers/SettingsContainer.h"
#include "dialogs/AboutDialog.h"
#include "widgets/ErrorToast.h"
#include "widgets/GitProgressToast.h"
#include "widgets/InfoToast.h"
#include "widgets/TextInputOverlayWidget.h"
#include "widgets/TitlebarWidget.h"

#include <ConfigKey.h>
#include <Dispatcher.h>
#include <Log.h>

#include <QCloseEvent>
#include <QEnterEvent>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSplitter>
#include <QStandardPaths>
#include <QWindow>

namespace {
class BottomResizeGripWidget final : public QWidget {
public:
  explicit BottomResizeGripWidget(QWidget *parent = nullptr)
      : QWidget(parent) {
    setMouseTracking(true);
    setCursor(Qt::SizeVerCursor);
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("background: transparent;");
  }

protected:
  void enterEvent(QEnterEvent *) override { setCursor(Qt::SizeVerCursor); }

  void leaveEvent(QEvent *) override { unsetCursor(); }

  void mousePressEvent(QMouseEvent *event) override {
    if (event->button() != Qt::LeftButton) {
      QWidget::mousePressEvent(event);
      return;
    }

    QWidget *parentWindow = window();
    if (parentWindow && parentWindow->windowHandle()) {
      Qt::Edges edges = Qt::BottomEdge;
      if (event->pos().x() <= 6) {
        edges |= Qt::LeftEdge;
      } else if (event->pos().x() >= width() - 6) {
        edges |= Qt::RightEdge;
      }
      parentWindow->windowHandle()->startSystemResize(edges);
    }
  }

  void mouseMoveEvent(QMouseEvent *event) override {
    Q_UNUSED(event);
    setCursor(Qt::SizeVerCursor);
  }
};
} // namespace

#include <QMessageBox>
#include <functional>

#define THREAD_ID "MainWindow"

namespace GuiManager {

using namespace Containers;
using namespace Dialogs;
using namespace Util;

constexpr int TOP_MARGIN_OFFSET = -55;
constexpr int PROFILE_WIDGET_ARROW_OFFSET = 32;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), _current(-1), _centralWidget(new CentralWidget),
      _topBarWidget(new TopBarWidget), _consoleWidget(new ConsoleWidget(this)),
      _toastHost(ToastHostWidget::getInstance()),
      _configManager(ConfigManager::ConfigManager::getInstance()),
      _eventManager(EventManager::EventManager::getInstance()),
      _maximized(false), _mousePressed(false), _hitDetector(nullptr),
      _bottomResizeGrip(nullptr),
      _profileBoardWidget(QSharedPointer<ProfileBoardWidget>::create(this)),
      _profileBoardActive(false) {
  LOG_PERF(0, "Enter");

  setWindowFlags(Qt::Window | Qt::FramelessWindowHint);

#if defined(Q_OS_WIN) or defined(Q_OS_LINUX)
  _hitDetector = new HitDetector(this);
  _hitDetector->init();
  setMouseTracking(true);
  installEventFilter(this);
  connect(_hitDetector, &HitDetector::edgeHit, this, &MainWindow::onEdgeHit);
#endif

  _profileBoardWidget->hide();

  const qlonglong minimumWidth =
      _configManager.getLong(ConfigManager::ConfigKey::MainWindowMinimumWidth);
  const qlonglong minimumHeight =
      _configManager.getLong(ConfigManager::ConfigKey::MainWindowMinimumHeight);

  setMinimumWidth(minimumWidth);
  setMinimumHeight(minimumHeight);

  const auto widget = new QWidget(this);
  widget->setMouseTracking(true);

  _centralWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  // Create upper widget with topBar and central widget
  auto *upperWidget = new QWidget();
  auto *upperLayout = new QVBoxLayout(upperWidget);
  upperLayout->setContentsMargins(0, 0, 0, 0);
  upperLayout->setSpacing(0);
  upperLayout->addWidget(_topBarWidget);
  upperLayout->addWidget(_centralWidget);

  // Create splitter and add upper widget and console
  auto *splitter = new QSplitter(Qt::Vertical, widget);
  splitter->addWidget(upperWidget);
  splitter->addWidget(_consoleWidget);
  splitter->setStretchFactor(0, 1); // Upper widget takes most space
  splitter->setStretchFactor(1, 0); // Console takes minimal space

  // Main layout
  const auto verticalLayout = new QVBoxLayout(widget);
  verticalLayout->setContentsMargins(0, 0, 0, 0);
  verticalLayout->setSpacing(0);
  verticalLayout->addWidget(splitter);

  _consoleWidget->hide();

  setCentralWidget(widget);

  _bottomResizeGrip = new BottomResizeGripWidget(this);
  _bottomResizeGrip->raise();

  const qlonglong titleBarHeight =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarHeight);

  LOG_DEBUG("TitleBar height is {} pixels.", titleBarHeight);

  _toastHost.setParent(this);
  _toastHost.setYOffset(titleBarHeight);

  connect(_topBarWidget, &TopBarWidget::open, this, &MainWindow::onOpen);
  connect(_topBarWidget, &TopBarWidget::clone, this, &MainWindow::onClone);
  connect(_topBarWidget, &TopBarWidget::init, this, &MainWindow::onInit);
  connect(_topBarWidget, &TopBarWidget::saveWorkspace, this,
          &MainWindow::onSaveWorkspace);
  connect(_topBarWidget, &TopBarWidget::settings, this,
          &MainWindow::onSettings);
  connect(_topBarWidget, &TopBarWidget::credentials, this,
          &MainWindow::onCredentials);
  connect(_topBarWidget, &TopBarWidget::sshKeys, this, &MainWindow::onSshKeys);
  connect(_topBarWidget, &TopBarWidget::terminal, this,
          &MainWindow::onTerminal);
  connect(_topBarWidget, &TopBarWidget::bugReport, this,
          &MainWindow::onBugReport);
  connect(_topBarWidget, &TopBarWidget::about, this, &MainWindow::onAbout);
  connect(_topBarWidget, &TopBarWidget::closedTab, this,
          &MainWindow::onClosedTab);
  connect(_topBarWidget, &TopBarWidget::selectedTab, this,
          &MainWindow::onSelectedTab);
  connect(_topBarWidget, &TopBarWidget::resizeTabBar, this,
          &MainWindow::onTabBarResize);
  connect(_topBarWidget, &TopBarWidget::minimized, this,
          &MainWindow::onMinimizeClicked);
  connect(_topBarWidget, &TopBarWidget::maximized, this,
          &MainWindow::onMaximizeClicked);
  connect(_topBarWidget, &TopBarWidget::closed, this,
          &MainWindow::onCloseClicked);
  connect(_topBarWidget, &TopBarWidget::doubleClicked, this,
          &MainWindow::onMaximizeClicked);
  connect(_topBarWidget, &TopBarWidget::dragged, this,
          &MainWindow::onTitleBarDrag);
  connect(_topBarWidget, &TopBarWidget::profile, this, &MainWindow::onProfile);

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

  _profileBoardWidget->setParent(nullptr);
  _profileBoardWidget.clear();

  _centralWidget->setCentralWidget(nullptr);

  if (_current >= 0 && _current < _containers.size()) {
    if (_containers[_current]) {
      _containers[_current]->setParent(nullptr);
    }
  }

  delete _bottomResizeGrip;
  delete _hitDetector;

  LOG_PERF(0, "Exit");
}

QStringList MainWindow::containerPaths() const {
  LOG_PERF(0, "Enter");

  QStringList paths;

  for (int i = 0; i < _containers.length(); ++i) {
    const auto container = _containers[i];
    Q_ASSERT(container != nullptr);
    if (!container) {
      continue;
    }

    const ContainerType type = container->type();
    if (type == ContainerType::Home) {
      continue;
    }

    paths += container->path();
  }

  LOG_PERF(0, "Exit");

  return paths;
}

void MainWindow::mouseMoveEvent(QMouseEvent *event) {
  if (!event)
    return;

  event->ignore();
}

void MainWindow::resizeEvent(QResizeEvent *event) {
  QMainWindow::resizeEvent(event);

  if (_bottomResizeGrip) {
    const qlonglong borderWidth =
        _configManager.getLong(ConfigManager::ConfigKey::MainWindowBorderWidth);
    const int gripHeight = qMax<qlonglong>(borderWidth, 6);
    _bottomResizeGrip->setGeometry(0, height() - gripHeight, width(), gripHeight);
    _bottomResizeGrip->raise();
  }
}

void MainWindow::changeEvent(QEvent *event) {
  LOG_PERF(0, "Enter");

  if (event->type() == QEvent::WindowStateChange) {
    if (windowState() & Qt::WindowMinimized) {
      // Window is minimized, hide all toasts
      _toastHost.hideAllToasts();
    } else {
      // Window is restored, show all toasts
      _toastHost.showAllToasts();
    }
  }

  QMainWindow::changeEvent(event);

  LOG_PERF(0, "Exit");
}

void MainWindow::closeEvent(QCloseEvent *event) {
  LOG_PERF(0, "Enter");

  _configManager.setString(ConfigManager::ConfigKey::MainWindowGeometry,
                           saveGeometry().toHex());

  QMainWindow::closeEvent(event);

  LOG_PERF(0, "Exit");
}

void MainWindow::showEvent(QShowEvent *event)
{
  LOG_PERF(0, "Enter");

  QMainWindow::showEvent(event);

  const QString savedGeometry =
      _configManager.getString(ConfigManager::ConfigKey::MainWindowGeometry);
  if (!savedGeometry.isEmpty()) {
    restoreGeometry(QByteArray::fromHex(savedGeometry.toUtf8()));
    _maximized = windowState().testFlag(Qt::WindowMaximized);
  }

  LOG_PERF(0, "Exit");
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
  return _hitDetector->eventFilter(watched, event);
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

void MainWindow::onOpenRepo(const QString &path) {
  LOG_PERF(0, "Enter");

  addGitRepoContainer(path);

  LOG_PERF(0, "Exit");
}

void MainWindow::onRecentClicked(const QString &path) {
  LOG_PERF(0, "Enter");

  if (!path.isEmpty())
    addGitRepoContainer(path);

  LOG_PERF(0, "Exit");
}

void MainWindow::onSaveWorkspace() {
  LOG_PERF(0, "Enter");

  saveNewWorkspace();

  LOG_PERF(0, "Exit");
}

void MainWindow::onSettings() {
  LOG_PERF(0, "Enter");

  addSettingsContainer();

  LOG_PERF(0, "Exit");
}

void MainWindow::onCredentials() {
  LOG_PERF(0, "Enter");

  addGitCredentialsContainer();

  LOG_PERF(0, "Exit");
}

void MainWindow::onSshKeys() {
  LOG_PERF(0, "Enter");

  addGitKeysContainer();

  LOG_PERF(0, "Exit");
}

void MainWindow::onSubmitWorkspace(const QString &name) {
  LOG_PERF(0, "Enter");

  if (name.isEmpty()) {
    LOG_PERF(0, "Exit");
    return;
  }

  const QString writableDir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  const QString workspacesDirectory =
      writableDir + "/" + WORKSPACES_SUBDIRECTORY;
  const QString workspaceFilePath =
      workspacesDirectory + "/" + name + ".workspace";

  const QDir dir;
  dir.mkpath(workspacesDirectory);

  QFile file(workspaceFilePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text |
                 QIODevice::Truncate)) {
    LOG_ERROR(Log::ErrorId::FileSystem, "Can't open file \"{}\" for writing.",
              workspaceFilePath);
    return;
  }

  QVector<QPair<ContainerType, QString>> workspaces;

  for (int i = 0; i < _containers.length(); ++i) {
    if (const auto repoContainer =
            qobject_cast<Widgets::ContainerWidget *>(_containers[i].get())) {
      const ContainerType type = repoContainer->type();
      if (type == ContainerType::Home) {
        continue;
      }

      const QString path = repoContainer->path();
      if (path.isEmpty())
        continue;

      workspaces += qMakePair(type, path);
    }
  }

  QJsonArray array;

  for (const auto &[type, path] : workspaces) {
    array += QJsonObject{
        {"type", static_cast<int>(type)},
        {"path", QDir::fromNativeSeparators(path)},
    };
  }

  const QJsonDocument document(array);
  const QString jsonStr = document.toJson(QJsonDocument::Compact);

  QTextStream stream(&file);
  stream << jsonStr;
  stream.flush();

  file.close();

  LOG_PERF(0, "Exit");
}

void MainWindow::onTerminal() {
  LOG_PERF(0, "Enter");

  // Get the terminal action to update its text
  QAction *terminalAction =
      _topBarWidget->titleBarWidget()->menuButton()->terminalAction();

  if (_consoleWidget->isVisible()) {
    // Hide console
    _consoleWidget->hide();
    if (terminalAction) {
      terminalAction->setText(tr("Show &terminal"));
    }
  } else {
    // Show console
    QString workingDirectory =
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation);

    // Prefer the current repository directory when one is open.
    if (_current >= 0 && _current < _containers.size()) {
      const auto container = _containers[_current];
      if (container) {
        const QString path = container->path();
        if (!path.isEmpty()) {
          workingDirectory = path;
        }
      }
    }

    _consoleWidget->setWorkingDirectory(workingDirectory);

    _consoleWidget->init();
    _consoleWidget->show();
    _consoleWidget->setFocusToConsole();

    if (terminalAction) {
      terminalAction->setText(tr("Hide &terminal"));
    }
  }

  LOG_PERF(0, "Exit");
}

void MainWindow::onWorkspaceClicked(const QString &name) {
  LOG_PERF(0, "Enter");

  bool foundNotHome = false;

  for (int i = 0; i < _containers.length(); ++i) {
    ContainerWidget *containerWidget = _containers[i].get();
    Q_ASSERT(containerWidget != nullptr);
    if (!containerWidget) {
      continue;
    }
    if (containerWidget->type() == ContainerType::Home) {
      continue;
    }

    foundNotHome = true;
    break;
  }

  if (foundNotHome) {
    const QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Confirm Action", "Do you want to close existing repositories?",
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) {
      LOG_PERF(0, "Exit");
      return;
    }
  }

  const QString writableDir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  const QString workspacesDirectory =
      writableDir + "/" + WORKSPACES_SUBDIRECTORY;
  const QString workspaceFilePath =
      workspacesDirectory + "/" + name + ".workspace";

  QFile file(workspaceFilePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    LOG_ERROR(Log::ErrorId::FileSystem, "Can't open file \"{}\" for writing.",
              workspaceFilePath);
    return;
  }

  const QByteArray data = file.readAll();

  file.close();

  const QJsonDocument document = QJsonDocument::fromJson(data);
  if (document.isNull()) {
    LOG_PERF(0, "Exit");
    return;
  }

  if (!document.isArray()) {
    LOG_PERF(0, "Exit");
    return;
  }

  const auto containersListCopy = _containers;

  for (int i = 0; i < containersListCopy.length(); ++i) {
    ContainerWidget *containerWidget = containersListCopy[i].get();
    Q_ASSERT(containerWidget != nullptr);
    if (!containerWidget) {
      continue;
    }
    if (containerWidget->type() == ContainerType::Home) {
      continue;
    }

    closeContainer(containerWidget);
  }

  QJsonArray array = document.array();

  for (const auto &val : array) {
    Q_ASSERT(val.isObject());
    if (!val.isObject()) {
      continue;
    }

    const QJsonObject object = val.toObject();

    const int type = object["type"].toInt();

    if (type <= static_cast<int>(ContainerType::Unknown) ||
        type >= static_cast<int>(ContainerType::last)) {
      continue;
    }

    const QString path = object["path"].toString();
    if (path.isEmpty()) {
      continue;
    }

    switch (static_cast<ContainerType>(type)) {
    case ContainerType::GitRepo: {
      addGitRepoContainer(path);
      break;
    }
    default: {
      LOG_WARN(("Internal error! Unknown container type {}."), type);
      break;
    }
    }
  }

  LOG_PERF(0, "Exit");
}

void MainWindow::onAbout() const {
  LOG_PERF(0, "Enter");

  AboutDialog aboutDlg;
  aboutDlg.move(x() + (width() - aboutDlg.width()) / 2,
                y() + (height() - aboutDlg.height()) / 2);
  aboutDlg.exec();

  LOG_PERF(0, "Exit");
}

void MainWindow::onBugReport() {
  LOG_PERF(0, "Enter");

  addBugReportContainer();

  LOG_PERF(0, "Exit");
}

void MainWindow::onClosedTab(const int index) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("index is {}.", index);

  if (index < 0 || index >= _containers.length()) {
    LOG_WARN("Invalid tab index {}.", index);
    LOG_PERF(0, "Exit");
    return;
  }

  closeContainer(_containers[index].get());

  LOG_PERF(0, "Exit");
}

void MainWindow::onSelectedTab(const int index) {
  LOG_PERF(0, "Enter");

  selectContainer(index);

  LOG_PERF(0, "Exit");
}

void MainWindow::onTabBarResize() {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

void MainWindow::onToastHostSizeChanged(const QSize &newSize) {
  LOG_PERF(0, "Enter");

  QSize adjustedSize;
  adjustedSize.setWidth(adjustedSize.width());
  adjustedSize.setHeight(newSize.height() +
                         _topBarWidget->titleBarWidget()->height());

  setMinimumSize(newSize);

  LOG_PERF(0, "Exit");
}

void MainWindow::onMinimizeClicked() {
  LOG_PERF(0, "Enter");

  showMinimized();

  LOG_PERF(0, "Exit");
}

void MainWindow::onMaximizeClicked() {
  LOG_PERF(0, "Enter");

  if (_maximized) {
    _maximized = false;
    showNormal();
  } else {
    _maximized = true;
    showMaximized();
  }

  LOG_PERF(0, "Exit");
}

void MainWindow::onCloseClicked() {
  LOG_PERF(0, "Enter");

  close();

  LOG_PERF(0, "Exit");
}

void MainWindow::onTitleBarDrag() const {
  LOG_PERF(0, "Enter");

  windowHandle()->startSystemMove();

  LOG_PERF(0, "Exit");
}

void MainWindow::onEdgeHit(Qt::Edges edges) const {
  LOG_PERF(0, "Enter");

  windowHandle()->startSystemResize(edges);

  LOG_PERF(0, "Exit");
}

void MainWindow::onProfile(int middleX) {
  LOG_PERF(0, "Enter");

  Q_UNUSED(middleX);

  _profileBoardWidget->adjustSize();

  const QPushButton *profileButton = _topBarWidget->profileButton();
  const QRect profileButtonRect =
      profileButton ? profileButton->geometry() : QRect();
  const QPoint popupTopLeft =
      profileButton
          ? _topBarWidget->mapToGlobal(QPoint(profileButtonRect.center().x() -
                                                  _profileBoardWidget->width() +
                                                  PROFILE_WIDGET_ARROW_OFFSET,
                                              _topBarWidget->height()))
          : mapToGlobal(QPoint(width() / 2 - _profileBoardWidget->width() / 2,
                               _topBarWidget->height()));
  _profileBoardWidget->move(popupTopLeft);
  _profileBoardWidget->setArrowTargetX(_profileBoardWidget->width() -
                                       PROFILE_WIDGET_ARROW_OFFSET);
  _profileBoardWidget->show();
  _profileBoardWidget->raise();
  _profileBoardActive = true;

  LOG_PERF(0, "Exit");
}

void MainWindow::addCloneContainer() {
  LOG_PERF(0, "Enter");

  const qlonglong titleBarHeight =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarHeight);
  const auto container = QSharedPointer<CloneGitRepoContainer>::create(this);
  container->setContentsMargins(0, titleBarHeight - (-TOP_MARGIN_OFFSET), 0, 0);
  container->init();
  container->selected();
  container->setupShortcuts();
  _containers += container;
  _topBarWidget->titleBarWidget()->addTab(tr("Clone"));
  _topBarWidget->titleBarWidget()->selectTab(
      _topBarWidget->titleBarWidget()->tabsCount() - 1);

  const auto handlerFunc =
      std::bind(&MainWindow::onCloneFinished, this, container.get());

  connect(container.get(), &CloneGitRepoContainer::finished, this, handlerFunc);

  LOG_PERF(0, "Exit");
}

void MainWindow::addGitRepoContainer(const QString &path) {
  LOG_PERF(0, "Enter");

  const qlonglong titleBarHeight =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarHeight);
  const auto container = QSharedPointer<GitRepoContainer>::create(path, this);
  container->setContentsMargins(0, titleBarHeight - (-TOP_MARGIN_OFFSET), 0, 0);
  container->init();
  container->selected();
  container->setupShortcuts();

  connect(container.get(), &GitRepoContainer::openRepoRequested, this,
          &MainWindow::onOpenRepo);

  _containers += container;

  const QStringList parts =
      QDir::fromNativeSeparators(path).split('/', Qt::SkipEmptyParts);

  QString repoName;
  if (!parts.empty()) {
    repoName = parts.last();
  } else {
    repoName = path;
  }

  _topBarWidget->titleBarWidget()->addTab(repoName);
  _topBarWidget->titleBarWidget()->selectTab(
      _topBarWidget->titleBarWidget()->tabsCount() - 1);

  LOG_PERF(0, "Exit");
}

void MainWindow::addHomeContainer() {
  LOG_PERF(0, "Enter");

  const qlonglong titleBarHeight =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarHeight);
  const auto container = QSharedPointer<HomeContainer>::create(this);
  container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  container->setContentsMargins(0, titleBarHeight - (-TOP_MARGIN_OFFSET), 0, 0);
  container->init();
  container->selected();
  container->setupShortcuts();
  _containers += container;
  _topBarWidget->titleBarWidget()->addTab(tr("Home"));
  _topBarWidget->titleBarWidget()->selectTab(
      _topBarWidget->titleBarWidget()->tabsCount() - 1);

  connect(container.get(), &HomeContainer::clone, this, &MainWindow::onClone);
  connect(container.get(), &HomeContainer::init, this, &MainWindow::onInit);
  connect(container.get(), &HomeContainer::open, this, &MainWindow::onOpen);
  connect(container.get(), &HomeContainer::recentClicked, this,
          &MainWindow::onRecentClicked);
  connect(container.get(), &HomeContainer::workspaceClicked, this,
          &MainWindow::onWorkspaceClicked);

  LOG_PERF(0, "Exit");
}

void MainWindow::addInitGitRepoContainer() {
  LOG_PERF(0, "Enter");

  const qlonglong titleBarHeight =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarHeight);
  const auto container = QSharedPointer<InitGitRepoContainer>::create(this);
  container->setContentsMargins(0, titleBarHeight - (-TOP_MARGIN_OFFSET), 0, 0);
  container->init();
  container->selected();
  container->setupShortcuts();
  _containers += container;
  _topBarWidget->titleBarWidget()->addTab(tr("Init"));
  _topBarWidget->titleBarWidget()->selectTab(
      _topBarWidget->titleBarWidget()->tabsCount() - 1);

  const auto handlerFunc =
      std::bind(&MainWindow::onInitFinished, this, container.get());

  connect(container.get(), &InitGitRepoContainer::initialized, this,
          handlerFunc);

  LOG_PERF(0, "Exit");
}

void MainWindow::addSettingsContainer() {
  LOG_PERF(0, "Enter");

  const qlonglong titleBarHeight =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarHeight);
  const auto container = QSharedPointer<SettingsContainer>::create(this);
  container->setContentsMargins(0, titleBarHeight - (-TOP_MARGIN_OFFSET), 0, 0);
  container->init();
  container->selected();
  container->setupShortcuts();
  _containers += container;
  _topBarWidget->titleBarWidget()->addTab(tr("Settings"));
  _topBarWidget->titleBarWidget()->selectTab(
      _topBarWidget->titleBarWidget()->tabsCount() - 1);

  LOG_PERF(0, "Exit");
}

void MainWindow::addBugReportContainer() {
  LOG_PERF(0, "Enter");

  const qlonglong titleBarHeight =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarHeight);
  const auto container = QSharedPointer<BugReportContainer>::create(this);
  container->setContentsMargins(0, titleBarHeight - (-TOP_MARGIN_OFFSET), 0, 0);
  container->init();
  container->selected();
  container->setupShortcuts();
  _containers += container;
  _topBarWidget->titleBarWidget()->addTab(tr("Report bug"));
  _topBarWidget->titleBarWidget()->selectTab(
      _topBarWidget->titleBarWidget()->tabsCount() - 1);

  connect(container.get(), &BugReportContainer::close, this,
          [this, container]() { closeContainer(container.get()); });

  LOG_PERF(0, "Exit");
}

void MainWindow::addGitCredentialsContainer() {
  LOG_PERF(0, "Enter");

  const qlonglong titleBarHeight =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarHeight);
  const auto container = QSharedPointer<GitCredentialsContainer>::create(this);
  container->setContentsMargins(0, titleBarHeight - (-TOP_MARGIN_OFFSET), 0, 0);
  container->init();
  container->selected();
  container->setupShortcuts();
  _containers += container;
  _topBarWidget->titleBarWidget()->addTab(tr("Git credentials"));
  _topBarWidget->titleBarWidget()->selectTab(
      _topBarWidget->titleBarWidget()->tabsCount() - 1);

  LOG_PERF(0, "Exit");
}

void MainWindow::addGitKeysContainer() {
  LOG_PERF(0, "Enter");

  const qlonglong titleBarHeight =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarHeight);
  const auto container = QSharedPointer<GitKeysContainer>::create(this);
  container->setContentsMargins(0, titleBarHeight - (-TOP_MARGIN_OFFSET), 0, 0);
  container->init();
  container->selected();
  container->setupShortcuts();
  _containers += container;
  _topBarWidget->titleBarWidget()->addTab(tr("SSH keys"));
  _topBarWidget->titleBarWidget()->selectTab(
      _topBarWidget->titleBarWidget()->tabsCount() - 1);

  LOG_PERF(0, "Exit");
}

void MainWindow::closeContainer(const ContainerWidget *container) {
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

  // Hide and deselect the container being closed
  if (_current == index) {
    _containers[index]->hide();
    _containers[index]->deselected();
    _current = -1;
  } else if (_current > index && _current > -1) {
    _current--;
  }

  _toastHost.setParent(this);

  _containers.remove(index);

  if (_containers.length() == 0)
    addHomeContainer();

  _topBarWidget->titleBarWidget()->removeTab(index);

  LOG_PERF(0, "Exit");
}

void MainWindow::eventCallback(EventManager::Event event) {
  LOG_PERF(0, "Enter");

  const EventManager::EventType eventType = event.type();
  LOG_DEBUG("Processing event of type {}.", (int)eventType);

  switch (eventType) {
  case EventManager::EventType::GitRepoManagerGetCredentials: {
    const auto request = event.data<GitCredentialsRequest>();
    Util::Dispatcher::dispatchToMainThread(getSelf().toWeakRef(), [=]() {
      Q_ASSERT(_current != -1);
      QWidget *parent =
          _current != -1 ? qobject_cast<QWidget *>(_containers[_current].get())
                         : qobject_cast<QWidget *>(this);
      Q_ASSERT(parent != nullptr);
      if (!parent) {
        LOG_WARN("Parent widget is null.");
        return;
      }
      if (request.needsPassphrase) {
        SshPassphraseOverlayWidget *overlayWidget = nullptr;
        if (_passphraseWidgets.contains(request.id)) {
          overlayWidget = _passphraseWidgets[request.id];
        } else {
          overlayWidget = new SshPassphraseOverlayWidget(request.id, parent);
          _passphraseWidgets[request.id] = overlayWidget;
        }
        connect(overlayWidget, &SshPassphraseOverlayWidget::error, this, [=]() {
          hidePassphraseOverlay(overlayWidget);
          _passphraseWidgets.remove(request.id);
          delete overlayWidget;
        });
        connect(overlayWidget, &SshPassphraseOverlayWidget::finished, this,
                [=]() {
                  hidePassphraseOverlay(overlayWidget);
                  _passphraseWidgets.remove(request.id);
                  delete overlayWidget;
                });
        showPassphraseOverlay(request, overlayWidget);
      } else {
        GitCredentialsOverlayWidget *overlayWidget = nullptr;
        if (_credentialsWidgets.contains(request.id)) {
          overlayWidget = _credentialsWidgets[request.id];
        } else {
          overlayWidget = new GitCredentialsOverlayWidget(request.id, parent);
          _credentialsWidgets[request.id] = overlayWidget;
        }
        connect(overlayWidget, &GitCredentialsOverlayWidget::error, this,
                [=]() {
                  hideCredentialsOverlay(overlayWidget);
                  _credentialsWidgets.remove(request.id);
                  delete overlayWidget;
                });
        connect(overlayWidget, &GitCredentialsOverlayWidget::finished, this,
                [=]() {
                  hideCredentialsOverlay(overlayWidget);
                  _credentialsWidgets.remove(request.id);
                  delete overlayWidget;
                });
        showCredentialsOverlay(request, overlayWidget);
      }
    });
    break;
  }
  case EventManager::EventType::GitRepoManagerCloneRepoError: {
    Util::Dispatcher::dispatchToMainThread(getSelf().toWeakRef(), [=]() {
      showErrorToast(tr("Unable to clone Git repository"), tr("Error"));
    });
    break;
  }
  case EventManager::EventType::GitRepoManagerCloneRepoResult: {
    const auto commandResult =
        event.data<GitRepoManager::Structs::GitRepoResult>();
    const QString repoPath = commandResult.value.toString();
    Util::Dispatcher::dispatchToMainThread(
        getSelf().toWeakRef(), [=]() { addGitRepoContainer(repoPath); });
    break;
  }
  case EventManager::EventType::GitRepoManagerCloneRepoProgress: {
    const auto commandResult =
        event.data<GitRepoManager::Structs::GitRepoResult>();
    if (const CloneProgress data = commandResult.value.value<CloneProgress>();
        !data.error && !data.finished && !_progressing.contains(data.uuid)) {
      _progressing += data.uuid;
      Util::Dispatcher::dispatchToMainThread(
          getSelf().toWeakRef(), [=]() { showCloneProgressToast(data); });
    } else if (data.finished)
      _progressing.remove(data.uuid);

    break;
  }
  case EventManager::EventType::GitRepoManagerInitRepoResult: {
    const auto commandResult =
        event.data<GitRepoManager::Structs::GitRepoResult>();
    const auto repoPath = commandResult.value.toString();
    Util::Dispatcher::dispatchToMainThread(
        getSelf().toWeakRef(), [=]() { addGitRepoContainer(repoPath); });
    break;
  }
  default: {
    LOG_WARN("Unhandled event.");
    break;
  }
  }

  LOG_PERF(0, "Exit");
}

void MainWindow::saveNewWorkspace() {
  LOG_PERF(0, "Enter");

  const auto inputOverlay = new TextInputOverlayWidget(this);
  inputOverlay->setPlaceholderText(tr("Name of the workspace"));

  connect(inputOverlay, &TextInputOverlayWidget::canceled, this, [=]() {
    inputOverlay->hide();
    delete inputOverlay;
  });
  connect(inputOverlay, &TextInputOverlayWidget::submitted, this,
          [=](const QString &name) {
            inputOverlay->hide();
            delete inputOverlay;
            onSubmitWorkspace(name);
          });

  inputOverlay->show();
  inputOverlay->raise();

  LOG_PERF(0, "Exit");
}

void MainWindow::showCredentialsOverlay(const GitCredentialsRequest &request,
                                        GitCredentialsOverlayWidget *widget) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(widget != nullptr);

  if (!widget) {
    LOG_PERF(0, "Exit")
    return;
  }

  widget->setRepoPath(request.repoPath);
  widget->setRequest(request);
  widget->start();
  widget->show();
  widget->raise();

  LOG_PERF(0, "Exit");
}

void MainWindow::hidePassphraseOverlay(SshPassphraseOverlayWidget *widget) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(widget != nullptr);

  if (!widget) {
    LOG_PERF(0, "Exit");
    return;
  }

  widget->stop();
  widget->hide();

  LOG_PERF(0, "Exit");
}

void MainWindow::showPassphraseOverlay(const GitCredentialsRequest &request,
                                       SshPassphraseOverlayWidget *widget) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(widget != nullptr);

  if (!widget) {
    LOG_PERF(0, "Exit")
    return;
  }

  widget->setRequest(request);
  widget->start();
  widget->show();
  widget->raise();

  LOG_PERF(0, "Exit");
}

void MainWindow::selectContainer(const int index) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("index is {}.", index);

  if (index < 0 || index >= _containers.length()) {
    LOG_WARN("Invalid tab index {}.", index);
    LOG_PERF(0, "Exit");
    return;
  }

  if (_current != -1) {
    _containers[_current]->hide();
    _containers[_current]->deselected();
  }

  const qlonglong titleBarHeight =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarHeight);

  _toastHost.setParent(_containers[index].get());
  _toastHost.setYOffset(titleBarHeight);

  _containers[index]->show();
  _containers[index]->setupShortcuts();
  _containers[index]->selected();
  _centralWidget->setCentralWidget(_containers[index].get());
  _current = index;

  LOG_PERF(0, "Exit");
}

void MainWindow::hideCredentialsOverlay(GitCredentialsOverlayWidget *widget) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(widget != nullptr);

  if (!widget) {
    LOG_PERF(0, "Exit");
    return;
  }

  widget->stop();
  widget->hide();

  LOG_PERF(0, "Exit");
}

void MainWindow::showCloneProgressToast(const CloneProgress &data) {
  LOG_PERF(0, "Enter");

  const auto toast = QSharedPointer<GitProgressToast>::create(
      data, data.cloning ? tr("Clone") : tr("Fetch"));

  Dispatcher::dispatchToMainThread(getSelf().toWeakRef(),
                                   [=]() { _toastHost.addToast(toast); });

  LOG_PERF(0, "Exit");
}

void MainWindow::showToast(const QString &text, const QString &title) {
  LOG_PERF(0, "Enter");

  Dispatcher::dispatchToMainThread(getSelf().toWeakRef(), [=]() {
    _toastHost.addToast(new InfoToast(text, title));
  });

  LOG_PERF(0, "Exit");
}

void MainWindow::showErrorToast(const QString &text, const QString &title) {
  LOG_PERF(0, "Enter");

  Dispatcher::dispatchToMainThread(getSelf().toWeakRef(), [=]() {
    _toastHost.addToast(new ErrorToast(text, title));
  });

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager
