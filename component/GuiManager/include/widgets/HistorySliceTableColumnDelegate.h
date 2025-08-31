#pragma once

#include <QPainter>
#include <QStyledItemDelegate>

namespace GuiManager {

namespace Widgets {

class HistorySliceTableColumnDelegate : public QStyledItemDelegate {
public:
  HistorySliceTableColumnDelegate(QObject *parent = nullptr);
  ~HistorySliceTableColumnDelegate() override = default;

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;
};

}
}
