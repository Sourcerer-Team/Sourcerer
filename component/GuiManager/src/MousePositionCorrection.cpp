#include "MousePositionCorrection.h"

#include <Log.h>

#include <QApplication>
#include <QCursor>
#include <QEvent>
#include <QFile>
#include <QMouseEvent>
#include <QString>
#include <QtGlobal>

#define THREAD_ID "MousePositionCorrection"

namespace GuiManager {

MousePositionCorrection &MousePositionCorrection::instance() {
  static MousePositionCorrection correction;
  return correction;
}

MousePositionCorrection::MousePositionCorrection(QObject *parent)
    : QObject(parent), _enabled(isRunningUnderWsl2()), _initialized(false),
      _offset(0.0, 0.0) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("WSL2 mouse correction enabled: {}", _enabled);
  LOG_PERF(0, "Exit");
}

void MousePositionCorrection::install(QApplication &app) {
  LOG_PERF(0, "Enter");

  if (_enabled) {
    app.installEventFilter(this);
    LOG_DEBUG("Installed global mouse position correction event filter");
  }

  LOG_PERF(0, "Exit");
}

bool MousePositionCorrection::eventFilter(QObject *watched, QEvent *event) {
  (void)watched;

  if (!_enabled || !event) {
    return false;
  }

  switch (event->type()) {
  case QEvent::MouseMove:
  case QEvent::MouseButtonPress:
  case QEvent::MouseButtonRelease:
  case QEvent::MouseButtonDblClick:
    updateOffset(static_cast<QMouseEvent *>(event));
    break;
  default:
    break;
  }

  return false;
}

QPoint MousePositionCorrection::correctedGlobalPosition(
    const QMouseEvent *event) const {
  if (!event) {
    return {};
  }

  return correctedGlobalPosition(event->globalPosition()).toPoint();
}

QPointF MousePositionCorrection::correctedGlobalPosition(
    const QPointF &globalPosition) const {
  if (!_enabled || !_initialized) {
    return globalPosition;
  }

  return globalPosition + _offset;
}

void MousePositionCorrection::updateOffset(const QMouseEvent *event) {
  if (!event) {
    return;
  }

  const QPointF cursorPosition = QCursor::pos();
  const QPointF reportedPosition = event->globalPosition();
  const QPointF newOffset = cursorPosition - reportedPosition;

  if (!_initialized || qAbs(newOffset.x() - _offset.x()) > 0.5 ||
      qAbs(newOffset.y() - _offset.y()) > 0.5) {
    _offset = newOffset;
    _initialized = true;
    LOG_DEBUG("Updated WSL2 mouse offset to ({}, {})", _offset.x(),
              _offset.y());
  }
}

bool MousePositionCorrection::isRunningUnderWsl2() {
#ifdef Q_OS_LINUX
  const auto containsMicrosoft = [](const QString &text) {
    return text.contains(QStringLiteral("microsoft"), Qt::CaseInsensitive);
  };

  if (qEnvironmentVariableIsSet("WSL_INTEROP") ||
      qEnvironmentVariableIsSet("WSL_DISTRO_NAME")) {
    return true;
  }

  for (const char *path : {"/proc/sys/kernel/osrelease", "/proc/version"}) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      continue;
    }

    if (containsMicrosoft(QString::fromUtf8(file.readAll()))) {
      return true;
    }
  }
#endif

  return false;
}

} // namespace GuiManager
