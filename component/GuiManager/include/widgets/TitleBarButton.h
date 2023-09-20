#pragma once

#include "widgets/ColorButton.h"

#include <QColor>
#include <QEvent>
#include <QHoverEvent>
#include <QWidget>

namespace GuiManager {

namespace Widgets {

class TitleBarButton : public ColorButton {
  Q_OBJECT
public:
  explicit TitleBarButton(const QPixmap &icon, const QColor &hoverColor,
                          QWidget *parent = nullptr);

    // QObject interface
public:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool hover(QHoverEvent* event, bool entered);

private:
    QColor _hoverColor;
};

} // namespace Widgets
} // namespace GuiManager
