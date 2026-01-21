#include "widgets/TitleBarMenuButton.h"

#include <Log.h>

#include <QApplication>
#include <QKeySequence>

#define THREAD_ID "TitleBarMenuButton"

namespace GuiManager::Widgets {

TitleBarMenuButton::TitleBarMenuButton(QWidget *parent)
    : QToolButton{parent},
      _configManager(ConfigManager::ConfigManager::getInstance()),
      _openAction(nullptr), _cloneAction(nullptr), _initAction(nullptr),
      _saveWorkspaceAction(nullptr), _settingsAction(nullptr),
      _credentialsAction(nullptr), _terminalAction(nullptr),
      _aboutAction(nullptr) {
  LOG_PERF(0, "Enter");

  setMouseTracking(true);
  setAttribute(Qt::WA_Hover, true);
  setPopupMode(QToolButton::MenuButtonPopup);
  installEventFilter(this);

  createActions();

  connect(this, &TitleBarMenuButton::clicked, this,
          &TitleBarMenuButton::onClicked);

  LOG_PERF(0, "Exit");
}

bool TitleBarMenuButton::eventFilter(QObject *watched, QEvent *event) {
  if (!event || this != watched)
    return false;

  switch (event->type()) {
  case QEvent::HoverEnter: {
    return hover(static_cast<QHoverEvent *>(event), true);
  }
  case QEvent::HoverLeave: {
    return hover(static_cast<QHoverEvent *>(event), false);
  }
  default: {
    break;
  }
  }

  return QToolButton::eventFilter(watched, event);
}

void TitleBarMenuButton::onClicked() {
  LOG_PERF(0, "Enter");

  setMenu(createMenu());
  showMenu();

  LOG_PERF(0, "Exit");
}

void TitleBarMenuButton::onOpen() {
  LOG_PERF(0, "Enter");

  emit open();

  LOG_PERF(0, "Exit");
}

void TitleBarMenuButton::onClone() {
  LOG_PERF(0, "Enter");

  emit clone();

  LOG_PERF(0, "Exit");
}

void TitleBarMenuButton::onInit() {
  LOG_PERF(0, "Enter");

  emit init();

  LOG_PERF(0, "Exit");
}

void TitleBarMenuButton::onSaveWorkspace() {
  LOG_PERF(0, "Enter");

  emit saveWorkspace();

  LOG_PERF(0, "Exit");
}

void TitleBarMenuButton::onSettings() {
  LOG_PERF(0, "Enter");

  emit settings();

  LOG_PERF(0, "Exit");
}

void TitleBarMenuButton::onCredentials() {
  LOG_PERF(0, "Enter");

  emit credentials();

  LOG_PERF(0, "Exit");
}

void TitleBarMenuButton::onTerminal() {
  LOG_PERF(0, "Enter");

  emit terminal();

  LOG_PERF(0, "Exit");
}

void TitleBarMenuButton::onAbout() {
  LOG_PERF(0, "Enter");

  emit about();

  LOG_PERF(0, "Exit");
}

void TitleBarMenuButton::createActions() {
  LOG_PERF(0, "Enter");

  _openAction = new QAction(this);
  _openAction->setText(tr("&Open"));
  _openAction->setShortcut(QKeySequence::Open);
  _openAction->setShortcutContext(Qt::ApplicationShortcut);
  connect(_openAction, &QAction::triggered, this, &TitleBarMenuButton::onOpen);

  _cloneAction = new QAction(this);
  _cloneAction->setText(tr("&Clone"));
  connect(_cloneAction, &QAction::triggered, this,
          &TitleBarMenuButton::onClone);

  _initAction = new QAction(this);
  _initAction->setText(tr("&Init"));
  _initAction->setShortcut(QKeySequence::New);
  _initAction->setShortcutContext(Qt::ApplicationShortcut);
  connect(_initAction, &QAction::triggered, this, &TitleBarMenuButton::onInit);

  _saveWorkspaceAction = new QAction(this);
  _saveWorkspaceAction->setText(tr("Save &workspace"));
  _saveWorkspaceAction->setShortcut(QKeySequence("Ctrl+w"));
  _saveWorkspaceAction->setShortcutContext(Qt::ApplicationShortcut);
  connect(_saveWorkspaceAction, &QAction::triggered, this,
          &TitleBarMenuButton::onSaveWorkspace);

  _settingsAction = new QAction(this);
  _settingsAction->setText(tr("Settings"));
  connect(_settingsAction, &QAction::triggered, this,
          &TitleBarMenuButton::onSettings);

  _terminalAction = new QAction(this);
  _terminalAction->setText(tr("Show &terminal"));
  _terminalAction->setShortcut(QKeySequence("Ctrl+t"));
  _terminalAction->setShortcutContext(Qt::ApplicationShortcut);
  connect(_terminalAction, &QAction::triggered, this,
          &TitleBarMenuButton::onTerminal);

  _credentialsAction = new QAction(this);
  _credentialsAction->setText(tr("Credentials manager"));
  connect(_credentialsAction, &QAction::triggered, this,
          &TitleBarMenuButton::onCredentials);

  _aboutAction = new QAction(this);
  _aboutAction->setText(tr("About"));
  connect(_aboutAction, &QAction::triggered, this,
          &TitleBarMenuButton::onAbout);

  // Add actions to the widget so their shortcuts become active
  addAction(_openAction);
  addAction(_initAction);
  addAction(_saveWorkspaceAction);
  addAction(_terminalAction);

  LOG_PERF(0, "Exit");
}

QMenu *TitleBarMenuButton::createMenu() {
  LOG_PERF(0, "Enter");

  const auto menu = new QMenu(this);

  const MainWindow *mainWindow_ = mainWindow();
  Q_ASSERT(mainWindow_ != nullptr);

  // Update save workspace action enabled state based on current containers
  _saveWorkspaceAction->setEnabled(mainWindow_ != nullptr &&
                                   !mainWindow_->containerPaths().isEmpty());

  menu->addAction(_openAction);
  menu->addAction(_cloneAction);
  menu->addAction(_initAction);
  menu->addSeparator();
  menu->addAction(_saveWorkspaceAction);
  menu->addSeparator();
  menu->addAction(_settingsAction);
  menu->addAction(_terminalAction);
  menu->addSeparator();
  menu->addAction(_credentialsAction);
  menu->addSeparator();
  menu->addAction(_aboutAction);

  const qlonglong menuMinimumWidth =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarMenuMinimumSize);

  LOG_DEBUG("Menu minimum width is {} pixels.", menuMinimumWidth);

  menu->setMinimumWidth(menuMinimumWidth);

  LOG_PERF(0, "Exit");

  return menu;
}

MainWindow *TitleBarMenuButton::mainWindow() {
  LOG_PERF(0, "Enter");

  foreach (QWidget *widget, QApplication::topLevelWidgets()) {
    if (auto *mainWin = qobject_cast<MainWindow *>(widget)) {
      LOG_PERF(0, "Exit");
      return mainWin;
    }
  }

  LOG_PERF(0, "Exit");
  return nullptr;
}

bool TitleBarMenuButton::hover(QHoverEvent *event, bool entered) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Entered is '{}'", entered);

  setDown(entered);

  LOG_PERF(0, "Exit");
  return true;
}

} // namespace GuiManager::Widgets
