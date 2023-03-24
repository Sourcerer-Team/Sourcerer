#include "widgets/TitleBarMenuButton.h"

#include <Log.h>

#include <QKeySequence>

#define THREAD_ID "TitleBarMenuButton"

namespace GuiManager {

namespace Widgets {

TitleBarMenuButton::TitleBarMenuButton(QWidget *parent)
    : QPushButton{parent},
      _configManager(ConfigManager::ConfigManager::getInstance()) {
  LOG_PERF(0, "Enter");

  //setObjectName("titleBarMenuButton");
  setMouseTracking(true);
  setAttribute(Qt::WA_Hover, true);
  installEventFilter(this);

  setMenu(createMenu());

  //setStyleSheet("GuiManager--Widgets--TitleBarMenuButton::menu-indicator{width:0px;}");

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

  return QPushButton::eventFilter(watched, event);
}

void TitleBarMenuButton::onClicked() {
  LOG_PERF(0, "Enter");

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

void TitleBarMenuButton::onSettings() {
  LOG_PERF(0, "Enter");

  emit settings();

  LOG_PERF(0, "Exit");
}

void TitleBarMenuButton::onAbout() {
  LOG_PERF(0, "Enter");

  emit about();

  LOG_PERF(0, "Exit");
}

QMenu *TitleBarMenuButton::createMenu() {
  QMenu *menu = new QMenu(this);

  QAction *openAction = new QAction(menu);
  openAction->setText(tr("&Open"));
  openAction->setShortcut(QKeySequence::Open);

  QAction *cloneAction = new QAction(menu);
  cloneAction->setText(tr("&Clone"));

  QAction *initAction = new QAction(menu);
  initAction->setText(tr("&Init"));
  initAction->setShortcut(QKeySequence::New);

  QAction *settingsAction = new QAction(menu);
  settingsAction->setText(tr("Settings"));

  QAction *aboutAction = new QAction(menu);
  aboutAction->setText(tr("About"));

  menu->addAction(openAction);
  menu->addAction(cloneAction);
  menu->addAction(initAction);
  menu->addSeparator();
  menu->addAction(settingsAction);
  menu->addAction(aboutAction);

  const qlonglong menuMinimumWidth =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarMenuMinimumSize);

  LOG_DEBUG("Menu minimum width is %1 pixels.", menuMinimumWidth);

  menu->setMinimumWidth(menuMinimumWidth);

  connect(openAction, &QAction::triggered, this, &TitleBarMenuButton::onOpen);
  connect(cloneAction, &QAction::triggered, this, &TitleBarMenuButton::onClone);
  connect(initAction, &QAction::triggered, this, &TitleBarMenuButton::onInit);
  connect(settingsAction, &QAction::triggered, this,
          &TitleBarMenuButton::onSettings);
  connect(aboutAction, &QAction::triggered, this, &TitleBarMenuButton::onAbout);

  return menu;
}

bool TitleBarMenuButton::hover(QHoverEvent *event, bool entered) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Entered is '%1'", entered);

  setDown(entered);

  LOG_PERF(0, "Exit");
  return true;
}

} // namespace Widgets
} // namespace GuiManager
