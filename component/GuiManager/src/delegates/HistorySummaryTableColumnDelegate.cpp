// MIT License
// Copyright (c) 2026 Sourcerer

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

  // Badges are laid out from the right edge: the bisect mark, then the phase
  // of a draft or secret Mercurial commit. Badges of one kind share the width
  // of their widest label so they line up.
  struct Badge {
    QString label;
    QColor color;
    QStringList labels;
    QRect rect;
  };
  QVector<Badge> badges;

  const QStringList bisectLabels = {
      QCoreApplication::translate("HistorySummary", "GOOD"),
      QCoreApplication::translate("HistorySummary", "BAD"),
      QCoreApplication::translate("HistorySummary", "SKIP"),
      QCoreApplication::translate("HistorySummary", "TEST")};
  const int mark = index.data(BisectMarkRole).toInt();
  switch (static_cast<GitRepo::BisectMark>(mark)) {
  case GitRepo::BisectMark::Good:
    badges.append(
        {bisectLabels.at(0), Models::bisectMarkColor(mark), bisectLabels, {}});
    break;
  case GitRepo::BisectMark::Bad:
    badges.append(
        {bisectLabels.at(1), Models::bisectMarkColor(mark), bisectLabels, {}});
    break;
  case GitRepo::BisectMark::Skip:
    badges.append(
        {bisectLabels.at(2), Models::bisectMarkColor(mark), bisectLabels, {}});
    break;
  case GitRepo::BisectMark::Current:
    badges.append(
        {bisectLabels.at(3), Models::bisectMarkColor(mark), bisectLabels, {}});
    break;
  default:
    break;
  }

  const QStringList phaseLabels = {
      QCoreApplication::translate("HistorySummary", "DRAFT"),
      QCoreApplication::translate("HistorySummary", "SECRET")};
  const int phase = index.data(PhaseRole).toInt();
  switch (static_cast<GitRepo::CommitPhase>(phase)) {
  case GitRepo::CommitPhase::Draft:
    badges.append(
        {phaseLabels.at(0), Models::phaseColor(phase), phaseLabels, {}});
    break;
  case GitRepo::CommitPhase::Secret:
    badges.append(
        {phaseLabels.at(1), Models::phaseColor(phase), phaseLabels, {}});
    break;
  default:
    break;
  }

  if (!badges.isEmpty()) {
    constexpr int padding = 6;
    const QFontMetrics metrics(opt.font);
    const int badgeHeight = qMin(metrics.height() + 2, opt.rect.height() - 2);

    int right = opt.rect.right() - padding;
    for (Badge &badge : badges) {
      int widestLabel = 0;
      for (const QString &label : badge.labels) {
        widestLabel = qMax(widestLabel, metrics.horizontalAdvance(label));
      }

      const int badgeWidth = widestLabel + 2 * padding;
      badge.rect =
          QRect(right - badgeWidth, opt.rect.center().y() - badgeHeight / 2,
                badgeWidth, badgeHeight);
      right = badge.rect.left() - padding;
    }

    // Keep the text clear of the badges.
    opt.text =
        metrics.elidedText(opt.text, Qt::ElideRight,
                           qMax(0, right - opt.rect.left() - 2 * padding));
  }

  // Let the style engine handle selection, focus, etc.
  const QWidget *widget = opt.widget;
  const QStyle *style = widget ? widget->style() : QApplication::style();

  painter->save();

  style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

  for (const Badge &badge : badges) {
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(badge.color);
    painter->drawRoundedRect(badge.rect, 3, 3);

    painter->setFont(opt.font);
    painter->setPen(Qt::white);
    painter->drawText(badge.rect, Qt::AlignCenter, badge.label);
  }

  painter->restore();
}
} // namespace GuiManager::Delegates
