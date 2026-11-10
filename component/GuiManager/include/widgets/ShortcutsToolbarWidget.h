// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "layouts/FlowLayout.h"

#include <QHBoxLayout>
#include <QWidget>

namespace GuiManager::Widgets {

class ShortcutsToolbarWidget final : public QWidget {
  Q_OBJECT

public:
  explicit ShortcutsToolbarWidget(QWidget *parent = nullptr);
  ~ShortcutsToolbarWidget() override;

  void addShortcutWidget(QWidget *widget);

  void clearShortcuts() const;

private:
  Layouts::FlowLayout *_shortcutsLayout;
};

} // namespace GuiManager::Widgets
