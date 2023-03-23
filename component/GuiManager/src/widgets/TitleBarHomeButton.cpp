#include "widgets/TitleBarHomeButton.h"

namespace GuiManager {

namespace Widgets {

TitleBarHomeButton::TitleBarHomeButton(QWidget *parent) : QPushButton{parent} {
  setMouseTracking(true);
  setAttribute(Qt::WA_Hover, true);
  installEventFilter(this);

  connect(this, &TitleBarHomeButton::clicked, this,
          &TitleBarHomeButton::onClicked);
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

void TitleBarHomeButton::onClicked() { emit home(); }

bool TitleBarHomeButton::hover(QHoverEvent *event, bool entered) {
  setDown(entered);
  return true;
}

} // namespace Widgets
} // namespace GuiManager
