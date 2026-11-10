// MIT License
// Copyright (c) 2026 Sourcerer

#include "widgets/WorkspacesTableView.h"
#include "MousePositionCorrection.h"

#include <Log.h>

#include <QHeaderView>

#define THREAD_ID "WorkspacesTableView"

namespace GuiManager {

namespace Widgets {

WorkspacesTableView::WorkspacesTableView(QWidget *parent) : QTableView{parent} {
  LOG_PERF(0, "Enter");

  horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  viewport()->setMouseTracking(true);
  viewport()->installEventFilter(this);

  LOG_PERF(0, "Exit");
}

bool WorkspacesTableView::eventFilter(QObject *watched, QEvent *event) {
  if (!event || viewport() != watched)
    return false;

  switch (event->type()) {
  case QEvent::MouseMove: {
    mouseMove(static_cast<QMouseEvent *>(event));
    break;
  }
  case QEvent::Leave: {
    leave(event);
    break;
  }
  default: {
    break;
  }
  }

  return QTableView::eventFilter(watched, event);
}

void WorkspacesTableView::mouseMoveEvent(QMouseEvent *event) {
  const QPoint globalPos =
      MousePositionCorrection::instance().correctedGlobalPosition(event);
  const QPoint pos = viewport()->mapFromGlobal(globalPos);

  if (const QModelIndex index = indexAt(pos); index.isValid()) {
    setCursor(Qt::PointingHandCursor);
  } else {
    unsetCursor();
  }

  QTableView::mouseMoveEvent(event);
}

bool WorkspacesTableView::leave(QEvent *event) {
  LOG_PERF(0, "Enter");

  emit hoverLeave(_prev);
  _prev = QModelIndex();

  LOG_PERF(0, "Exit");
  return true;
}

bool WorkspacesTableView::mouseMove(const QMouseEvent *event) {
  if (!event)
    return false;

  const QPoint globalPos =
      MousePositionCorrection::instance().correctedGlobalPosition(event);
  const QPoint pos = viewport()->mapFromGlobal(globalPos);

  if (const auto index = indexAt(pos); index.row() == -1) {
    emit hoverLeave(_prev);
    _prev = QModelIndex();
  } else if (index != _prev) {
    _prev = index;
    emit hoverEnter(index);
  }

  return true;
}

} // namespace Widgets
} // namespace GuiManager
