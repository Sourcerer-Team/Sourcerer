#include "widgets/ErrorToast.h"

#include <Log.h>

#define THREAD_ID "ErrorToast"

namespace GuiManager::Widgets {

ErrorToast::ErrorToast(const QString &message, const QString &title,
                       QWidget *parent)
    : InfoToast(message, title, parent) {
  LOG_PERF(0, "Enter");

  // Apply dark red background and dark yellow title
  applyToastColors(QColor("#4B0000"), QColor("#B8860B"));

  LOG_PERF(0, "Exit");
}

ErrorToast::~ErrorToast() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
