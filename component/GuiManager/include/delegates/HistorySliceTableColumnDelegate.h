#pragma once

#include "models/GitHistoryModel.h"

#include <QPainter>
#include <QStyledItemDelegate>

namespace GuiManager::Delegates {

// Width of one graph lane inside a graph cell. Lanes are as wide as the row is
// tall, unless that would not fit `laneCount` lanes into the cell.
int historyLaneWidth(const QRect &rect, int laneCount);

class HistorySliceTableColumnDelegate final : public QStyledItemDelegate {

  Q_OBJECT

public:
  explicit HistorySliceTableColumnDelegate(QObject *parent = nullptr);
  ~HistorySliceTableColumnDelegate() override = default;

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;

private:
  void paintDefault(QPainter *painter, const QStyleOptionViewItem &option,
                    const QModelIndex &index) const;

  void paintSlices(QPainter *painter, const QStyleOptionViewItem &option,
                   const QModelIndex &index) const;

  void paintStatus(QPainter *painter, const QStyleOptionViewItem &option,
                   const QModelIndex &index) const;
};

} // namespace GuiManager::Delegates
