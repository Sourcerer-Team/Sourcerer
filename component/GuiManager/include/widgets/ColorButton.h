#pragma once

#include <QColor>
#include <QPixmap>
#include <QPushButton>
#include <QWidget>

namespace GuiManager {

namespace Widgets {

class ColorButton : public QPushButton {
  Q_OBJECT
public:
  explicit ColorButton(const QColor &hoverColor, QWidget *parent = nullptr);
  explicit ColorButton(const QPixmap &icon, const QColor &hoverColor,
                       QWidget *parent = nullptr);

  const QColor &color() const { return _color; }
  void setColor(const QColor &color);

private:
  QColor _color;
};

} // namespace Widgets
} // namespace GuiManager
