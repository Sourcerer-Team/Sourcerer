#pragma once

#include "models/GitHistoryModel.h"

#include <QPainter>
#include <QStyledItemDelegate>

namespace GuiManager::Widgets {

class HistorySliceTableColumnDelegate final : public QStyledItemDelegate {
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

} // namespace GuiManager::Widgets

