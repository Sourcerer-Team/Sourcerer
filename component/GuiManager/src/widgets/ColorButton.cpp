#include "widgets/ColorButton.h"

#include <Log.h>

#define THREAD_ID "ColorButton"

namespace GuiManager {

namespace Widgets {

ColorButton::ColorButton(const QColor &hoverColor, QWidget *parent)
    : QPushButton{parent} {
  LOG_PERF(0, "Enter");

  setColor(hoverColor);

  LOG_PERF(0, "Exit");
}

ColorButton::ColorButton(const QPixmap &icon, const QColor &hoverColor,
                         QWidget *parent)
    : QPushButton{parent} {
  LOG_PERF(0, "Enter");

  setIcon(icon);
  setColor(hoverColor);

  LOG_PERF(0, "Exit");
}

void ColorButton::setColor(const QColor &color) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Set button color '{}", color.name());

  _color = color;
  setStyleSheet(QString("background-color:%1").arg(_color.name()));

  LOG_PERF(0, "Exit");
}

} // namespace Widgets
} // namespace GuiManager
