
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
  setStyleSheet("text-align: center top; QPushButton:hover { background-color: rgba(255,255,255,0.2); }");

  LOG_PERF(0, "Exit");
}

} // namespace Widgets
} // namespace GuiManager
