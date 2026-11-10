// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "widgets/SettingsPaneItemWidget.h"

#include <ConfigGroup.h>

#include <QMap>
#include <QWidget>

namespace GuiManager {

namespace Widgets {

class SettingsPaneWidget : public QWidget {
  Q_OBJECT
public:
  explicit SettingsPaneWidget(QWidget *parent = nullptr);

  void setGroup(const ConfigManager::ConfigGroup &group);

  int sectionPosition(const QString &sectionName) const;

private:
  SettingsPaneItemWidget *
  createItemWidget(const ConfigManager::ConfigItem &item);
  void recreate();
  void recreateGroup(const ConfigManager::ConfigGroup &group);

private:
  QMap<QString, QWidget *> _sections;
  ConfigManager::ConfigGroup _group;
};

} // namespace Widgets
} // namespace GuiManager
