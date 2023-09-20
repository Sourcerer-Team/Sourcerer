#include "widgets/TitleBarButton.h"

#include <Log.h>

#define THREAD_ID "TitleBarButton"

namespace GuiManager {

namespace Widgets {

TitleBarButton::TitleBarButton(const QPixmap &icon, const QColor &hoverColor,
                               QWidget *parent)
    : ColorButton(icon, QColor::Invalid, parent) {
  LOG_PERF(0, "Enter");

  setMouseTracking(true);
  setAttribute(Qt::WA_Hover, true);
  installEventFilter(this);

  _hoverColor = hoverColor;

  LOG_PERF(0, "Exit");
}

bool TitleBarButton::eventFilter(QObject *watched, QEvent *event) {
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

bool TitleBarButton::hover(QHoverEvent *event, bool entered) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Entered is '{}'", entered);

  setColor(entered ? _hoverColor : color());
  setDown(entered);

  LOG_PERF(0, "Exit");

  return true;
}

} // namespace Widgets
} // namespace GuiManager
