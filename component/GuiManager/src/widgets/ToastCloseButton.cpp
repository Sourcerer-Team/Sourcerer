
#include "widgets/ToastCloseButton.h"

#include <Log.h>

#define THREAD_ID "ToastCloseButton"

namespace GuiManager {

namespace Widgets {

ToastCloseButton::ToastCloseButton(QWidget *parent) : QPushButton{parent} {
  LOG_PERF(0, "Enter");

  setFlat(true);
  setMouseTracking(true);
  setAttribute(Qt::WA_Hover, true);
  setAttribute(Qt::WA_TransparentForMouseEvents, false);

  setText(tr("x"));
  setStyleSheet(
      "QPushButton { text-align: center top; color: palette(window-text); } "
      "QPushButton:hover { background-color: rgba(127,127,127,0.2); }");

  LOG_PERF(0, "Exit");
}

} // namespace Widgets
} // namespace GuiManager
