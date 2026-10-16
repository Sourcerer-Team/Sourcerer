#pragma once

#include <QHoverEvent>
#include <QModelIndex>
#include <QTableView>
#include <QWidget>

namespace GuiManager {

namespace Widgets {

class WorkspacesTableView : public QTableView {
  Q_OBJECT
public:
  explicit WorkspacesTableView(QWidget *parent = nullptr);

  // QObject interface
public:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;

signals:
  void hoverEnter(const QModelIndex &index);
  void hoverLeave(const QModelIndex &index);

private:
  bool leave(QEvent *event);
  bool mouseMove(const QMouseEvent *event);

private:
  QModelIndex _prev;
};

} // namespace Widgets
} // namespace GuiManager
