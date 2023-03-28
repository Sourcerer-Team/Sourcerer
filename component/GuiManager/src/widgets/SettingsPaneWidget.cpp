#include "widgets/SettingsPaneWidget.h"
#include "widgets/factories/SettingsPaneItemWidgetFactory.h"

#include <Log.h>

#include <QLabel>
#include <QVBoxLayout>

#define THREAD_ID "SettingsPaneWidget"

namespace GuiManager {

namespace Widgets {

SettingsPaneWidget::SettingsPaneWidget(QWidget *parent) : QWidget{parent} {
  LOG_PERF(0, "Enter");

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  LOG_PERF(0, "Exit");
}

void SettingsPaneWidget::setGroup(const ConfigManager::ConfigGroup &group) {
  LOG_PERF(0, "Enter");

  _group = group;

  recreate();

  LOG_PERF(0, "Exit");
}

SettingsPaneItemWidget *
SettingsPaneWidget::createItemWidget(const ConfigManager::ConfigItem &item) {
  LOG_PERF(0, "Enter");

  LOG_DEBUG("Recreating item '%1'.", item.key());

  Factories::SettingsPaneItemWidgetFactory &factory =
      Factories::SettingsPaneItemWidgetFactory::getInstance();
  SettingsPaneItemWidget *widget = factory.create(item, this);

  LOG_PERF(0, "Exit");

  return widget;
}

void SettingsPaneWidget::recreate() {
  LOG_PERF(0, "Enter");

  setUpdatesEnabled(false);

  qDeleteAll(findChildren<QWidget *>("", Qt::FindDirectChildrenOnly));
  delete layout();

  new QVBoxLayout(this);

  for (const auto &subgroup : _group.subgroups()) {
    recreateGroup(subgroup);
  }

  setUpdatesEnabled(true);

  update();

  LOG_PERF(0, "Exit");
}

void SettingsPaneWidget::recreateGroup(const ConfigManager::ConfigGroup &group) {
  LOG_PERF(0, "Enter");

  LOG_DEBUG("Recreating group '%1'.", group.name());

  QString groupName = group.name();
  if (groupName.length() > 0)
    groupName = groupName.first(1).toUpper() + groupName.mid(1);

  QLabel *groupLabel = new QLabel(groupName, this);
  groupLabel->setObjectName("groupLabel");

  layout()->addWidget(groupLabel);

  for (const auto &item : group.items()) {
    SettingsPaneItemWidget *itemWidget = createItemWidget(item);
    if (itemWidget) {
      itemWidget->inflate();
      layout()->addWidget(itemWidget);
    }
  }

  for (const auto &subgroup : group.subgroups()) {
    recreateGroup(subgroup);
  }

  LOG_PERF(0, "Exit");
}

} // namespace Widgets
} // namespace GuiManager
