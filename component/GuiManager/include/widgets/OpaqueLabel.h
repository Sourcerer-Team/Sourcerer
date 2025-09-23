#pragma once

#include <QLabel>
#include <QPainter>

namespace GuiManager::Widgets {
class OpaqueLabel final : public QLabel {
public:
  using QLabel::QLabel;

  explicit OpaqueLabel(const QColor &backgroundColor, QWidget *parent = nullptr)
      : QLabel(parent), _backgroundColor(backgroundColor) {}

protected:
  void paintEvent(QPaintEvent *event) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(_backgroundColor);
    painter.setPen(Qt::NoPen);
    painter.drawRect(rect());
    QLabel::paintEvent(event);
  }

private:
  QColor _backgroundColor;
};

} // namespace GuiManager::Widgets