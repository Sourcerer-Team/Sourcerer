#pragma once

#include "widgets/SettingsPaneItemWidget.h"

#include <ConfigGroup.h>

#include <QWidget>

namespace GuiManager {

namespace Widgets {

class SettingsPaneStringWidget : public SettingsPaneItemWidget {
  Q_OBJECT
public:
  SettingsPaneStringWidget(const ConfigManager::ConfigItem &item,
                           QWidget *parent = nullptr);

  // SettingsPaneItemWidget interface
protected:
  QWidget *createWidget() override;
  void load() override;
  void save() const override;

private slots:
  void onTextChanged(const QString &text);
};

} // namespace Widgets
} // namespace GuiManager
