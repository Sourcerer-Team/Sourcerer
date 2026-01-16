#include "widgets/TitleBarMenuButton.h"

#include <Log.h>

#include <QApplication>
#include <QKeySequence>

#define THREAD_ID "TitleBarMenuButton"

namespace GuiManager::Widgets {

TitleBarMenuButton::TitleBarMenuButton(QWidget *parent)
    : QToolButton{parent},
      _configManager(ConfigManager::ConfigManager::getInstance()) {
  LOG_PERF(0, "Enter");

  setMouseTracking(true);
  setAttribute(Qt::WA_Hover, true);
  setPopupMode(QToolButton::MenuButtonPopup);
  installEventFilter(this);

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

QMenu *TitleBarMenuButton::createMenu() {
  LOG_PERF(0, "Enter");

  const auto menu = new QMenu(this);

  const auto openAction = new QAction(menu);
  openAction->setText(tr("&Open"));
  openAction->setShortcut(QKeySequence::Open);

  const auto cloneAction = new QAction(menu);
  cloneAction->setText(tr("&Clone"));

  const auto initAction = new QAction(menu);
  initAction->setText(tr("&Init"));
  initAction->setShortcut(QKeySequence::New);

  const MainWindow *mainWindow_ = mainWindow();
  Q_ASSERT(mainWindow_ != nullptr);

  const auto saveWorkspaceAction = new QAction(menu);
  saveWorkspaceAction->setText(tr("Save &workspace"));
  saveWorkspaceAction->setEnabled(mainWindow_ != nullptr &&
                                  !mainWindow_->containerPaths().isEmpty());
  saveWorkspaceAction->setShortcut(QKeySequence("Ctrl+w"));

  const auto settingsAction = new QAction(menu);
  settingsAction->setText(tr("Settings"));

  const auto terminalAction = new QAction(menu);
  terminalAction->setText(tr("Show &terminal"));
  terminalAction->setShortcut(QKeySequence("Ctrl+t"));

  const auto aboutAction = new QAction(menu);
  aboutAction->setText(tr("About"));

  menu->addAction(openAction);
  menu->addAction(cloneAction);
  menu->addAction(initAction);
  menu->addSeparator();
  menu->addAction(saveWorkspaceAction);
  menu->addSeparator();
  menu->addAction(settingsAction);
  menu->addAction(terminalAction);
  menu->addAction(aboutAction);

  const qlonglong menuMinimumWidth =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarMenuMinimumSize);

  LOG_DEBUG("Menu minimum width is {} pixels.", menuMinimumWidth);

  menu->setMinimumWidth(menuMinimumWidth);

  connect(openAction, &QAction::triggered, this, &TitleBarMenuButton::onOpen);
  connect(cloneAction, &QAction::triggered, this, &TitleBarMenuButton::onClone);
  connect(initAction, &QAction::triggered, this, &TitleBarMenuButton::onInit);
  connect(saveWorkspaceAction, &QAction::triggered, this,
          &TitleBarMenuButton::onSaveWorkspace);
  connect(settingsAction, &QAction::triggered, this,
          &TitleBarMenuButton::onSettings);
  connect(terminalAction, &QAction::triggered, this,
          &TitleBarMenuButton::onTerminal);
  connect(aboutAction, &QAction::triggered, this, &TitleBarMenuButton::onAbout);

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
