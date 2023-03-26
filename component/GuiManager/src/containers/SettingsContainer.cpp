#include "containers/SettingsContainer.h"
#include "containers/ui_SettingsContainer.h"

#include <Log.h>

#define THREAD_ID "SettingsContainer"

namespace GuiManager {

namespace Containers {

SettingsContainer::SettingsContainer(QWidget *parent)
    : ContainerWidget(ContainerType::Settings, parent),
      ui(new Ui::SettingsContainer),
      _configManager(ConfigManager::ConfigManager::getInstance()) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);

  const ConfigManager::ConfigGroup rootGroup = _configManager.getGroupped();

  QStandardItemModel *model = createModel(rootGroup);
  if (model)
    ui->groupsTreeView->setModel(model);

  ui->groupsPaneWidget->setGroup(rootGroup);

  LOG_PERF(0, "Exit");
}

SettingsContainer::~SettingsContainer() {
  LOG_PERF(0, "Enter");

  delete ui;

  LOG_PERF(0, "Exit");
}

static QStandardItem *
createItemForGroup(const ConfigManager::ConfigGroup &group) {
  if (group.isEmpty())
    return nullptr;

  QStandardItem *item = new QStandardItem(group.name());
  for (const auto &subgroup : group.subgroups()) {
    QStandardItem *subgroupItem = createItemForGroup(subgroup);
    item->appendRow(subgroupItem);
  }

  return item;
}

QStandardItemModel *SettingsContainer::createModel(
    const ConfigManager::ConfigGroup &rootGroup) const {
  LOG_PERF(0, "Enter");

  QStandardItemModel *model = new QStandardItemModel;
  QStandardItem *parentItem = model->invisibleRootItem();

  for (const auto &subgroup : rootGroup.subgroups()) {
    QStandardItem *subgroupItem = createItemForGroup(subgroup);
    if (subgroupItem)
      parentItem->appendRow(subgroupItem);
  }

  LOG_PERF(0, "Exit");

  return model;
}

} // namespace Containers
} // namespace GuiManager
