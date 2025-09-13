#include "delegates/HomeRecentItemDelegate.h"

#include <Log.h>

#include <QTableView>

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
    _option.font.setUnderline(true);
  }

  QStyledItemDelegate::paint(painter, _option, index);
}

} // namespace GuiManager::Delegates
