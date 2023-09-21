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

  _pixelRatio = _window->devicePixelRatioF();

  LOG_PERF(0, "Exit");
}

bool LinuxFrame::eventFilter(QObject *watched, QEvent *event) {
  if (watched != _window)
    return false;

  switch (event->type()) {
  case QEvent::MouseButtonPress: {
    QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() == Qt::LeftButton)
      emit edgeHit(_lastEdges);

    break;
  }
  case QEvent::MouseMove: {
    const QPoint currentPosition = QCursor::pos();
    const LinuxFrame::Margin currentMargin = ncHitTest(
        _window->frameGeometry(), qFloor(currentPosition.x() * _pixelRatio),
        qFloor(currentPosition.y() * _pixelRatio));

    switch (currentMargin) {
    case LinuxFrame::Margin::HTBOTTOM: {
      _window->setCursor(Qt::SizeVerCursor);
      _lastEdges = Qt::BottomEdge;
      break;
    }
    case LinuxFrame::Margin::HTRIGHT: {
      _window->setCursor(Qt::SizeHorCursor);
      _lastEdges = Qt::RightEdge;
      break;
    }
    case LinuxFrame::Margin::HTBOTTOMRIGHT: {
      _window->setCursor(Qt::SizeFDiagCursor);
      _lastEdges = Qt::BottomEdge | Qt::RightEdge;
      break;
    }
    case LinuxFrame::Margin::HTBOTTOMLEFT: {
      _window->setCursor(Qt::SizeBDiagCursor);
      _lastEdges = Qt::BottomEdge | Qt::LeftEdge;
      break;
    }
    case LinuxFrame::Margin::HTLEFT: {
      _window->setCursor(Qt::SizeHorCursor);
      _lastEdges = Qt::LeftEdge;
      break;
    }
    case LinuxFrame::Margin::HTTOPLEFT: {
      _window->setCursor(Qt::SizeFDiagCursor);
      _lastEdges = Qt::TopEdge | Qt::LeftEdge;
      break;
    }
    case LinuxFrame::Margin::HTTOP: {
      _window->setCursor(Qt::SizeVerCursor);
      _lastEdges = Qt::TopEdge;
      break;
    }
    case LinuxFrame::Margin::HTTOPRIGHT: {
      _window->setCursor(Qt::SizeBDiagCursor);
      _lastEdges = Qt::TopEdge | Qt::RightEdge;
      break;
    }
    case LinuxFrame::Margin::HTNOWHERE: {
      _window->unsetCursor();
      break;
    }
    default: {
      break;
    }
    }

    break;
  }
  default: {
    break;
  }
  }

  return false;
}

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
