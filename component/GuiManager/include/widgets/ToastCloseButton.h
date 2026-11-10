// MIT License
// Copyright (c) 2026 Sourcerer


#pragma once

#include <QPushButton>

#include <QHoverEvent>
#include <QWidget>

namespace GuiManager {

namespace Widgets {

class ToastCloseButton : public QPushButton {
  Q_OBJECT
public:
  explicit ToastCloseButton(QWidget *parent = nullptr);
};

} // namespace Widgets
} // namespace GuiManager
