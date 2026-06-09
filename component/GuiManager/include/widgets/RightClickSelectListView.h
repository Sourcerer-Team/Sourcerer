#pragma once

#include <QListView>
#include <QMouseEvent>

namespace GuiManager::Widgets {

class RightClickSelectListView : public QListView {
  Q_OBJECT
public:
  explicit RightClickSelectListView(QWidget *parent = nullptr)
      : QListView(parent) {}
  ~RightClickSelectListView() override = default;

  // QWidget interface
protected:
  void mousePressEvent(QMouseEvent *event) override {
    if (event->button() == Qt::RightButton) {
      if (const QModelIndex index = indexAt(event->pos()); index.isValid()) {
        selectionModel()->select(index, QItemSelectionModel::Select);
      }
      // Accept the event for right-click to prevent parent class from
      // deselecting the item when showing context menu
      event->accept();
      return;
    }

    QListView::mousePressEvent(event);
  }
};

} // namespace GuiManager::Widgets
