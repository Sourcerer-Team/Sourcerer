#pragma once

#include <QPainter>
#include <QPushButton>

namespace GuiManager::Widgets {
class OpaquePushButton final : public QPushButton {
public:
  using QPushButton::QPushButton;

  explicit OpaquePushButton(const QColor &backgroundColor,
                            QWidget *parent = nullptr)
      : QPushButton(parent), _backgroundColor(backgroundColor) {}

protected:
  void paintEvent(QPaintEvent *event) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(_backgroundColor);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect(), 5, 5);
    QPushButton::paintEvent(event);
  }

private:
  QColor _backgroundColor;
};

} // namespace GuiManager::Widgets