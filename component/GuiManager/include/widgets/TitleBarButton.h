#pragma once

#include <QPixmap>
#include <QPushButton>
#include <QWidget>

namespace GuiManager {

namespace Widgets {

class TitleBarButton : public QPushButton {
  Q_OBJECT
public:
  explicit TitleBarButton(const QPixmap &icon, QWidget *parent = nullptr);
};

} // namespace Widgets
} // namespace GuiManager
