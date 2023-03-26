#pragma once

#include <QHoverEvent>
#include <QPushButton>
#include <QWidget>

namespace GuiManager {

namespace Widgets {

class TitleBarTabBarCloseButton : public QPushButton {
  Q_OBJECT
public:
  TitleBarTabBarCloseButton(int index, QWidget *parent = nullptr);

  int index() const { return _index; }

  void setIndex(int index) { _index = index; }

  // QObject interface
public:
  bool eventFilter(QObject *watched, QEvent *event) override;

signals:
  void clickedClose(int index);

private slots:
  void onClicked();

private:
  bool hover(QHoverEvent* event, bool entered);

private:
  int _index;
};

} // namespace Widgets
} // namespace GuiManager
