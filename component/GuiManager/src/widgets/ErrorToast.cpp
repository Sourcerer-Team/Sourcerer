#include "widgets/ErrorToast.h"

#include <Log.h>

#define THREAD_ID "ErrorToast"

namespace GuiManager::Widgets {

ErrorToast::ErrorToast(const QString &message, const QString &title,
                       QWidget *parent)
    : InfoToast(message, title, parent) {
  LOG_PERF(0, "Enter");

  setStyleSheet("font-size:14px;color:white;background-color:#4a2439;");

  LOG_PERF(0, "Exit");
}

ErrorToast::~ErrorToast() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets