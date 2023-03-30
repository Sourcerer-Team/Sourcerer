#include "widgets/TitleBarHomeButton.h"

#include <Log.h>

#define THREAD_ID "TitleBarHomeButton"

namespace GuiManager {

namespace Widgets {

TitleBarHomeButton::TitleBarHomeButton(QWidget *parent) : QPushButton{parent} {
  LOG_PERF(0, "Enter");

  setMouseTracking(true);
  setAttribute(Qt::WA_Hover, true);
  installEventFilter(this);

  connect(this, &TitleBarHomeButton::clicked, this,
          &TitleBarHomeButton::onClicked);

  LOG_PERF(0, "Exit");
}

bool TitleBarHomeButton::eventFilter(QObject *watched, QEvent *event) {
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

void TitleBarHomeButton::onClicked() {
  LOG_PERF(0, "Enter");

  emit home();

  LOG_PERF(0, "Exit");
}

bool TitleBarHomeButton::hover(QHoverEvent *event, bool entered) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Entered is '{}'", entered);

  setDown(entered);

  LOG_PERF(0, "Exit");
  return true;
}

} // namespace Widgets
} // namespace GuiManager
