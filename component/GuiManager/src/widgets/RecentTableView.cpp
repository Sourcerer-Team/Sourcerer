#include "widgets/RecentTableView.h"

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
  if (const QModelIndex index = indexAt(event->pos()); index.isValid()) {
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

  const auto pos = viewport()->mapFromGlobal(event->globalPosition());

  if (const auto index = indexAt(pos.toPoint()); index.row() == -1) {
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
