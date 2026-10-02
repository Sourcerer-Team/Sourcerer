#include "delegates/HistorySliceTableColumnDelegate.h"
#include "models/GitHistoryModel.h"

#include <QApplication>
#include <QPainterPath>
#include <QRect>
#include <StylesheetManager.h>

namespace GuiManager::Delegates {

namespace {

QColor laneColor(const QVector<QColor> &palette, const int color) {
  if (palette.isEmpty() || color < 0) {
    return QColor(Qt::darkGreen);
  }

  return palette.at(color % palette.size());
}

// Geometry of one graph cell.
struct Cell {
  int left;
  int top;
  int width;
  int height;

  int right() const { return left + width; }
  int bottom() const { return top + height; }
  int centerX() const { return left + width / 2; }
  int centerY() const { return top + height / 2; }

  QPoint leftPoint() const { return {left, centerY()}; }
  QPoint rightPoint() const { return {right(), centerY()}; }
  QPoint topPoint() const { return {centerX(), top}; }
  QPoint bottomPoint() const { return {centerX(), bottom()}; }
  QPoint centerPoint() const { return {centerX(), centerY()}; }
};

// Lanes are normally as wide as the row is tall. The column width is derived
// from the font height, which can be smaller than the row height, so shrink
// the lanes when needed to keep every lane inside the column.
int laneWidth(const QRect &rect, const int laneCount) {
  const int width = std::max(1, rect.height());
  if (laneCount > 0 && rect.width() < laneCount * width) {
    return std::max(1, rect.width() / laneCount);
  }

  return width;
}

QPen lanePen(const QColor &color) {
  return QPen(QBrush(color), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
}

void drawLine(QPainter *painter, const QColor &color, const QPoint &from,
              const QPoint &to) {
  painter->setPen(lanePen(color));
  painter->drawLine(from, to);
}

// Quarter turn between the middle of a horizontal edge and the middle of a
// vertical edge of the cell. The curve bends toward the cell center.
void drawCorner(QPainter *painter, const QColor &color, const QPoint &horizontal,
                const QPoint &vertical) {
  QPainterPath path;
  path.moveTo(horizontal);
  path.quadTo(QPoint(vertical.x(), horizontal.y()), vertical);

  painter->setPen(lanePen(color));
  painter->setBrush(Qt::NoBrush);
  painter->drawPath(path);
}

void drawNode(QPainter *painter, const Cell &cell, const QColor &color) {
  const int radius = std::max(3, std::min(cell.width, cell.height) / 5);

  painter->setPen(lanePen(color));
  painter->setBrush(QBrush(color));
  painter->drawEllipse(cell.centerPoint(), radius, radius);
}

} // namespace

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

