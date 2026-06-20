#include "widgets/RecentTableView.h"
#include "MousePositionCorrection.h"

#include <Log.h>

#include <QHeaderView>

#define THREAD_ID "RecentTableView"

namespace GuiManager {

namespace Widgets {

RecentTableView::RecentTableView(QWidget *parent) : QTableView{parent} {
  LOG_PERF(0, "Enter");

  horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  viewport()->setMouseTracking(true);
  viewport()->installEventFilter(this);

  LOG_PERF(0, "Exit");
}

bool RecentTableView::eventFilter(QObject *watched, QEvent *event) {
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

void RecentTableView::mouseMoveEvent(QMouseEvent *event) {
  const QPoint globalPos =
      MousePositionCorrection::instance().correctedGlobalPosition(event);
  const QPoint pos = viewport()->mapFromGlobal(globalPos);

  if (const QModelIndex index = indexAt(pos); index.isValid()) {
    setCursor(Qt::PointingHandCursor); // Or any other shape
  } else {
    unsetCursor(); // Restore default
  }

  QTableView::mouseMoveEvent(event);
}

bool RecentTableView::leave(QEvent *event) {
  LOG_PERF(0, "Enter");

  emit hoverLeave(_prev);
  _prev = QModelIndex();

  LOG_PERF(0, "Exit");
  return true;
}

bool RecentTableView::mouseMove(const QMouseEvent *event) {
  if (!event)
    return false;

  const QPoint globalPos =
      MousePositionCorrection::instance().correctedGlobalPosition(event);
  const QPoint pos = viewport()->mapFromGlobal(globalPos);

  if (const auto index = indexAt(pos); index.row() == -1) {
    emit hoverLeave(_prev);
    _prev = QModelIndex();
    LOG_DEBUG("emitted hoveLeave");
  } else if (index != _prev) {
    _prev = index;
    emit hoverEnter(index);
    LOG_DEBUG("emitted hoveEnter");
  }

  return true;
}

} // namespace Widgets
} // namespace GuiManager
