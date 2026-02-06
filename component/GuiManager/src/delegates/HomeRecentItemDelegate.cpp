#include "delegates/HomeRecentItemDelegate.h"

#include <Log.h>

#include <QTableView>
#include <QPainter>

#define THREAD_ID "HomeRecentItemDelegate"

namespace GuiManager::Delegates {

HomeRecentItemDelegate::HomeRecentItemDelegate(QObject *parent)
    : QStyledItemDelegate{parent} {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

void HomeRecentItemDelegate::setHover(const QModelIndex &index) {
  _indexHovered = index;
}

void HomeRecentItemDelegate::paint(QPainter *painter,
                                   const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const {
  QStyleOptionViewItem _option = option;

  if (_indexHovered == index) {
    // Draw hover background color across the entire row
    QColor hoverBackground(66, 66, 66);  // Slightly lighter gray from dark theme
    QRect fullRowRect(0, option.rect.top(), 10000, option.rect.height());
    painter->fillRect(fullRowRect, hoverBackground);
  }

  QStyledItemDelegate::paint(painter, _option, index);
}

} // namespace GuiManager::Delegates
