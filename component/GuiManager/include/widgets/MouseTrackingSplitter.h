#pragma once

#include <QSplitter>

namespace GuiManager {

namespace Widgets {

class MouseTrackingSplitterHandle : public QSplitterHandle {
  Q_OBJECT
 public:
  MouseTrackingSplitterHandle(Qt::Orientation orientation, QSplitter *parent);
  ~MouseTrackingSplitterHandle() override = default;

signals:
  void mousePositionChanged(int x, int y);

protected:
  void mouseMoveEvent(QMouseEvent *event) override;
};

class MouseTrackingSplitter : public QSplitter {
  Q_OBJECT
public:
  explicit MouseTrackingSplitter(QWidget *parent = nullptr);
  ~MouseTrackingSplitter() override = default;

protected:
  QSplitterHandle *createHandle() override;
};

}
}