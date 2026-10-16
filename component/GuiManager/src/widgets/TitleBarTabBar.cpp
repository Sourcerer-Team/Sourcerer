#include "widgets/TitleBarTabBar.h"

#include <ConfigManager.h>
#include <Log.h>

#include <QMouseEvent>

#define THREAD_ID "TitleBarTabBar"

namespace GuiManager {

namespace Widgets {

TitleBarTabBar::TitleBarTabBar(QWidget *parent)
    : QTabBar{parent}, _resized{false}, _pressedIndex{-1}, _pressedX{0},
      _pinnedPressed{false} {
  LOG_PERF(0, "Enter");

  setTabsClosable(true);
  setMovable(true);

  const ConfigManager::ConfigManager &configManager =
      ConfigManager::ConfigManager::getInstance();

  _maximumTabWidth =
      configManager.getLong(ConfigManager::ConfigKey::TitleBarMaxTabWidth);

  LOG_DEBUG("Maximum tab width is {} pixels.", _maximumTabWidth);

  LOG_PERF(0, "Exit");
}

void TitleBarTabBar::paintEvent(QPaintEvent *event) {
  LOG_PERF(0, "Enter");

  QTabBar::paintEvent(event);

  if (_resized) {
    _resized = false;
    emit resizeTabBar();
  }

  LOG_PERF(0, "Exit");
}

void TitleBarTabBar::resizeEvent(QResizeEvent *event) {
  LOG_PERF(0, "Enter");

  _resized = true;

  QTabBar::resizeEvent(event);

  LOG_PERF(0, "Exit");
}

void TitleBarTabBar::setTabPinned(const int index, const bool pinned) {
  LOG_PERF(0, "Enter");

  setTabData(index, pinned);

  LOG_PERF(0, "Exit");
}

bool TitleBarTabBar::isTabPinned(const int index) const {
  return tabData(index).toBool();
}

int TitleBarTabBar::pinnedTabsCount() const {
  int pinned = 0;
  while (pinned < count() && isTabPinned(pinned))
    ++pinned;

  return pinned;
}

void TitleBarTabBar::mousePressEvent(QMouseEvent *event) {
  LOG_PERF(0, "Enter");

  if (event->button() == Qt::LeftButton) {
    _pressedIndex = tabAt(event->position().toPoint());
    _pressedX = event->position().x();

    // Pinned tab can be selected, but not dragged.
    if (_pressedIndex != -1 && isTabPinned(_pressedIndex) && isMovable()) {
      _pinnedPressed = true;
      setMovable(false);
    }
  }

  QTabBar::mousePressEvent(event);

  LOG_PERF(0, "Exit");
}

void TitleBarTabBar::mouseMoveEvent(QMouseEvent *event) {
  const int pinned = pinnedTabsCount();

  if (!(event->buttons() & Qt::LeftButton) || !isMovable() || pinned == 0 ||
      _pressedIndex < pinned || _pressedIndex >= count()) {
    QTabBar::mouseMoveEvent(event);
    return;
  }

  // Don't let the dragged tab's left edge go past the last pinned tab, so
  // it can't be dropped before it.
  const qreal minimumX = _pressedX - (tabRect(_pressedIndex).left() -
                                      tabRect(pinned - 1).right() - 1);

  if (event->position().x() >= minimumX) {
    QTabBar::mouseMoveEvent(event);
    return;
  }

  const QPointF offset(minimumX - event->position().x(), 0);
  QMouseEvent clamped(event->type(), event->position() + offset,
                      event->scenePosition() + offset,
                      event->globalPosition() + offset, event->button(),
                      event->buttons(), event->modifiers(),
                      event->pointingDevice());
  QTabBar::mouseMoveEvent(&clamped);
  event->setAccepted(clamped.isAccepted());
}

void TitleBarTabBar::mouseReleaseEvent(QMouseEvent *event) {
  LOG_PERF(0, "Enter");

  QTabBar::mouseReleaseEvent(event);

  if (event->button() == Qt::LeftButton) {
    if (_pinnedPressed) {
      _pinnedPressed = false;
      setMovable(true);
    }

    _pressedIndex = -1;
  }

  LOG_PERF(0, "Exit");
}

QSize TitleBarTabBar::sizeHint() const {
  QSize size = QTabBar::sizeHint();

  const QWidget *rootWidget = qobject_cast<QWidget *>(window());
  if (!rootWidget)
    return size;

  const int _count = count();
  int width = _maximumTabWidth;
  if (_count > 0)
    width = std::min(_maximumTabWidth * _count, rootWidget->width());

  size.setWidth(width);

  return size;
}

} // namespace Widgets
} // namespace GuiManager
