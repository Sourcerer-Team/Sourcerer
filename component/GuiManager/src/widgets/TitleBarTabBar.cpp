#include "widgets/TitleBarTabBar.h"

#include <ConfigManager.h>
#include <Log.h>

#define THREAD_ID "TitleBarTabBar"

namespace GuiManager {

namespace Widgets {

TitleBarTabBar::TitleBarTabBar(QWidget *parent)
    : QTabBar{parent}, _resized{false} {
  LOG_PERF(0, "Enter");

  setTabsClosable(true);

  ConfigManager::ConfigManager &configManager =
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

QSize TitleBarTabBar::sizeHint() const {
  QSize size = QTabBar::sizeHint();

  QWidget *rootWidget = qobject_cast<QWidget *>(window());
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
