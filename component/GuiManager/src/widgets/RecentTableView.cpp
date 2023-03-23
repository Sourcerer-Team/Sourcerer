#include "widgets/RecentTableView.h"

#include <QHeaderView>

#include <iostream>

namespace GuiManager {

namespace Widgets {

RecentTableView::RecentTableView(QWidget *parent) : QTableView{parent} {
  horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  viewport()->setMouseTracking(true);
  viewport()->installEventFilter(this);
}

bool RecentTableView::eventFilter(QObject *watched, QEvent *event) {
  if (!event || viewport() != watched)
    return false;

  switch (event->type()) {
  case QEvent::MouseMove: {
    return mouseMove(static_cast<QMouseEvent *>(event));
  }
  case QEvent::Leave: {
    return leave(event);
  }
  default: {
    break;
  }
  }

  return QTableView::eventFilter(watched, event);
}

bool RecentTableView::leave(QEvent *event) {
  emit hoverLeave(_prev);
  _prev = QModelIndex();
  return true;
}

bool RecentTableView::mouseMove(QMouseEvent *event) {
  if (!event)
    return false;

  const auto pos = viewport()->mapFromGlobal(event->globalPosition());
  const auto index = indexAt(pos.toPoint());

  if (index.row() == -1 || index.column() != 0) {
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
