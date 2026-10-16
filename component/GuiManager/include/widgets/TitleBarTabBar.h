#pragma once

#include <QTabBar>
#include <QWidget>

namespace GuiManager {

namespace Widgets {

class TitleBarTabBar : public QTabBar {
  Q_OBJECT
public:
  explicit TitleBarTabBar(QWidget *parent = nullptr);

  // Pinned tabs stay in front of all other tabs, they can't be dragged and
  // other tabs can't be dropped before them.
  void setTabPinned(int index, bool pinned);
  bool isTabPinned(int index) const;

signals:
  void resizeTabBar();

  // QWidget interface
protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

  // QTabBar interface
protected:
  QSize sizeHint() const override;

private:
  int pinnedTabsCount() const;

  bool _resized;
  int _maximumTabWidth;
  int _pressedIndex;
  qreal _pressedX;
  bool _pinnedPressed;
};

} // namespace Widgets
} // namespace GuiManager
