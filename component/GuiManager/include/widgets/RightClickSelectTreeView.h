#pragma once

#include <QItemSelectionModel>
#include <QMouseEvent>
#include <QTreeView>

namespace GuiManager::Widgets {

class RightClickSelectTreeView : public QTreeView {
  Q_OBJECT
public:
  explicit RightClickSelectTreeView(QWidget *parent = nullptr)
      : QTreeView(parent) {}
  ~RightClickSelectTreeView() override = default;

  // QWidget interface
protected:
  void mousePressEvent(QMouseEvent *event) override {
      if (event->button() == Qt::LeftButton) {
        if (const QModelIndex index = indexAt(event->pos()); index.isValid()) {
          selectionModel()->clearSelection();
          selectionModel()->select(index, QItemSelectionModel::Select);
        }
        // Accept the event for right-click to prevent parent class from
        // deselecting the item when showing context menu
        event->accept();
        QTreeView::mousePressEvent(event);
        return;
      } else if (event->button() == Qt::RightButton) {
        if (const QModelIndex index = indexAt(event->pos()); index.isValid()) {
          selectionModel()->clearSelection();
          selectionModel()->select(index, QItemSelectionModel::Select);
        }
        // Accept the event for right-click to prevent parent class from
        // deselecting the item when showing context menu
        event->accept();
        return;
      }

    if (const QModelIndex index = indexAt(event->pos()); index.isValid()) {
      selectionModel()->clearSelection();
      selectionModel()->select(index,
                               QItemSelectionModel::ClearAndSelect |
                                   QItemSelectionModel::Current);
      setCurrentIndex(index);
    }

    QTreeView::mousePressEvent(event);
  }
};

} // namespace GuiManager::Widgets
