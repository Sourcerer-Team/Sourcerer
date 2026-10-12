#include "delegates/HomeRecentItemDelegate.h"

#include <StylesheetManager.h>

#include <Log.h>

#include <QAbstractItemView>
#include <QPainter>
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

  if (_indexHovered.isValid() && _indexHovered.model() == index.model() &&
      _indexHovered.row() == index.row() && index.column() == 0) {
    const auto theme = StylesheetManager::StylesheetManager::currentStylesheet();
    const QColor hoverBackground =
        theme == StylesheetManager::StylesheetKey::SourcererLight
            ? QColor(255, 241, 201)
        : theme == StylesheetManager::StylesheetKey::SourcererPurple
            ? QColor(63, 77, 99)
        : theme == StylesheetManager::StylesheetKey::SourcererBlue
            ? QColor(42, 58, 85)
        : theme == StylesheetManager::StylesheetKey::SourcererMonokai
            ? QColor(62, 61, 50)
        : theme == StylesheetManager::StylesheetKey::SourcererGruvbox
            ? QColor(60, 56, 54)
            : QColor(66, 66, 66);
    const auto *view = qobject_cast<const QAbstractItemView *>(option.widget);
    const int rowWidth =
        view ? view->viewport()->width() : _option.rect.width();
    painter->fillRect(
        QRect(0, _option.rect.top(), rowWidth, _option.rect.height()),
        hoverBackground);
  }

  QStyledItemDelegate::paint(painter, _option, index);
}

} // namespace GuiManager::Delegates
