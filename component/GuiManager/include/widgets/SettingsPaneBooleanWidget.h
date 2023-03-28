#pragma once

#include "widgets/SettingsPaneItemWidget.h"

#include <ConfigGroup.h>

#include <QWidget>

namespace GuiManager {

namespace Widgets {

class SettingsPaneBooleanWidget : public SettingsPaneItemWidget {
  Q_OBJECT
public:
  SettingsPaneBooleanWidget(const ConfigManager::ConfigItem &item,
                            QWidget *parent = nullptr);

  // SettingsPaneItemWidget interface
protected:
  QWidget *createWidget() override;
  void load() override;
  void save() const override;

private slots:
  void onStateChanged(int state);
};

} // namespace Widgets
} // namespace GuiManager
