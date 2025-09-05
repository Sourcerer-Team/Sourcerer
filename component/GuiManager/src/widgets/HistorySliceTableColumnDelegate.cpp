#include "widgets/HistorySliceTableColumnDelegate.h"
#include "models/GitHistoryModel.h"

#include <QApplication>

namespace GuiManager::Widgets {

HistorySliceTableColumnDelegate::HistorySliceTableColumnDelegate(
  QObject *parent)
    : QStyledItemDelegate(parent) {}

void HistorySliceTableColumnDelegate::paint(QPainter *painter,
                                            const QStyleOptionViewItem &option,
                                            const QModelIndex &index) const {
  if (index.column() != COLUMN_HISTORY_SLICE) {
    paintDefault(painter, option, index);
    return;
  }

  const auto width = index.data(HistorySliceWidthRole).value<qsizetype>();
  if (width <= 0) {
    paintDefault(painter, option, index);
    return;
  }

  const auto slices =
    index.data(Qt::DisplayRole).value<GuiManager::Models::Slices>();

  const int cellHeight = option.rect.height();
  const int cellWidth = cellHeight /*option.rect.width() / width*/;

  painter->save();

  painter->setRenderHint(QPainter::Antialiasing, true);

  QRegion clipRegion(option.rect);
  painter->setClipRegion(clipRegion);

  if (option.state & QStyle::State_Selected) {
    painter->fillRect(option.rect, option.palette.highlight());
  }

  painter->setPen(QPen(QBrush(Qt::darkGreen), 2, Qt::SolidLine, Qt::RoundCap,
                       Qt::RoundJoin));
  painter->setBrush(QBrush(Qt::darkGreen));

  for (int i = 0; i < slices.size(); ++i) {
    if (const Models::Slice &slice = slices[i];
        slice.top && slice.left && !slice.right && !slice.bottom) {
      // Top-left corner
      if (slice.main) {
        QList<QPoint> points;

        // Left
        points += QPoint(option.rect.left() + i * cellWidth,
                         option.rect.top() + cellHeight / 2);
        points += QPoint(option.rect.left() + i * cellWidth + cellWidth / 2,
                         option.rect.top() + cellHeight / 2);

        // Top
        points += QPoint(option.rect.left() + i * cellWidth + cellWidth / 2,
                         option.rect.top());
        points += QPoint(option.rect.left() + i * cellWidth + cellWidth / 2,
                         option.rect.top() + cellHeight / 2);

        painter->drawLines(points);

        // Draw commit circle
        const int centerX = option.rect.x() + cellWidth / 2 + i * cellWidth;
        const int centerY = option.rect.y() + cellHeight / 2;
        const int radius = qMin(cellWidth, cellHeight) / 6;
        painter->drawEllipse(QPoint(centerX, centerY), radius, radius);
      } else {
        const QPoint p0(option.rect.left() + i * cellWidth - cellWidth / 2 - 1,
                        option.rect.top() - cellHeight / 2 - 1);
        const QPoint p1(option.rect.left() + (i + 1) * cellWidth -
                          cellWidth / 2 - 1,
                        option.rect.top() + cellHeight - cellHeight / 2 - 1);
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
        points += QPoint(option.rect.left() + i * cellWidth + cellWidth / 2,
                         option.rect.top());
        points += QPoint(option.rect.left() + i * cellWidth + cellWidth / 2,
                         option.rect.top() + cellHeight / 2);

        // Right
        points += QPoint(option.rect.left() + i * cellWidth + cellWidth / 2,
                         option.rect.top() + cellHeight / 2);
        points += QPoint(option.rect.left() + i * cellWidth + cellWidth,
                         option.rect.top() + cellHeight / 2);

        painter->drawLines(points);

        // Draw commit circle
        const int centerX = option.rect.x() + cellWidth / 2 + i * cellWidth;
        const int centerY = option.rect.y() + cellHeight / 2;
        const int radius = qMin(cellWidth, cellHeight) / 6;
        painter->drawEllipse(QPoint(centerX, centerY), radius, radius);
      } else {
        const QPoint p0(option.rect.left() + i * cellWidth + cellWidth / 2,
                        option.rect.top() - cellHeight / 2 - 1);
        const QPoint p1(option.rect.left() + i * cellWidth + cellWidth +
                          cellWidth / 2,
                        option.rect.top() + cellHeight - cellHeight / 2 - 1);
        const QRect rect(p0, p1);

        constexpr int startAngle = 180 * 16;
        constexpr int spanAngle = 90 * 16;

        painter->drawArc(rect, startAngle, spanAngle);
      }
    } else if (slice.bottom && slice.left && !slice.right && !slice.top) {
      // Bottom-left corner
      if (slice.main) {
        QList<QPoint> points;

        // Left
        points += QPoint(option.rect.left() + i * cellWidth,
                         option.rect.top() + cellHeight / 2);
        points += QPoint(option.rect.left() + i * cellWidth + cellWidth / 2,
                         option.rect.top() + cellHeight / 2);

        // Bottom
        points += QPoint(option.rect.left() + i * cellWidth + cellWidth / 2,
                         option.rect.top() + cellHeight / 2);
        points += QPoint(option.rect.left() + i * cellWidth + cellWidth / 2,
                         option.rect.top() + cellHeight);

        painter->drawLines(points);

        // Draw commit circle
        const int centerX = option.rect.x() + cellWidth / 2 + i * cellWidth;
        const int centerY = option.rect.y() + cellHeight / 2;
        const int radius = qMin(cellWidth, cellHeight) / 6;
        painter->drawEllipse(QPoint(centerX, centerY), radius, radius);
      } else {
        const QPoint p0(option.rect.left() + i * cellWidth - cellWidth / 2 - 1,
                        option.rect.top() + cellHeight / 2);
        const QPoint p1(option.rect.left() + i * cellWidth + cellWidth -
                          cellWidth / 2 - 1,
                        option.rect.top() + cellHeight + cellHeight / 2);
        const QRect rect(p0, p1);

        constexpr int startAngle = 90 * 16;
        constexpr int spanAngle = -90 * 16;

        painter->drawArc(rect, startAngle, spanAngle);
      }
    } else if (slice.bottom && slice.right && !slice.left && !slice.top) {
      // Bottom-right corner
      if (slice.main) {
        QList<QPoint> points;

        // Bottom
        points += QPoint(option.rect.left() + i * cellWidth + cellWidth / 2,
                         option.rect.top() + cellHeight / 2);
        points += QPoint(option.rect.left() + i * cellWidth + cellWidth / 2,
                         option.rect.top() + cellHeight);

        // Right
        points += QPoint(option.rect.left() + i * cellWidth + cellWidth / 2,
                         option.rect.top() + cellHeight / 2);
        points += QPoint(option.rect.left() + i * cellWidth + cellWidth,
                         option.rect.top() + cellHeight / 2);

        painter->drawLines(points);

        // Draw commit circle
        const int centerX = option.rect.x() + cellWidth / 2 + i * cellWidth;
        const int centerY = option.rect.y() + cellHeight / 2;
        const int radius = qMin(cellWidth, cellHeight) / 6;
        painter->drawEllipse(QPoint(centerX, centerY), radius, radius);
      } else {
        const QPoint p0(option.rect.left() + i * cellWidth + cellWidth / 2,
                        option.rect.top() + cellHeight / 2);
        const QPoint p1(option.rect.left() + i * cellWidth + +cellWidth +
                          cellWidth / 2,
                        option.rect.top() + cellHeight + cellHeight / 2);
        const QRect rect(p0, p1);

        constexpr int startAngle = 180 * 16;
        constexpr int spanAngle = -90 * 16;

        painter->drawArc(rect, startAngle, spanAngle);
      }
    } else {
      if (slice.left) {
        // Left horizontal line
        QList<QPoint> points;
        points += QPoint(option.rect.left() + i * cellWidth,
                         option.rect.top() + cellHeight / 2);
        points += QPoint(option.rect.left() + i * cellWidth + cellWidth / 2,
                         option.rect.top() + cellHeight / 2);

        painter->drawLines(points);
      }

      if (slice.right) {
        // Right horizontal line
        QList<QPoint> points;
        points += QPoint(option.rect.left() + i * cellWidth + cellWidth / 2,
                         option.rect.top() + cellHeight / 2);
        points += QPoint(option.rect.left() + i * cellWidth + cellWidth,
                         option.rect.top() + cellHeight / 2);

        painter->drawLines(points);
      }

      if (slice.top) {
        // Top vertical line
        QList<QPoint> points;
        points += QPoint(option.rect.left() + i * cellWidth + cellWidth / 2,
                         option.rect.top());
        points += QPoint(option.rect.left() + i * cellWidth + cellWidth / 2,
                         option.rect.top() + cellHeight / 2);

        painter->drawLines(points);
      }

      if (slice.bottom) {
        // Bottom vertical line
        QList<QPoint> points;
        points += QPoint(option.rect.left() + i * cellWidth + cellWidth / 2,
                         option.rect.top() + cellHeight / 2);
        points += QPoint(option.rect.left() + i * cellWidth + cellWidth / 2,
                         option.rect.top() + cellHeight);

        painter->drawLines(points);
      }

      if (slice.main) {
        // Draw commit circle
        const int centerX = option.rect.x() + cellWidth / 2 + i * cellWidth;
        const int centerY = option.rect.y() + cellHeight / 2;
        const int radius = qMin(cellWidth, cellHeight) / 6;
        painter->drawEllipse(QPoint(centerX, centerY), radius, radius);
      }
    }
  }

  painter->restore();
}

void HistorySliceTableColumnDelegate::paintDefault(
  QPainter *painter, const QStyleOptionViewItem &option,
  const QModelIndex &index) const {
  QStyleOptionViewItem opt(option); // Make a copy so we can modify it
  initStyleOption(&opt, index);     // Populate with model data

  opt.features &= ~QStyleOptionViewItem::WrapText;
  opt.textElideMode = Qt::ElideRight; // Optional: truncate long text

  // Let the style engine handle selection, focus, etc.
  const QWidget *widget = opt.widget;
  const QStyle *style = widget ? widget->style() : QApplication::style();

  painter->save();

  style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

  painter->restore();
}
} // namespace GuiManager::Widgets
