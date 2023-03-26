#pragma once

#include <ConfigGroup.h>

#include <QVBoxLayout>
#include <QWidget>

namespace GuiManager {

namespace Widgets {

class SettingsPaneWidget : public QWidget {
  Q_OBJECT
public:
  explicit SettingsPaneWidget(QWidget *parent = nullptr);

  void setGroup(const ConfigManager::ConfigGroup &group);

private:
  QWidget *createItemWidget(const ConfigManager::ConfigItem& item);
  void recreate();
  void recreateGroup(const ConfigManager::ConfigGroup& group, QVBoxLayout* layout);

private:
  ConfigManager::ConfigGroup _group;
};

} // namespace Widgets
} // namespace GuiManager
