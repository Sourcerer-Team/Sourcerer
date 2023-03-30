#include "containers/SettingsContainer.h"
#include "containers/ui_SettingsContainer.h"

#include <Log.h>

#include <QScrollBar>

#define THREAD_ID "SettingsContainer"

namespace GuiManager {

namespace Containers {

SettingsContainer::SettingsContainer(QWidget *parent)
    : ContainerWidget(ContainerType::Settings, parent),
      ui(new Ui::SettingsContainer),
      _configManager(ConfigManager::ConfigManager::getInstance()) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);
  ui->splitter->setStretchFactor(0, 1);
  ui->splitter->setStretchFactor(1, 10);
  ui->groupsTreeView->header()->setVisible(false);
  ui->groupsTreeView->header()->setSectionResizeMode(
      QHeaderView::ResizeToContents);

  const ConfigManager::ConfigGroup rootGroup = _configManager.getGroupped();

  QStandardItemModel *model = createModel(rootGroup);
  if (model)
    ui->groupsTreeView->setModel(model);

  ui->groupsPaneWidget->setGroup(rootGroup);

  const QByteArray splitterState = QByteArray::fromHex(
      _configManager.getString(ConfigManager::ConfigKey::SettingsSplitterPos)
          .toUtf8());
  ui->splitter->restoreState(splitterState);

  connect(ui->splitter, &QSplitter::splitterMoved, this,
          &SettingsContainer::onSplitterMoved);
  connect(ui->groupsTreeView, &QTreeView::clicked, this,
          &SettingsContainer::onTreeViewClicked);

  LOG_PERF(0, "Exit");
}

SettingsContainer::~SettingsContainer() {
  LOG_PERF(0, "Enter");

  delete ui;

  LOG_PERF(0, "Exit");
}

void SettingsContainer::onSplitterMoved(int pos, int index) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("pos = {}, index = {}", pos, index);

  const QString splitterState = ui->splitter->saveState().toHex();
  _configManager.setString(ConfigManager::ConfigKey::SettingsSplitterPos,
                           splitterState);
  _configManager.save();

  LOG_PERF(0, "Exit");
}

void SettingsContainer::onTreeViewClicked(const QModelIndex &index) {
  LOG_PERF(0, "Enter");

  const QString sectionName = index.data().toString();
  LOG_DEBUG("Clicked on section name '{}'", sectionName);

  const int sectionPosition = ui->groupsPaneWidget->sectionPosition(sectionName);
  if (sectionPosition != -1)
    ui->groupsPaneScrollArea->verticalScrollBar()->setValue(sectionPosition);

  LOG_PERF(0, "Exit");
}

static QStandardItem *
createItemForGroup(const ConfigManager::ConfigGroup &group) {
  if (group.isEmpty())
    return nullptr;

  QString groupName = group.name();
  if (groupName.length() > 0)
    groupName = groupName.first(1).toUpper() + groupName.mid(1);

  QStandardItem *item = new QStandardItem(groupName);
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
