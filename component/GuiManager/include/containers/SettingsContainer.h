#pragma once

#include "widgets/ContainerWidget.h"

#include <ConfigGroup.h>
#include <ConfigManager.h>

#include <QStandardItemModel>
#include <QWidget>

namespace Ui {
class SettingsContainer;
}

namespace GuiManager {

namespace Containers {

using namespace Widgets;

class SettingsContainer : public ContainerWidget {
  Q_OBJECT

public:
  explicit SettingsContainer(QWidget *parent = nullptr);
  virtual ~SettingsContainer();

private slots:
  void onSplitterMoved(int pos, int index);
  void onTreeViewClicked(const QModelIndex &index);

private:
  QStandardItemModel *
  createModel(const ConfigManager::ConfigGroup &rootGroup) const;

private:
  Ui::SettingsContainer *ui;
  ConfigManager::ConfigManager &_configManager;
};

} // namespace Containers
} // namespace GuiManager
