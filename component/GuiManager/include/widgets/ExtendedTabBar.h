#pragma once

#include <QTabBar>
#include <QWidget>

namespace GuiManager::Widget {
class ExtendedTabBar final : public QTabBar {
  QWidget *_trailingWidget;

public:
  explicit ExtendedTabBar(QWidget *trailingWidget, QWidget *parent = nullptr)
      : QTabBar(parent), _trailingWidget(trailingWidget) {
    if (_trailingWidget) {
      _trailingWidget->setFixedSize(24, 24);
      _trailingWidget->show();
    }
  }

protected:
  void resizeEvent(QResizeEvent *event) override {
    QTabBar::resizeEvent(event);
    repositionTrailingWidget();
  }

  void repositionTrailingWidget() const {
    if (!_trailingWidget)
      return;

    if (count() == 0)
      return;

    const QRect lastTab = tabRect(count() - 1);
    int x = lastTab.right() + 8;
    int y = lastTab.top() + (lastTab.height() - _trailingWidget->height()) / 2;
    _trailingWidget->move(x, y);
  }
};

} // namespace GuiManager::Widget