// MIT License
// Copyright (c) 2026 Sourcerer


#pragma once

#include <QToolBox>

#include <QWidget>

namespace GuiManager {

namespace Widgets {

class ShadeIconToolbox : public QToolBox {
  Q_OBJECT
public:
  explicit ShadeIconToolbox(QWidget *parent = nullptr);
};

} // namespace Widgets
} // namespace GuiManager
