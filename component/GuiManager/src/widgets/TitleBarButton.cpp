#include "widgets/TitleBarButton.h"

#include <Log.h>

#define THREAD_ID "TitleBarButton"

namespace GuiManager::Widgets {

TitleBarButton::TitleBarButton(const QPixmap &icon, QWidget *parent)
    : QPushButton{parent} {
  LOG_PERF(0, "Enter");

  setIcon(icon);
  setFlat(true);
  setMouseTracking(true);
  setAttribute(Qt::WA_Hover, true);
  setAttribute(Qt::WA_StyledBackground, true);
  setAutoFillBackground(false);
  setFocusPolicy(Qt::NoFocus);
  setIconSize(QSize(16, 16));

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
