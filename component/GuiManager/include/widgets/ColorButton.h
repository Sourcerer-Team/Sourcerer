#pragma once

#include <QColor>
#include <QPushButton>
#include <QWidget>

namespace GuiManager {

namespace Widgets {

class ColorButton : public QPushButton {
  Q_OBJECT
public:
  ColorButton(const QColor &color, QWidget *parent = nullptr);

  const QColor& color() const { return _color; }
  void setColor(const QColor& color);

private:
  QColor _color;
};

} // namespace Widgets
} // namespace GuiManager
