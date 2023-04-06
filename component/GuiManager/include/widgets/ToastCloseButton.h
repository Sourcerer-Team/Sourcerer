
#pragma once

#include <QPushButton>

#include <QHoverEvent>
#include <QWidget>

namespace GuiManager {

namespace Widgets {

class ToastCloseButton : public QPushButton {
  Q_OBJECT
public:
  explicit ToastCloseButton(QWidget *parent = nullptr);

  // QObject interface
public:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  bool hover(QHoverEvent *event, bool entered);
};

} // namespace Widgets
} // namespace GuiManager
