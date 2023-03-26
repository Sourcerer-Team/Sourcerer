#include "widgets/TitleBarTabBar.h"

namespace GuiManager {

namespace Widgets {

TitleBarTabBar::TitleBarTabBar(QWidget *parent) : QTabBar{parent} {}

void TitleBarTabBar::resizeEvent(QResizeEvent *event) { emit resizeTabBar(); }

QSize TitleBarTabBar::sizeHint() const {
  QSize size = QTabBar::sizeHint();

  QWidget *rootWidget = qobject_cast<QWidget *>(window());
  if (!rootWidget)
    return size;

  const int _count = count();
  int width = 285;
  if (_count > 0)
    width = std::min(285 * _count, rootWidget->width());

  size.setWidth(width);

  return size;
}

} // namespace Widgets
} // namespace GuiManager
