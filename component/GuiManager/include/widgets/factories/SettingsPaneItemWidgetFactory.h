#pragma once

#include "widgets/SettingsPaneItemWidget.h"

#include <ConfigGroup.h>

#include <QWidget>

namespace GuiManager {

namespace Widgets {

namespace Factories {

class SettingsPaneItemWidgetFactory {
public:
  static SettingsPaneItemWidgetFactory &getInstance();

  SettingsPaneItemWidget *create(const ConfigManager::ConfigItem &item,
                                 QWidget *parent = nullptr) const;

private:
  SettingsPaneItemWidgetFactory();
  ~SettingsPaneItemWidgetFactory();
  SettingsPaneItemWidgetFactory &
  operator=(const SettingsPaneItemWidgetFactory &) = delete;
};

} // namespace Factories
} // namespace Widgets
} // namespace GuiManager
