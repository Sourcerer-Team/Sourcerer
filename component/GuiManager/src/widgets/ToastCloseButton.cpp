
#include "widgets/ToastCloseButton.h"

#include <Log.h>

#define THREAD_ID "ToastCloseButton"

namespace GuiManager {

namespace Widgets {

ToastCloseButton::ToastCloseButton(QWidget *parent) : QPushButton{parent} {
  LOG_PERF(0, "Enter");

  setFlat(true);
  setMouseTracking(true);
  setAttribute(Qt::WA_Hover, true);
  installEventFilter(this);

  setText(tr("x"));

  LOG_PERF(0, "Exit");
}

bool ToastCloseButton::eventFilter(QObject *watched, QEvent *event) {
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

bool ToastCloseButton::hover(QHoverEvent *event, bool entered) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Entered is '{}'", entered);

  setDown(entered);

  LOG_PERF(0, "Exit");
  return true;
}

} // namespace Widgets
} // namespace GuiManager
