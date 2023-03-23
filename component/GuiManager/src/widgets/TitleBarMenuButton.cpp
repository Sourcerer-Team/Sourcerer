#include "widgets/TitleBarMenuButton.h"

#include <Log.h>

#include <QKeySequence>

#define THREAD_ID "TitleBarMenuButton"

namespace GuiManager {

namespace Widgets {

TitleBarMenuButton::TitleBarMenuButton(QWidget *parent)
    : QPushButton{parent},
      _configManager(ConfigManager::ConfigManager::getInstance()) {
  setMouseTracking(true);
  setAttribute(Qt::WA_Hover, true);
  installEventFilter(this);

  setMenu(createMenu());

  setStyleSheet("QPushButton::menu-indicator{width:0px;}");

  connect(this, &TitleBarMenuButton::clicked, this,
          &TitleBarMenuButton::onClicked);
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

void TitleBarMenuButton::onClicked() { showMenu(); }

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

  const qlonglong menuFontSize =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarFontSize);

  LOG_DEBUG("Menu font size is %1 pixels.", menuFontSize);

  const qlonglong menuMinimumWidth =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarMenuMinimumSize);

  LOG_DEBUG("Menu minimum width is %1 pixels.", menuMinimumWidth);

  QFont font = menu->font();
  font.setPixelSize(menuFontSize);

  menu->setFont(font);
  menu->setMinimumWidth(250);

  return menu;
}

bool TitleBarMenuButton::hover(QHoverEvent *event, bool entered) {
  setDown(entered);
  return true;
}

} // namespace Widgets
} // namespace GuiManager
