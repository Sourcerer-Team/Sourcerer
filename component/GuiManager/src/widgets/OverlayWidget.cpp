#include "widgets/OverlayWidget.h"

#ifdef Q_OS_WIN
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

namespace GuiManager::Widgets {

#ifdef Q_OS_WIN
void enableBlur(const WId wid) {
  // For older Windows 7/8 style blur:
  DWM_BLURBEHIND bb = {0};
  bb.dwFlags = DWM_BB_ENABLE;
  bb.fEnable = TRUE;
  bb.hRgnBlur = NULL;
  DwmEnableBlurBehindWindow(reinterpret_cast<HWND>(wid), &bb);

  // For Windows 10 Acrylic / 11 Mica, use SetWindowCompositionAttribute
  // with ACCENT_POLICY = ACCENT_ENABLE_BLURBEHIND or
  // ACCENT_ENABLE_ACRYLICBLURBEHIND
}
#else
void enableBlur(const WId wid) {}
#endif


}
