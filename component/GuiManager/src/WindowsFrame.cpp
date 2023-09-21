#include "WindowsFrame.h"

#include <Log.h>

#include <WinUser.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <objidl.h>
#include <windows.h>
#include <windowsx.h>

#define THREAD_ID "WindowsFrame"

namespace GuiManager {

WindowsFrame::WindowsFrame(MainWindow *window)
    : _window(window), _margins(0, 0, 0, 0), _frames(0, 0, 0, 0),
      _justMaximized(false) {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

void WindowsFrame::init() {
  LOG_PERF(0, "Enter");

  if (!_window) {
    LOG_PERF(0, "Exit");
    return;
  }

  _window->setWindowFlags(_window->windowFlags() |
                          Qt::WindowMaximizeButtonHint);

  HWND hwnd = (HWND)_window->winId();
  DWORD style = ::GetWindowLong(hwnd, GWL_STYLE);
  ::SetWindowLong(hwnd, GWL_STYLE,
                  style | WS_MAXIMIZEBOX | WS_THICKFRAME | WS_CAPTION);

  const MARGINS shadow = {1, 1, 1, 1};
  DwmExtendFrameIntoClientArea(hwnd, &shadow);

  LOG_PERF(0, "Exit");
}

bool WindowsFrame::nativeEvent(const QByteArray &eventType, void *message,
                               qintptr *result) {
  if (!_window)
    return false;

  MSG *msg = reinterpret_cast<MSG *>(message);

  switch (msg->message) {
  case WM_NCCALCSIZE: {
    NCCALCSIZE_PARAMS &params =
        *reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam);
    if (params.rgrc[0].top != 0)
      params.rgrc[0].top -= 1;

    // this kills the window frame and title bar we added with WS_THICKFRAME and
    // WS_CAPTION
    *result = WVR_REDRAW;
    return true;
  }
  case WM_NCHITTEST: {
    *result = 0;

    const LONG border_width = 5;
    RECT winrect;
    ::GetWindowRect(HWND(_window->winId()), &winrect);

    long x = GET_X_LPARAM(msg->lParam);
    long y = GET_Y_LPARAM(msg->lParam);

    bool resizeWidth = _window->minimumWidth() != _window->maximumWidth();
    bool resizeHeight = _window->minimumHeight() != _window->maximumHeight();

    if (resizeWidth) {
      // left border
      if (x >= winrect.left && x < winrect.left + border_width) {
        *result = HTLEFT;
      }
      // right border
      if (x < winrect.right && x >= winrect.right - border_width) {
        *result = HTRIGHT;
      }
    }
    if (resizeHeight) {
      // bottom border
      if (y < winrect.bottom && y >= winrect.bottom - border_width) {
        *result = HTBOTTOM;
      }
      // top border
      if (y >= winrect.top && y < winrect.top + border_width) {
        *result = HTTOP;
      }
    }
    if (resizeWidth && resizeHeight) {
      // bottom left corner
      if (x >= winrect.left && x < winrect.left + border_width &&
          y < winrect.bottom && y >= winrect.bottom - border_width) {
        *result = HTBOTTOMLEFT;
      }
      // bottom right corner
      if (x < winrect.right && x >= winrect.right - border_width &&
          y < winrect.bottom && y >= winrect.bottom - border_width) {
        *result = HTBOTTOMRIGHT;
      }
      // top left corner
      if (x >= winrect.left && x < winrect.left + border_width &&
          y >= winrect.top && y < winrect.top + border_width) {
        *result = HTTOPLEFT;
      }
      // top right corner
      if (x < winrect.right && x >= winrect.right - border_width &&
          y >= winrect.top && y < winrect.top + border_width) {
        *result = HTTOPRIGHT;
      }
    }
    if (0 != *result)
      return true;

    // support highdpi
    double dpr = _window->devicePixelRatioF();
    QPoint pos =
        _window->topBarWidget()->mapFromGlobal(QPoint(x / dpr, y / dpr));

    if (!_window->topBarWidget()->rect().contains(pos))
      return false;

    QWidget *child = _window->topBarWidget()->childAt(pos);
    if (!child) {
      *result = HTCAPTION;
      return true;
    }

    return false;
  }
  case WM_GETMINMAXINFO: {
    if (::IsZoomed(msg->hwnd)) {
      RECT frame = {0, 0, 0, 0};
      ::AdjustWindowRectEx(&frame, WS_OVERLAPPEDWINDOW, FALSE, 0);

      // record frame area data
      double dpr = _window->devicePixelRatioF();

      _frames.setLeft(abs(frame.left) / dpr + 0.5);
      _frames.setTop(abs(frame.bottom) / dpr + 0.5);
      _frames.setRight(abs(frame.right) / dpr + 0.5);
      _frames.setBottom(abs(frame.bottom) / dpr + 0.5);

      _window->setContentsMargins(_frames.left() + _margins.left(),
                                  _frames.top() + _margins.top(),
                                  _frames.right() + _margins.right(),
                                  _frames.bottom() + _margins.bottom());
      _justMaximized = true;
    } else {
      if (_justMaximized) {
        _window->setContentsMargins(_margins);
        _frames = QMargins();
        _justMaximized = false;
      }
    }
    return false;
  }
  default:
    return false;
  }
}

} // namespace GuiManager
