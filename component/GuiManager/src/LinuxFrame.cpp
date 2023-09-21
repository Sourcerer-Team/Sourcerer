#include "LinuxFrame.h"

#include <Log.h>

#include <QApplication>
#include <QtGui/private/qtx11extras_p.h>

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <xcb/xcb.h>

#include <iostream>

#define THREAD_ID "LinuxFrame"

#define FIXED_WIDTH(gw) (gw->minimumWidth() >= gw->maximumWidth())
#define FIXED_HEIGHT(gw) (gw->minimumHeight() >= gw->maximumHeight())
#define FIXED_SIZE(gw) (FIXED_WIDTH(gw) && FIXED_HEIGHT(gw))

namespace GuiManager {

LinuxFrame::LinuxFrame(MainWindow *window)
    : _window(window), _pixelRatio(1.0),
      _lastClickMargin(LinuxFrame::Margin::Unknown),
      _configManager(ConfigManager::ConfigManager::getInstance()) {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

void LinuxFrame::init() {
  LOG_PERF(0, "Enter");

  if (!_window) {
    LOG_PERF(0, "Exit");
    return;
  }

  _lastClickMargin = LinuxFrame::Margin::Unknown;
  _pixelRatio = _window->devicePixelRatioF();

  LOG_PERF(0, "Exit");
}

bool LinuxFrame::eventFilter(QObject *watched, QEvent *event) {
  // if (watched != _window)
  //   return false;

  switch (event->type()) {
  case QEvent::MouseButtonPress: {
    QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() == Qt::LeftButton) {
      const QPoint currentPosition = QCursor::pos();
      _lastClickMargin = ncHitTest(_window->frameGeometry(),
                                   qFloor(currentPosition.x() * _pixelRatio),
                                   qFloor(currentPosition.y() * _pixelRatio));
    }

    break;
  }
  case QEvent::MouseButtonRelease: {
    QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() == Qt::LeftButton)
      _lastClickMargin = LinuxFrame::Margin::Unknown;

    break;
  }
  case QEvent::MouseMove: {
    QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
    const QPoint currentPosition = QCursor::pos();

    if (_previousPosition.isNull()) {
      _previousPosition = currentPosition;
      break;
    }

    setCursorForCurrentPosition();

    if (_lastClickMargin != LinuxFrame::Margin::Unknown) {
      const int xOffset = currentPosition.x() - _previousPosition.x();
      const int yOffset = currentPosition.y() - _previousPosition.y();
      const QRect windowRect = _window->frameGeometry();

      switch (_lastClickMargin) {
      case LinuxFrame::Margin::HTBOTTOM: {
        const QRect newRect(windowRect.x(), windowRect.y(), windowRect.width(),
                            windowRect.height() + yOffset);
        const QRect availableRect = _window->screen()->availableGeometry();
        if (!availableRect.contains(newRect))
          break;

        const QSize minimumSize = _window->minimumSize();
        const QSize newRectSize = newRect.size();
        if (newRectSize.width() > minimumSize.width() &&
            newRectSize.height() > minimumSize.height())
          _window->setGeometry(newRect);
        break;
      }
      case LinuxFrame::Margin::HTRIGHT: {
        const QRect newRect(windowRect.x(), windowRect.y(), windowRect.width() + xOffset,
                            windowRect.height());
        const QRect availableRect = _window->screen()->availableGeometry();
        if (!availableRect.contains(newRect))
          break;

        const QSize minimumSize = _window->minimumSize();
        const QSize newRectSize = newRect.size();
        if (newRectSize.width() > minimumSize.width() &&
            newRectSize.height() > minimumSize.height())
          _window->setGeometry(newRect);
        break;
        break;
      }
      case LinuxFrame::Margin::HTBOTTOMRIGHT: {
        const QRect newRect(windowRect.x(), windowRect.y(),
                            windowRect.width() + xOffset,
                            windowRect.height() + yOffset);
        const QRect availableRect = _window->screen()->availableGeometry();
        if (!availableRect.contains(newRect))
          break;

        const QSize minimumSize = _window->minimumSize();
        const QSize newRectSize = newRect.size();
        if (newRectSize.width() > minimumSize.width() &&
            newRectSize.height() > minimumSize.height())
          _window->setGeometry(newRect);
        break;
      }
      case LinuxFrame::Margin::HTBOTTOMLEFT: {
        const QRect newRect(windowRect.x() + xOffset, windowRect.y(),
                            windowRect.width() - xOffset,
                            windowRect.height() + yOffset);
        const QRect availableRect = _window->screen()->availableGeometry();
        if (!availableRect.contains(newRect))
          break;

        const QSize minimumSize = _window->minimumSize();
        const QSize newRectSize = newRect.size();
        if (newRectSize.width() > minimumSize.width() &&
            newRectSize.height() > minimumSize.height())
          _window->setGeometry(newRect);
        break;
      }
      case LinuxFrame::Margin::HTLEFT: {
        const QRect newRect(windowRect.x() + xOffset, windowRect.y(),
                            windowRect.width() - xOffset, windowRect.height());
        const QRect availableRect = _window->screen()->availableGeometry();
        if (!availableRect.contains(newRect))
          break;

        const QSize minimumSize = _window->minimumSize();
        const QSize newRectSize = newRect.size();
        if (newRectSize.width() > minimumSize.width())
          _window->setGeometry(newRect);
        break;
      }
      case LinuxFrame::Margin::HTTOPLEFT: {
        const QRect newRect(windowRect.x() + xOffset, windowRect.y() + yOffset,
                            windowRect.width() - xOffset,
                            windowRect.height() - yOffset);
        const QRect availableRect = _window->screen()->availableGeometry();
        if (!availableRect.contains(newRect))
          break;

        const QSize minimumSize = _window->minimumSize();
        const QSize newRectSize = newRect.size();
        if (newRectSize.width() > minimumSize.width() &&
            newRect.height() > minimumSize.height())
          _window->setGeometry(newRect);
        break;
      }
      case LinuxFrame::Margin::HTTOP: {
        const QRect newRect(windowRect.x(), windowRect.y() + yOffset,
                            windowRect.width(), windowRect.height() - yOffset);
        const QRect availableRect = _window->screen()->availableGeometry();
        if (!availableRect.contains(newRect))
          break;

        const QSize minimumSize = _window->minimumSize();
        const QSize newRectSize = newRect.size();
        if (newRectSize.height() > minimumSize.height())
          _window->setGeometry(newRect);
        break;
      }
      case LinuxFrame::Margin::HTTOPRIGHT: {
        const QRect newRect(windowRect.x(), windowRect.y() + yOffset,
                            windowRect.width() + xOffset,
                            windowRect.height() - yOffset);
        const QRect availableRect = _window->screen()->availableGeometry();
        if (!availableRect.contains(newRect))
          break;

        const QSize minimumSize = _window->minimumSize();
        const QSize newRectSize = newRect.size();
        if (newRectSize.width() > minimumSize.width() &&
            newRect.height() > minimumSize.height())
          _window->setGeometry(newRect);
        break;
      }
      default: {
        break;
      }
      }
    }

    _previousPosition = currentPosition;

    break;
  }
  default: {
    break;
  }
  }

