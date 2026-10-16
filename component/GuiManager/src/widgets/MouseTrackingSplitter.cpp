#include "widgets/MouseTrackingSplitter.h"

#include <QMainWindow>
#include <QMouseEvent>

namespace GuiManager {

namespace Widgets {

MouseTrackingSplitterHandle::MouseTrackingSplitterHandle(
    Qt::Orientation orientation, QSplitter *parent)
    : QSplitterHandle(orientation, parent) {
  setMouseTracking(true);
}

void MouseTrackingSplitterHandle::mouseMoveEvent(QMouseEvent *event) {

  qDebug() << "Emitting mousePositionChanged with x:" << event->pos().x()
           << "y:" << event->pos().y();

  emit mousePositionChanged(event->pos().x(), event->pos().y());
  QSplitterHandle::mouseMoveEvent(event);
}

MouseTrackingSplitter::MouseTrackingSplitter(QWidget *parent)
    : QSplitter(parent) {}

QSplitterHandle *MouseTrackingSplitter::createHandle() {
  return new MouseTrackingSplitterHandle(orientation(), this);
}

} // namespace Widgets

} // namespace GuiManager
