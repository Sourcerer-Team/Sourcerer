#pragma once

#include <QGroupBox>
#include <QPainter>

namespace GuiManager::Widgets {
class OpaqueGroupBox final : public QGroupBox {
public:
  using QGroupBox::QGroupBox;

  explicit OpaqueGroupBox(const QColor &backgroundColor,
                          QWidget *parent = nullptr)
      : QGroupBox(parent), _backgroundColor(backgroundColor) {}

protected:
  void paintEvent(QPaintEvent *event) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(_backgroundColor);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect(), 10, 10); // solid white
    QGroupBox::paintEvent(event);            // draw title and children
  }

private:
  QColor _backgroundColor;
};

} // namespace GuiManager::Widgets