#include "widgets/ColorButton.h"

#include <Log.h>

#define THREAD_ID "ColorButton"

namespace GuiManager {

namespace Widgets {

ColorButton::ColorButton(const QColor &color, QWidget *parent)
  : QPushButton{parent}, _color(color) {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

void ColorButton::setColor(const QColor &color)
{
  _color = color;
}

} // namespace Widgets
} // namespace GuiManager
