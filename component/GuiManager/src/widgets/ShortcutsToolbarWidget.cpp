#include "widgets/ShortcutsToolbarWidget.h"

#include <Log.h>

#define THREAD_ID "ShortcutsToolbarWidget"

namespace GuiManager::Widgets {

ShortcutsToolbarWidget::ShortcutsToolbarWidget(QWidget *parent)
    : QWidget(parent), _shortcutsLayout(new Layouts::FlowLayout(this)) {
  LOG_PERF(0, "Enter");

  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

  _shortcutsLayout->setContentsMargins(0, 0, 0, 0);
  _shortcutsLayout->setSpacing(6);

  setLayout(_shortcutsLayout);

  LOG_PERF(0, "Exit");
}

ShortcutsToolbarWidget::~ShortcutsToolbarWidget() {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

void ShortcutsToolbarWidget::addShortcutWidget(QWidget *widget) {
  LOG_PERF(0, "Enter");

  if (!widget) {
    LOG_PERF(0, "Exit");
    return;
  }

  widget->setParent(this);

  _shortcutsLayout->addWidget(widget);

  LOG_PERF(0, "Exit");
}

void ShortcutsToolbarWidget::clearShortcuts() const {
  LOG_PERF(0, "Enter");

  auto *layout_ = qobject_cast<QVBoxLayout *>(layout());

  Q_ASSERT(layout_ != nullptr);

  if (!layout_) {
    LOG_PERF(0, "Exit");
    return;
  }

  for (int i = 0; i < layout_->count(); ++i) {
    QWidget *widget = layout_->itemAt(i)->widget();
    Q_ASSERT(widget != nullptr);
    if (widget) {
      layout_->removeWidget(widget);
      delete widget;
    }
  }

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
