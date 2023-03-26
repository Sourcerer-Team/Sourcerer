#include "widgets/SettingsPaneWidget.h"

#include <Log.h>

#include <QLabel>
#include <QVBoxLayout>

#define THREAD_ID "SettingsPaneWidget"

namespace GuiManager {

namespace Widgets {

SettingsPaneWidget::SettingsPaneWidget(QWidget *parent) : QWidget{parent} {
  LOG_PERF(0, "Enter");

  setLayout(new QVBoxLayout(this));

  LOG_PERF(0, "Exit");
}

void SettingsPaneWidget::setGroup(const ConfigManager::ConfigGroup &group) {
  LOG_PERF(0, "Enter");

  _group = group;

  recreate();

  LOG_PERF(0, "Exit");
}

QWidget *SettingsPaneWidget::createItemWidget(const ConfigManager::ConfigItem &item)
{
  LOG_PERF(0, "Enter");

  LOG_DEBUG("Recreating item '%1'.", item.key());

  LOG_PERF(0, "Exit");

  return nullptr;
}

void SettingsPaneWidget::recreate() {
  LOG_PERF(0, "Enter");

  setUpdatesEnabled(false);

  qDeleteAll(findChildren<QWidget *>("", Qt::FindDirectChildrenOnly));

  QVBoxLayout *_layout = static_cast<QVBoxLayout*>(layout());

  for(const auto& subgroup : _group.subgroups()) {
    recreateGroup(subgroup, _layout);
  }

  setUpdatesEnabled(true);

  update();

  LOG_PERF(0, "Exit");
}

void SettingsPaneWidget::recreateGroup(const ConfigManager::ConfigGroup &group, QVBoxLayout *layout)
{
  LOG_PERF(0, "Enter");

  LOG_DEBUG("Recreating group '%1'.", group.name());

  QLabel *groupLabel = new QLabel(group.name(), this);

  layout->addWidget(groupLabel);

  for(const auto& item : group.items()) {
    QWidget *itemWidget = createItemWidget(item);
    if(itemWidget)
      layout->addWidget(itemWidget);
  }

  for(const auto& subgroup: group.subgroups()) {
    recreateGroup(subgroup, layout);
  }

  LOG_PERF(0, "Exit");
}

} // namespace Widgets
} // namespace GuiManager
