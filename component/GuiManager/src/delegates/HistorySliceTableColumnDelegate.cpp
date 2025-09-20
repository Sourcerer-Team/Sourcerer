#include "delegates/HistorySliceTableColumnDelegate.h"
#include "models/GitHistoryModel.h"

#include <QApplication>
#include <QRect>

namespace GuiManager::Delegates {

HistorySliceTableColumnDelegate::HistorySliceTableColumnDelegate(
    QObject *parent)
    : QStyledItemDelegate(parent) {}

void HistorySliceTableColumnDelegate::paint(QPainter *painter,
                                            const QStyleOptionViewItem &option,
                                            const QModelIndex &index) const {
  Q_ASSERT(index.column() == COLUMN_HISTORY_SLICE);
  if (index.column() != COLUMN_HISTORY_SLICE) {
    paintDefault(painter, option, index);
    return;
  }

  const auto width = index.data(HistorySliceWidthRole).value<qsizetype>();
  Q_ASSERT(width >= 0);
  if (width < 0) {
    paintDefault(painter, option, index);
    return;
  }

  const auto statusMap =
      index.data(StatusRole).value<Models::StatusFieldsMap>();

  if (const bool hasStatus = !statusMap.isEmpty()) {
    paintStatus(painter, option, index);
  } else {
    paintSlices(painter, option, index);
  }
}

void HistorySliceTableColumnDelegate::paintDefault(
    QPainter *painter, const QStyleOptionViewItem &option,
    const QModelIndex &index) const {
  Q_ASSERT(painter != nullptr);
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

void HistorySliceTableColumnDelegate::paintSlices(
    QPainter *painter, const QStyleOptionViewItem &option,
    const QModelIndex &index) const {
  Q_ASSERT(painter != nullptr);
  if (!painter) {
    return;
  }

  bool hasStatus = false;

  if (index.row() == 1) {
    const auto previousIndex = index.sibling(index.row() - 1, index.column());
    hasStatus = !previousIndex.data(StatusRole)
                     .value<Models::StatusFieldsMap>()
                     .isEmpty();
  }

  const auto previousIndex = index.sibling(index.row() - 1, index.column());

  const auto slices =
      index.data(Qt::DisplayRole).value<GuiManager::Models::Slices>();

  QStyleOptionViewItem opt(option);
  initStyleOption(&opt, index);

  const int cellHeight = opt.rect.height();
  const int cellWidth = cellHeight;

  QStyledItemDelegate::paint(painter, opt, index);

  painter->save();

  painter->setRenderHint(QPainter::Antialiasing, true);

  QRegion clipRegion(opt.rect);
  painter->setClipRegion(clipRegion);

  painter->setPen(QPen(QBrush(Qt::darkGreen), 2, Qt::SolidLine, Qt::RoundCap,
                       Qt::RoundJoin));
  painter->setBrush(QBrush(Qt::darkGreen));

  for (int i = 0; i < slices.size(); ++i) {
    bool alreadyDrawnTopEdgeForStatus = false;
    if (const Models::Slice &slice = slices[i];
        slice.top && slice.left && !slice.right && !slice.bottom) {
      // Top-left corner
      if (slice.main) {
        QList<QPoint> points;

        // Left
        points += QPoint(opt.rect.left() + i * cellWidth,
                         opt.rect.top() + cellHeight / 2);
        points += QPoint(opt.rect.left() + i * cellWidth + cellWidth / 2,
                         opt.rect.top() + cellHeight / 2);

        // Top
        points += QPoint(opt.rect.left() + i * cellWidth + cellWidth / 2,
                         opt.rect.top());
        points += QPoint(opt.rect.left() + i * cellWidth + cellWidth / 2,
                         opt.rect.top() + cellHeight / 2);

        painter->drawLines(points);

        // Draw commit circle
        const int centerX = opt.rect.x() + cellWidth / 2 + i * cellWidth;
        const int centerY = opt.rect.y() + cellHeight / 2;
        const int radius = qMin(cellWidth, cellHeight) / 6;
        painter->drawEllipse(QPoint(centerX, centerY), radius, radius);
      } else {
        const QPoint p0(opt.rect.left() + i * cellWidth - cellWidth / 2 - 1,
                        opt.rect.top() - cellHeight / 2 - 1);
        const QPoint p1(opt.rect.left() + (i + 1) * cellWidth - cellWidth / 2 -
                            1,
                        opt.rect.top() + cellHeight - cellHeight / 2 - 1);
        const QRect rect(p0, p1);

        constexpr int startAngle = 270 * 16;
        constexpr int spanAngle = 90 * 16;

        painter->drawArc(rect, startAngle, spanAngle);
      }
    } else if (slice.top && slice.right && !slice.left && !slice.bottom) {
      // Top-right corner
      if (slice.main) {
        QList<QPoint> points;

        // Top
        points += QPoint(opt.rect.left() + i * cellWidth + cellWidth / 2,
                         opt.rect.top());
        points += QPoint(opt.rect.left() + i * cellWidth + cellWidth / 2,
                         opt.rect.top() + cellHeight / 2);

        // Right
        points += QPoint(opt.rect.left() + i * cellWidth + cellWidth / 2,
                         opt.rect.top() + cellHeight / 2);
        points += QPoint(opt.rect.left() + i * cellWidth + cellWidth,
                         opt.rect.top() + cellHeight / 2);

        painter->drawLines(points);

        // Draw commit circle
        const int centerX = opt.rect.x() + cellWidth / 2 + i * cellWidth;
        const int centerY = opt.rect.y() + cellHeight / 2;
        const int radius = qMin(cellWidth, cellHeight) / 6;
        painter->drawEllipse(QPoint(centerX, centerY), radius, radius);
      } else {
        const QPoint p0(opt.rect.left() + i * cellWidth + cellWidth / 2,
                        opt.rect.top() - cellHeight / 2 - 1);
        const QPoint p1(opt.rect.left() + i * cellWidth + cellWidth +
                            cellWidth / 2,
                        opt.rect.top() + cellHeight - cellHeight / 2 - 1);
        const QRect rect(p0, p1);

        constexpr int startAngle = 180 * 16;
        constexpr int spanAngle = 90 * 16;

        painter->drawArc(rect, startAngle, spanAngle);
      }
    } else if (slice.bottom && slice.left && !slice.right && !slice.top) {
      // Bottom-left corner
      if (slice.main ||
          (slice.main && hasStatus && !alreadyDrawnTopEdgeForStatus)) {
        QList<QPoint> points;

        if (hasStatus) {
          // Top
          points += QPoint(opt.rect.left() + i * cellWidth + cellWidth / 2,
                           opt.rect.top());
          points += QPoint(opt.rect.left() + i * cellWidth + cellWidth / 2,
                           opt.rect.top() + cellHeight / 2);

          alreadyDrawnTopEdgeForStatus = true;
        }

        // Left
        points += QPoint(opt.rect.left() + i * cellWidth,
                         opt.rect.top() + cellHeight / 2);
        points += QPoint(opt.rect.left() + i * cellWidth + cellWidth / 2,
                         opt.rect.top() + cellHeight / 2);

        // Bottom
        points += QPoint(opt.rect.left() + i * cellWidth + cellWidth / 2,
                         opt.rect.top() + cellHeight / 2);
        points += QPoint(opt.rect.left() + i * cellWidth + cellWidth / 2,
                         opt.rect.top() + cellHeight);

        painter->drawLines(points);

        // Draw commit circle
        const int centerX = opt.rect.x() + cellWidth / 2 + i * cellWidth;
        const int centerY = opt.rect.y() + cellHeight / 2;
        const int radius = qMin(cellWidth, cellHeight) / 6;
        painter->drawEllipse(QPoint(centerX, centerY), radius, radius);
      } else {
        const QPoint p0(opt.rect.left() + i * cellWidth - cellWidth / 2 - 1,
                        opt.rect.top() + cellHeight / 2);
        const QPoint p1(opt.rect.left() + i * cellWidth + cellWidth -
                            cellWidth / 2 - 1,
                        opt.rect.top() + cellHeight + cellHeight / 2);
        const QRect rect(p0, p1);

        constexpr int startAngle = 90 * 16;
        constexpr int spanAngle = -90 * 16;

        painter->drawArc(rect, startAngle, spanAngle);
      }
    } else if (slice.bottom && slice.right && !slice.left && !slice.top) {
      // Bottom-right corner
      if (slice.main ||
          (slice.main && hasStatus && !alreadyDrawnTopEdgeForStatus)) {
        QList<QPoint> points;

        if (hasStatus) {
          // Top
          points += QPoint(opt.rect.left() + i * cellWidth + cellWidth / 2,
                           opt.rect.top());
          points += QPoint(opt.rect.left() + i * cellWidth + cellWidth / 2,
                           opt.rect.top() + cellHeight / 2);

          alreadyDrawnTopEdgeForStatus = true;
        }

        // Bottom
        points += QPoint(opt.rect.left() + i * cellWidth + cellWidth / 2,
                         opt.rect.top() + cellHeight / 2);
        points += QPoint(opt.rect.left() + i * cellWidth + cellWidth / 2,
                         opt.rect.top() + cellHeight);

        // Right
        points += QPoint(opt.rect.left() + i * cellWidth + cellWidth / 2,
                         opt.rect.top() + cellHeight / 2);
        points += QPoint(opt.rect.left() + i * cellWidth + cellWidth,
                         opt.rect.top() + cellHeight / 2);

        painter->drawLines(points);

        // Draw commit circle
        const int centerX = opt.rect.x() + cellWidth / 2 + i * cellWidth;
        const int centerY = opt.rect.y() + cellHeight / 2;
        const int radius = qMin(cellWidth, cellHeight) / 6;
        painter->drawEllipse(QPoint(centerX, centerY), radius, radius);
      } else {
        const QPoint p0(opt.rect.left() + i * cellWidth + cellWidth / 2,
                        opt.rect.top() + cellHeight / 2);
        const QPoint p1(opt.rect.left() + i * cellWidth + +cellWidth +
                            cellWidth / 2,
                        opt.rect.top() + cellHeight + cellHeight / 2);
        const QRect rect(p0, p1);

        constexpr int startAngle = 180 * 16;
        constexpr int spanAngle = -90 * 16;

        painter->drawArc(rect, startAngle, spanAngle);
      }
    } else {
      if (slice.left) {
        // Left horizontal line
        QList<QPoint> points;
        points += QPoint(opt.rect.left() + i * cellWidth,
                         opt.rect.top() + cellHeight / 2);
        points += QPoint(opt.rect.left() + i * cellWidth + cellWidth / 2,
                         opt.rect.top() + cellHeight / 2);

        painter->drawLines(points);
      }

      if (slice.right) {
        // Right horizontal line
        QList<QPoint> points;
        points += QPoint(opt.rect.left() + i * cellWidth + cellWidth / 2,
                         opt.rect.top() + cellHeight / 2);
        points += QPoint(opt.rect.left() + i * cellWidth + cellWidth,
                         opt.rect.top() + cellHeight / 2);

        painter->drawLines(points);
      }

      if (slice.top || (slice.main && hasStatus)) {
        // Top vertical line
        QList<QPoint> points;
        points += QPoint(opt.rect.left() + i * cellWidth + cellWidth / 2,
                         opt.rect.top());
        points += QPoint(opt.rect.left() + i * cellWidth + cellWidth / 2,
                         opt.rect.top() + cellHeight / 2);

        painter->drawLines(points);
      }

      if (slice.bottom) {
        // Bottom vertical line
        QList<QPoint> points;
        points += QPoint(opt.rect.left() + i * cellWidth + cellWidth / 2,
                         opt.rect.top() + cellHeight / 2);
        points += QPoint(opt.rect.left() + i * cellWidth + cellWidth / 2,
                         opt.rect.top() + cellHeight);

        painter->drawLines(points);
      }

      if (slice.main) {
        // Draw commit circle
        const int centerX = opt.rect.x() + cellWidth / 2 + i * cellWidth;
        const int centerY = opt.rect.y() + cellHeight / 2;
        const int radius = qMin(cellWidth, cellHeight) / 6;
        painter->drawEllipse(QPoint(centerX, centerY), radius, radius);
      }
    }
  }

  painter->restore();
}

void HistorySliceTableColumnDelegate::paintStatus(
    QPainter *painter, const QStyleOptionViewItem &option,
    const QModelIndex &index) const {
  Q_ASSERT(painter != nullptr);
  if (!painter) {
    return;
  }

  int avatarIndex = -1;

  const QModelIndex nextIndex = index.sibling(index.row() + 1, index.column());
  if (nextIndex.isValid()) {
    const QList<Models::Slice> slices =
        nextIndex.data(Qt::DisplayRole).value<GuiManager::Models::Slices>();
    if (!slices.isEmpty()) {
      for (int i = 0; i < slices.size(); ++i) {
        if (slices[i].main) {
          avatarIndex = i;
          break;
        }
      }
    }
  }

  QStyleOptionViewItem opt(option);
  initStyleOption(&opt, index);

  const int cellHeight = opt.rect.height();
  const int cellWidth = cellHeight;

  QStyledItemDelegate::paint(painter, opt, index);

  painter->save();

  painter->setRenderHint(QPainter::Antialiasing, true);

  if (avatarIndex != -1) {
    // Bottom vertical line
    QList<QPoint> points;
    points += QPoint(opt.rect.left() + avatarIndex * cellWidth + cellWidth / 2,
                     opt.rect.top() + cellHeight / 2);
    points += QPoint(opt.rect.left() + avatarIndex * cellWidth + cellWidth / 2,
                     opt.rect.top() + cellHeight);

    painter->setPen(QPen(QBrush(Qt::darkGreen), 2, Qt::SolidLine, Qt::RoundCap,
                         Qt::RoundJoin));
    painter->setBrush(QBrush(Qt::darkGreen));

    painter->drawLines(points);
  }

  if (avatarIndex == -1) {
    avatarIndex = 0;
  }

  const int centerX = opt.rect.x() + cellWidth / 2 + avatarIndex * cellWidth;
  const int centerY = opt.rect.y() + cellHeight / 2;
  const int radius = qMin(cellWidth, cellHeight) / 6;

  painter->setPen(QPen(QBrush(Qt::darkYellow), 2, Qt::SolidLine, Qt::RoundCap,
                       Qt::RoundJoin));
  painter->setBrush(QBrush(Qt::darkYellow));

  painter->drawEllipse(QPoint(centerX, centerY), radius, radius);

  painter->restore();
}

} // namespace GuiManager::Delegates