  return false;
}

bool LinuxFrame::isX11() const { return true; }

bool LinuxFrame::isWayland() const { return false; }

void LinuxFrame::setCursorForCurrentPosition() {
  if (isX11())
    setCursorForCurrentPositionX11();
  else if (isWayland())
    setCursorForCurrentPositionWayland();
}

void LinuxFrame::setCursorForCurrentPositionX11() {
  const QPoint cursorPos = QCursor::pos();
  const LinuxFrame::Margin margin =
      ncHitTest(_window->frameGeometry(), qFloor(cursorPos.x() * _pixelRatio),
                qFloor(cursorPos.y() * _pixelRatio));

  QWidget *widget = QApplication::widgetAt(cursorPos);

  if (!widget)
    return;

  Display *dpy = QX11Info::display();

  switch (margin) {
  case LinuxFrame::Margin::HTTOPLEFT: {
    Cursor cursor;

    if (!FIXED_SIZE(_window))
      cursor = XCreateFontCursor(dpy, XC_top_left_corner);
    else
      cursor = XCreateFontCursor(dpy, XC_arrow);

    XDefineCursor(dpy, Window(widget->winId()), cursor);

    XFlush(dpy);

    XFreeCursor(dpy, cursor);

    break;
  }
  case LinuxFrame::Margin::HTTOP: {
    Cursor cursor;

    if (!FIXED_HEIGHT(_window))
      cursor = XCreateFontCursor(dpy, XC_top_side);
    else
      cursor = XCreateFontCursor(dpy, XC_arrow);

    XDefineCursor(dpy, Window(widget->winId()), cursor);

    XFlush(dpy);

    XFreeCursor(dpy, cursor);

    break;
  }
  case LinuxFrame::Margin::HTTOPRIGHT: {
    Cursor cursor;

    if (!FIXED_SIZE(_window))
      cursor = XCreateFontCursor(dpy, XC_top_right_corner);
    else
      cursor = XCreateFontCursor(dpy, XC_arrow);

    XDefineCursor(dpy, Window(widget->winId()), cursor);

    XFlush(dpy);

    XFreeCursor(dpy, cursor);

    break;
  }
  case LinuxFrame::Margin::HTLEFT: {
    Cursor cursor;

    if (!FIXED_WIDTH(_window))
      cursor = XCreateFontCursor(dpy, XC_left_side);
    else
      cursor = XCreateFontCursor(dpy, XC_arrow);

    XDefineCursor(dpy, Window(widget->winId()), cursor);

    XFlush(dpy);

    XFreeCursor(dpy, cursor);

    break;
  }
  case LinuxFrame::Margin::HTRIGHT: {
    Cursor cursor;

    if (!FIXED_WIDTH(_window))
      cursor = XCreateFontCursor(dpy, XC_right_side);
    else
      cursor = XCreateFontCursor(dpy, XC_arrow);

    XDefineCursor(dpy, Window(widget->winId()), cursor);

    XFlush(dpy);

    XFreeCursor(dpy, cursor);

    break;
  }
  case LinuxFrame::Margin::HTBOTTOMLEFT: {
    Cursor cursor;

    if (!FIXED_SIZE(_window))
      cursor = XCreateFontCursor(dpy, XC_bottom_left_corner);
    else
      cursor = XCreateFontCursor(dpy, XC_arrow);

    XDefineCursor(dpy, Window(widget->winId()), cursor);

    XFlush(dpy);

    XFreeCursor(dpy, cursor);

    break;
  }
  case LinuxFrame::Margin::HTBOTTOM: {
    Cursor cursor;

    if (!FIXED_HEIGHT(_window))
      cursor = XCreateFontCursor(dpy, XC_bottom_side);
    else
      cursor = XCreateFontCursor(dpy, XC_arrow);

    XDefineCursor(dpy, Window(widget->winId()), cursor);

    XFlush(dpy);

    XFreeCursor(dpy, cursor);

    break;
  }
  case LinuxFrame::Margin::HTBOTTOMRIGHT: {
    Cursor cursor;

    if (!FIXED_SIZE(_window))
      cursor = XCreateFontCursor(dpy, XC_bottom_right_corner);
    else
      cursor = XCreateFontCursor(dpy, XC_arrow);

    XDefineCursor(dpy, Window(widget->winId()), cursor);

    XFlush(dpy);

    XFreeCursor(dpy, cursor);

    break;
  }
  case LinuxFrame::Margin::HTNOWHERE: {
    Cursor cursor;

    cursor = XCreateFontCursor(dpy, XC_arrow);

    XDefineCursor(dpy, Window(widget->winId()), cursor);

    XFlush(dpy);

    XFreeCursor(dpy, cursor);

    break;
  }
  default:
    break;
  }
}

