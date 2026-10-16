#include "widgets/SettingsPaneItemWidget.h"

#include <Log.h>

#include <QHBoxLayout>
#include <QHoverEvent>

#define THREAD_ID "SettingsPaneItemWidget"

namespace GuiManager {

namespace Widgets {

SettingsPaneItemWidget::SettingsPaneItemWidget(
    const ConfigManager::ConfigItem &item, QWidget *parent)
    : QWidget{parent}, _item(item) {
  LOG_PERF(0, "Enter");

  setMouseTracking(true);
  setAttribute(Qt::WA_Hover, true);

  installEventFilter(this);

  LOG_PERF(0, "Exit");
}

void SettingsPaneItemWidget::inflate() {
  LOG_PERF(0, "Enter");

  load();

  QWidget *widget = createWidget();
  if (widget) {
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(widget);
  }

  LOG_PERF(0, "Exit");
}

bool SettingsPaneItemWidget::eventFilter(QObject *watched, QEvent *event) {
  if (!event || watched != this)
    return false;

  switch (event->type()) {
  case QEvent::HoverEnter: {
    return hover(static_cast<QHoverEvent *>(event), true);
  }
  case QEvent::HoverLeave: {
    return hover(static_cast<QHoverEvent *>(event), false);
  }
  default: {
    break;
  }
  }

  return QWidget::eventFilter(watched, event);
}

void SettingsPaneItemWidget::load() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

void SettingsPaneItemWidget::save() const {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

bool SettingsPaneItemWidget::hover(QHoverEvent *event, bool entered) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Entered is '{}'", entered);

  QPalette palette;
  if (entered)
    palette.setColor(QPalette::Window, _hoverColor);

  setAutoFillBackground(true);
  setPalette(palette);

  LOG_PERF(0, "Exit");
  return true;
}

} // namespace Widgets
} // namespace GuiManager
