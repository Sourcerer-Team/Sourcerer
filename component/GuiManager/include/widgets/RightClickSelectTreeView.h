#pragma once

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
    if (event->button() == Qt::RightButton) {
      QModelIndex index = indexAt(event->pos());
      if (index.isValid()) {
        selectionModel()->select(index, QItemSelectionModel::Select);
      }
    }

    QTreeView::mousePressEvent(event);
  }
};

} // namespace GuiManager::Widgets
