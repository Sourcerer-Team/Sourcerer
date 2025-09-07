#pragma once

#include <QHeaderView>
#include <QMouseEvent>

namespace GuiManager::Widgets {
class ClickableHeaderView : public QHeaderView {
  Q_OBJECT
public:
  explicit ClickableHeaderView(Qt::Orientation orientation,
                               QWidget *parent = nullptr)
      : QHeaderView(orientation, parent) {}

signals:
  void headerDoubleClicked(int section);

protected:
  void mouseDoubleClickEvent(QMouseEvent *event) override {
    int section = logicalIndexAt(event->pos());
    if (section >= 0)
      emit headerDoubleClicked(section);
    QHeaderView::mouseDoubleClickEvent(event);
  }
};

} // namespace GuiManager::Widgets
