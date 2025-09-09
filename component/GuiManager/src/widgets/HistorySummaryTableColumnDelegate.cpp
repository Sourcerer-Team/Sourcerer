#include "widgets/HistorySummaryTableColumnDelegate.h"
#include "models/GitHistoryModel.h"

#include <QApplication>
#include <QRect>

namespace GuiManager::Widgets {

HistorySummaryTableColumnDelegate::HistorySummaryTableColumnDelegate(
    QObject *parent)
    : QStyledItemDelegate(parent) {}

void HistorySummaryTableColumnDelegate::paint(
    QPainter *painter, const QStyleOptionViewItem &option,
    const QModelIndex &index) const {
  Q_ASSERT(index.column() == COLUMN_SUMMARY);
  if (index.column() != COLUMN_SUMMARY) {
    paintDefault(painter, option, index);
    return;
  }

  const auto statusFieldsMap =
      index.data(StatusRole).value<Models::StatusFieldsMap>();
  if (statusFieldsMap.count() > 0 && index.row() == 0) {
    paintStatus(painter, option, index, statusFieldsMap);
    return;
  }

  paintDefault(painter, option, index);
}

void HistorySummaryTableColumnDelegate::paintStatus(
    QPainter *painter, const QStyleOptionViewItem &option,
    const QModelIndex &index,
    const QMap<Models::StatusFields, int> &statusFieldsMap) const {
  if (!painter) {
    return;
  }

  int newCount = statusFieldsMap[Models::StatusFields::New];
  int modifiedCount = statusFieldsMap[Models::StatusFields::Modified];
  int deletedCount = statusFieldsMap[Models::StatusFields::Deleted];
  int renamedCount = statusFieldsMap[Models::StatusFields::Modified];

  newCount += statusFieldsMap[Models::StatusFields::NewStaged];
  modifiedCount += statusFieldsMap[Models::StatusFields::ModifiedStaged];
  deletedCount += statusFieldsMap[Models::StatusFields::DeletedStaged];
  renamedCount += statusFieldsMap[Models::StatusFields::RenamedStaged];

  paintDefault(painter, option, index);

  QStyleOptionViewItem opt(option);
  initStyleOption(&opt, index);

  const int cellHeight = opt.rect.height();
  const int cellWidth = opt.rect.width();

  QStyledItemDelegate::paint(painter, opt, index);

  painter->save();

  painter->setRenderHint(QPainter::Antialiasing, true);

  QRegion clipRegion(opt.rect);
  painter->setClipRegion(clipRegion);

  const QMap<int, int> indexedCounters = {
      {0, newCount}, {1, modifiedCount}, {2, deletedCount}, {3, renamedCount}};

  static const QMap<int, QColor> colors = {{0, QColor::fromString("#B05C24")},
                                           {1, QColor::fromString("#A03A3A")},
                                           {2, QColor::fromString("#8B5C2A")},
                                           {3, QColor::fromString("#7C3F58")}};

  const QFontMetrics metrics(painter->font());

  int prevWidth = 0;

  for (int i = 0; i < indexedCounters.count(); ++i) {
    const int counter = indexedCounters[i];
    if (counter < 1) {
      continue;
    }
    painter->setPen(
        QPen(QBrush(colors[i]), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(QBrush(colors[i]));

    constexpr int padding = 8;
    const int rectWidth = std::max(10, opt.rect.height() - 20);

    const QRect symbolRect(
        QPoint(prevWidth + padding + opt.rect.left(),
               opt.rect.top() + opt.rect.height() / 2 - rectWidth / 2),
        QPoint(prevWidth + padding + opt.rect.left() + rectWidth,
               opt.rect.top() + opt.rect.height() / 2 - rectWidth / 2 +
                   rectWidth));

    prevWidth += padding; // spacing
    prevWidth += symbolRect.width();

    painter->drawRect(symbolRect.x(), symbolRect.y(), symbolRect.width(),
                      symbolRect.height());

    const QString counterStr = QString::number(counter);

    prevWidth += padding / 2; // spacing

    const QRect textRect(prevWidth + opt.rect.left(), opt.rect.top(),
                         opt.rect.width(), opt.rect.height());
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, counterStr);

    const int textWidth = metrics.horizontalAdvance(counterStr);

    prevWidth += textWidth;
  }

  painter->restore();
}

void HistorySummaryTableColumnDelegate::paintDefault(
    QPainter *painter, const QStyleOptionViewItem &option,
    const QModelIndex &index) const {
  if (!painter) {
    return;
  }

  QStyleOptionViewItem opt(option); // Make a copy so we can modify it
  initStyleOption(&opt, index);     // Populate with model data

  opt.features &= ~QStyleOptionViewItem::WrapText;
  opt.textElideMode = Qt::ElideRight;

  // Let the style engine handle selection, focus, etc.
  const QWidget *widget = opt.widget;
  const QStyle *style = widget ? widget->style() : QApplication::style();

  painter->save();

  style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

  painter->restore();
}
} // namespace GuiManager::Widgets
