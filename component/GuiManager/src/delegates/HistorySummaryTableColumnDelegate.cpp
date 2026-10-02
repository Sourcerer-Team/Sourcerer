#include "delegates/HistorySummaryTableColumnDelegate.h"
#include "models/GitHistoryModel.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFontMetrics>
#include <QRect>

namespace GuiManager::Delegates {

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
  int renamedCount = statusFieldsMap[Models::StatusFields::Renamed];
  int conflictedCount = statusFieldsMap[Models::StatusFields::Conflicted];

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

  const QMap<int, int> indexedCounters = {{0, newCount},
                                          {1, modifiedCount},
                                          {2, deletedCount},
                                          {3, renamedCount},
                                          {4, conflictedCount}};

  static const QMap<int, QColor> colors = {{0, QColor(0xB05C24)},
                                           {1, QColor(0xA03A3A)},
                                           {2, QColor(0x8B5C2A)},
                                           {3, QColor(0x7C3F58)},
                                           {4, QColor(0x556B2F)}};

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

    painter->drawRoundedRect(symbolRect.x(), symbolRect.y(), symbolRect.width(),
                             symbolRect.height(), 2, 2);

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

  QString badgeLabel;
  const int mark = index.data(BisectMarkRole).toInt();
  switch (static_cast<GitRepo::BisectMark>(mark)) {
  case GitRepo::BisectMark::Good:
    badgeLabel = QCoreApplication::translate("HistorySummary", "GOOD");
    break;
  case GitRepo::BisectMark::Bad:
    badgeLabel = QCoreApplication::translate("HistorySummary", "BAD");
    break;
  case GitRepo::BisectMark::Skip:
    badgeLabel = QCoreApplication::translate("HistorySummary", "SKIP");
    break;
  case GitRepo::BisectMark::Current:
    badgeLabel = QCoreApplication::translate("HistorySummary", "TEST");
    break;
  default:
    break;
  }

  QRect badgeRect;
  if (!badgeLabel.isEmpty()) {
    constexpr int padding = 6;
    const QFontMetrics metrics(opt.font);

    // All badges share the width of the widest label.
    int widestLabel = 0;
    for (const QString &label :
         {QCoreApplication::translate("HistorySummary", "GOOD"),
          QCoreApplication::translate("HistorySummary", "BAD"),
          QCoreApplication::translate("HistorySummary", "SKIP"),
          QCoreApplication::translate("HistorySummary", "TEST")}) {
      widestLabel = qMax(widestLabel, metrics.horizontalAdvance(label));
    }

    const int badgeWidth = widestLabel + 2 * padding;
    const int badgeHeight = qMin(metrics.height() + 2, opt.rect.height() - 2);

    badgeRect =
        QRect(opt.rect.right() - padding - badgeWidth,
              opt.rect.center().y() - badgeHeight / 2, badgeWidth, badgeHeight);

    // Keep the text clear of the badge.
    opt.text = metrics.elidedText(
        opt.text, Qt::ElideRight,
        qMax(0, opt.rect.width() - badgeWidth - 4 * padding));
  }

  // Let the style engine handle selection, focus, etc.
  const QWidget *widget = opt.widget;
  const QStyle *style = widget ? widget->style() : QApplication::style();

  painter->save();

  style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

  if (!badgeLabel.isEmpty()) {
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(Models::bisectMarkColor(mark));
    painter->drawRoundedRect(badgeRect, 3, 3);

    painter->setFont(opt.font);
    painter->setPen(Qt::white);
    painter->drawText(badgeRect, Qt::AlignCenter, badgeLabel);
  }

  painter->restore();
}
} // namespace GuiManager::Delegates
