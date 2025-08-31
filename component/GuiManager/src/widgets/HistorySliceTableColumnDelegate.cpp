#include "widgets/HistorySliceTableColumnDelegate.h"

namespace GuiManager {

namespace Widgets {

HistorySliceTableColumnDelegate::HistorySliceTableColumnDelegate(
    QObject *parent)
    : QStyledItemDelegate(parent) {}

void HistorySliceTableColumnDelegate::paint(QPainter *painter,
                                            const QStyleOptionViewItem &option,
                                            const QModelIndex &index) const {
  QStyledItemDelegate::paint(painter, option, index);
}
} // namespace Widgets
} // namespace GuiManager