  if (!statusMap.isEmpty()) {
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

  if (!index.isValid()) {
    return;
  }

  QStyleOptionViewItem opt(option);
  initStyleOption(&opt, index);

  opt.features &= ~QStyleOptionViewItem::WrapText;
  opt.textElideMode = Qt::ElideRight;

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

  if (!index.isValid()) {
    return;
  }

  const auto row = index.data(GraphRowRole).value<Util::GraphRow>();
  if (row.isEmpty()) {
    paintDefault(painter, option, index);
    return;
  }

  // The working tree row above is attached to this commit: continue its line
  // into the commit.
  bool attachedToStatus = false;
  if (index.row() > 0 && index.data(WorkingTreeAnchorRole).toBool()) {
    const auto previousIndex = index.sibling(index.row() - 1, index.column());
    attachedToStatus = !previousIndex.data(StatusRole)
                            .value<Models::StatusFieldsMap>()
                            .isEmpty();
  }

  const QVector<QColor> palette =
      StylesheetManager::StylesheetManager::graphLaneColors();

  QStyleOptionViewItem opt(option);
  initStyleOption(&opt, index);

  QStyledItemDelegate::paint(painter, opt, index);

  const int cellHeight = opt.rect.height();
  const int cellWidth = laneWidth(
      opt.rect,
      static_cast<int>(index.data(HistorySliceWidthRole).value<qsizetype>()));

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setClipRect(opt.rect);

  for (int i = 0; i < row.size(); ++i) {
    const Util::GraphSlice &slice = row[i];
    const Cell cell{opt.rect.left() + i * cellWidth, opt.rect.top(), cellWidth,
                    cellHeight};

    // Cells entirely to the right of the visible area are not worth drawing.
    if (cell.left > opt.rect.right()) {
      break;
    }

    const bool hasNode = slice.node >= 0;
    const bool hasTop = slice.top >= 0 || (hasNode && attachedToStatus);
    const int topColor = slice.top >= 0 ? slice.top : slice.node;

    if (hasNode) {
      // Every edge of a commit cell runs from its dot.
      if (hasTop) {
        drawLine(painter, laneColor(palette, topColor), cell.topPoint(),
                 cell.centerPoint());
      }
      if (slice.bottom >= 0) {
        drawLine(painter, laneColor(palette, slice.bottom), cell.centerPoint(),
                 cell.bottomPoint());
      }
      if (slice.left >= 0) {
        drawLine(painter, laneColor(palette, slice.left), cell.leftPoint(),
                 cell.centerPoint());
      }
      if (slice.right >= 0) {
        drawLine(painter, laneColor(palette, slice.right), cell.centerPoint(),
                 cell.rightPoint());
      }

      drawNode(painter, cell, laneColor(palette, slice.node));
      continue;
    }

    const bool hasBottom = slice.bottom >= 0;

    if (hasTop && hasBottom) {
      // A lane passing through; horizontal edges cross or end at it.
      if (slice.top == slice.bottom) {
        drawLine(painter, laneColor(palette, slice.top), cell.topPoint(),
                 cell.bottomPoint());
      } else {
        drawLine(painter, laneColor(palette, slice.top), cell.topPoint(),
                 cell.centerPoint());
        drawLine(painter, laneColor(palette, slice.bottom), cell.centerPoint(),
                 cell.bottomPoint());
      }

      if (slice.left >= 0) {
        drawLine(painter, laneColor(palette, slice.left), cell.leftPoint(),
                 cell.centerPoint());
      }
      if (slice.right >= 0) {
        drawLine(painter, laneColor(palette, slice.right), cell.centerPoint(),
                 cell.rightPoint());
      }
      continue;
    }

    bool leftDrawn = slice.left < 0;
    bool rightDrawn = slice.right < 0;

    if (hasTop || hasBottom) {
      // A lane that starts or ends here. Horizontal edges turn into it: when
      // there are two of them, both arc to the same vertical edge midpoint.
      const int verticalColor = hasTop ? slice.top : slice.bottom;
      const QPoint vertical = hasTop ? cell.topPoint() : cell.bottomPoint();
      const QColor color = laneColor(palette, verticalColor);

      if (slice.left >= 0 && slice.right >= 0) {
        drawCorner(painter, laneColor(palette, slice.left), cell.leftPoint(),
                   vertical);
        drawCorner(painter, laneColor(palette, slice.right), cell.rightPoint(),
                   vertical);
        leftDrawn = true;
        rightDrawn = true;

        if (slice.left != verticalColor && slice.right != verticalColor) {
          drawLine(painter, color, vertical, cell.centerPoint());
        }
      } else if (slice.left == verticalColor) {
        drawCorner(painter, color, cell.leftPoint(), vertical);
        leftDrawn = true;
      } else if (slice.right == verticalColor) {
        drawCorner(painter, color, cell.rightPoint(), vertical);
        rightDrawn = true;
      } else {
        drawLine(painter, color, vertical, cell.centerPoint());
      }
    }

    if (!leftDrawn) {
      drawLine(painter, laneColor(palette, slice.left), cell.leftPoint(),
               cell.centerPoint());
    }
    if (!rightDrawn) {
      drawLine(painter, laneColor(palette, slice.right), cell.centerPoint(),
               cell.rightPoint());
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

  int anchorLane = -1;
  int anchorColor = -1;

  // Connect to the commit the working tree is based on, if it is the next row.
  const QModelIndex nextIndex = index.sibling(index.row() + 1, index.column());
  if (nextIndex.isValid() && nextIndex.data(WorkingTreeAnchorRole).toBool()) {
    const auto row = nextIndex.data(GraphRowRole).value<Util::GraphRow>();
    for (int i = 0; i < row.size(); ++i) {
      if (row[i].node >= 0) {
        anchorLane = i;
        anchorColor = row[i].node;
        break;
      }
    }
  }

  QStyleOptionViewItem opt(option);
  initStyleOption(&opt, index);

  const int cellHeight = opt.rect.height();
  const int cellWidth = laneWidth(
      opt.rect,
      static_cast<int>(index.data(HistorySliceWidthRole).value<qsizetype>()));

  QStyledItemDelegate::paint(painter, opt, index);

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setClipRect(opt.rect);

  const int lane = std::max(0, anchorLane);
  const Cell cell{opt.rect.left() + lane * cellWidth, opt.rect.top(), cellWidth,
                  cellHeight};

  if (anchorLane != -1) {
    const QVector<QColor> palette =
        StylesheetManager::StylesheetManager::graphLaneColors();
    drawLine(painter, laneColor(palette, anchorColor), cell.centerPoint(),
             cell.bottomPoint());
  }

  drawNode(painter, cell, QColor(Qt::darkYellow));

  painter->restore();
}

} // namespace GuiManager::Delegates
