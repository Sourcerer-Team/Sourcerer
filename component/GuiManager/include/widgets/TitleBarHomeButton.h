#pragma once

#include <QHoverEvent>
#include <QPushButton>
#include <QWidget>

namespace GuiManager {

namespace Widgets {

class TitleBarHomeButton : public QPushButton {
  Q_OBJECT
public:
  explicit TitleBarHomeButton(QWidget *parent = nullptr);

  // QObject interface
public:
  bool eventFilter(QObject *watched, QEvent *event) override;

signals:
  void home();

private slots:
  void onClicked();

private:
  bool hover(QHoverEvent *event, bool entered);
};

} // namespace Widgets
} // namespace GuiManager
