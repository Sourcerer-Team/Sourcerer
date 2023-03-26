#include "widgets/TitleBarTabBarCloseButton.h"

#include <Log.h>

#define THREAD_ID "TitleBarTabBarCloseButton"

namespace GuiManager {

namespace Widgets {

TitleBarTabBarCloseButton::TitleBarTabBarCloseButton(int index, QWidget *parent)
    : QPushButton{parent}, _index{index} {
  LOG_PERF(0, "Enter");

  setMouseTracking(true);
  setAttribute(Qt::WA_Hover, true);
  setFlat(true);

  installEventFilter(this);

  setText(tr("x"));
  setMaximumSize(24, 24);
  setDown(false);

  connect(this, &TitleBarTabBarCloseButton::clicked, this,
          &TitleBarTabBarCloseButton::onClicked);

  LOG_PERF(0, "Exit");
}

bool TitleBarTabBarCloseButton::eventFilter(QObject *watched, QEvent *event)
{
  if(!event || watched != this)
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

void TitleBarTabBarCloseButton::onClicked()
{
  LOG_PERF(0, "Enter");

  emit clickedClose(_index);

  LOG_PERF(0, "Exit");
}

bool TitleBarTabBarCloseButton::hover(QHoverEvent *event, bool entered) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Entered is '%1'", entered);

  setDown(entered);

  LOG_PERF(0, "Exit");
  return true;
}

} // namespace Widgets
} // namespace GuiManager