void LinuxFrame::setCursorForCurrentPositionWayland() {}

LinuxFrame::Margin LinuxFrame::ncHitTest(const QRect &frameGeometry, int x,
                                         int y) {
  if (!_window)
    return Margin::HTNOWHERE;

  const qlonglong borderWidth =
      _configManager.getLong(ConfigManager::ConfigKey::MainWindowBorderWidth);

  LOG_DEBUG("Border width is {} pixels.", borderWidth);

  // Get the point coordinates for the hit test.
  const QPoint cursorPos =
      QPoint(qFloor(x / _pixelRatio), qFloor(y / _pixelRatio));

  // Determine if the hit test is for resizing. Default middle (1,1).
  int row = 1;
  int col = 1;

  // Determine if the point is at the top or bottom of the window.
  if (cursorPos.y() < frameGeometry.top() + borderWidth) {
    row = 0; // top border.
  } else if (cursorPos.y() > frameGeometry.bottom() - borderWidth)
    row = 2; // bottom border.

  // Determine if the point is at the left or right of the window.
  if (cursorPos.x() < frameGeometry.left() + borderWidth)
    col = 0; // left border.
  else if (cursorPos.x() > frameGeometry.right() - borderWidth)
    col = 2; // right border.

  // Hit test (HTTOPLEFT, ... HTBOTTOMRIGHT)
  const LinuxFrame::Margin hitTests[3][3] = {
      {LinuxFrame::Margin::HTTOPLEFT, LinuxFrame::Margin::HTTOP,
       LinuxFrame::Margin::HTTOPRIGHT},
      {LinuxFrame::Margin::HTLEFT, LinuxFrame::Margin::HTNOWHERE,
       LinuxFrame::Margin::HTRIGHT},
      {LinuxFrame::Margin::HTBOTTOMLEFT, LinuxFrame::Margin::HTBOTTOM,
       LinuxFrame::Margin::HTBOTTOMRIGHT},
  };

  return hitTests[row][col];
}
} // namespace GuiManager
