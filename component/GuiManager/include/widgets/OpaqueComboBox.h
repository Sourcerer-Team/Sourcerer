#pragma once

#include <QComboBox>
#include <QPainter>

namespace GuiManager::Widgets {
class OpaqueComboBox final : public QComboBox {
public:
  using QComboBox::QComboBox;

  explicit OpaqueComboBox(const QColor &backgroundColor,
                          QWidget *parent = nullptr)
      : QComboBox(parent), _backgroundColor(backgroundColor) {}

protected:
  void paintEvent(QPaintEvent *event) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(_backgroundColor);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect(), 5, 5);
    QComboBox::paintEvent(event);
  }

private:
  QColor _backgroundColor;
};

} // namespace GuiManager::Widgets
