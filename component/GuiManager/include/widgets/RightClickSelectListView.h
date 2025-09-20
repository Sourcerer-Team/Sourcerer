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
      QModelIndex index = indexAt(event->pos());
      if (index.isValid()) {
        selectionModel()->select(index, QItemSelectionModel::Select);
      }
    }

    QListView::mousePressEvent(event);
  }
};

} // namespace GuiManager::Widgets
