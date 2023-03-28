#pragma once

#include "widgets/SettingsPaneItemWidget.h"

#include <ConfigGroup.h>

#include <QWidget>

namespace GuiManager {

namespace Widgets {

class SettingsPaneColorWidget : public SettingsPaneItemWidget {
  Q_OBJECT
public:
  SettingsPaneColorWidget(const ConfigManager::ConfigItem &item,
                          QWidget *parent = nullptr);

  // SettingsPaneItemWidget interface
protected:
  QWidget *createWidget() override;
  void load() override;
  void save() const override;

private slots:
  void onClicked();
};

} // namespace Widgets
} // namespace GuiManager
