#include "widgets/ErrorToast.h"

#include <StylesheetManager.h>

#include <Log.h>

#define THREAD_ID "ErrorToast"

namespace GuiManager::Widgets {

ErrorToast::ErrorToast(const QString &message, const QString &title,
                       QWidget *parent)
    : InfoToast(message, title, parent) {
  LOG_PERF(0, "Enter");

  const bool lightTheme =
      StylesheetManager::StylesheetManager::currentStylesheet() ==
      StylesheetManager::StylesheetKey::SourcererLight;
  if (lightTheme) {
    applyToastColors(QColor("#FFE7E1"), QColor("#D99A73"),
                     QColor("#3A241C"));
  } else {
    applyToastColors(QColor("#4B0000"), QColor("#B8860B"),
                     QColor("#FFFFFF"));
  }

  LOG_PERF(0, "Exit");
}

ErrorToast::~ErrorToast() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
