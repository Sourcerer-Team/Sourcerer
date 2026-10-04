#include "widgets/RefBadgesOverlay.h"

#include "delegates/HistorySliceTableColumnDelegate.h"
#include "models/GitHistoryModel.h"

#include <QCursor>
#include <QEvent>
#include <QFontMetrics>
#include <QHeaderView>
#include <QPainter>
#include <QScrollBar>
#include <QTableView>

#include <algorithm>

namespace GuiManager::Widgets {

namespace {

constexpr int BadgeMaxTextWidth = 140;
constexpr int BadgePaddingX = 5;
constexpr int BadgeGap = 3;
constexpr int BadgeLineGap = 2;
constexpr int BadgeLeadingGap = 6;
constexpr int BadgeRightMargin = 6;
// The hovered row shows its badges wrapped into this many lines.
constexpr int HoverMaxLines = 2;
// The cursor has to rest on a row this long before the branches containing
// the commit are looked up.
constexpr int RequestDelayMs = 150;

struct Badge {
  QString text;
  QColor fill;
  QColor textColor;
};

Badge makeBadge(const QString &ref) {
  static const QString heads = QStringLiteral("refs/heads/");
  static const QString remotes = QStringLiteral("refs/remotes/");
  static const QString tags = QStringLiteral("refs/tags/");

  if (ref.startsWith(heads)) {
    return {ref.mid(heads.size()), QColor(0x2E7D32), Qt::white};
  }

  if (ref.startsWith(remotes)) {
    return {ref.mid(remotes.size()), QColor(0x546E7A), Qt::white};
  }

  if (ref.startsWith(tags)) {
    return {ref.mid(tags.size()), QColor(0xF9A825), QColor(0x212121)};
  }

  return {ref, QColor(0x546E7A), Qt::white};
}

// Badges of one row, placed at x positions on lines.
struct BadgeLayout {
  QVector<Badge> badges;
  QVector<int> widths;
  QVector<QPoint> positions; // x and line
  int lines = 0;
  int startX = 0;
  int badgeHeight = 0;
};

// Wraps the badges into at most maxLines lines. The badges that do not fit
// are replaced by a trailing "+N" badge.
BadgeLayout layoutBadges(const QStringList &refs, const QFontMetrics &metrics,
                         const int startX, const int maxRight,
                         const int maxLines) {
  BadgeLayout layout;
  layout.startX = startX;

  QVector<Badge> badges;
  QVector<int> widths;
  for (const QString &ref : refs) {
    Badge badge = makeBadge(ref);
    badge.text =
        metrics.elidedText(badge.text, Qt::ElideRight, BadgeMaxTextWidth);
    widths.push_back(metrics.horizontalAdvance(badge.text) + 2 * BadgePaddingX);
    badges.push_back(badge);
  }

  int x = startX;
  int line = 0;
  int shown = 0;
  for (; shown < badges.size(); ++shown) {
    int badgeX = x;
    int badgeLine = line;
    if (badgeX > startX && badgeX + widths[shown] > maxRight) {
      badgeX = startX;
      ++badgeLine;
    }

    if (badgeLine >= maxLines) {
      break;
    }

    x = badgeX;
    line = badgeLine;
    layout.positions.push_back(QPoint(x, line));
    x += widths[shown] + BadgeGap;
  }

  Badge more;
  int moreWidth = 0;
  if (shown < badges.size()) {
    const auto makeMore = [&]() {
      more = {QStringLiteral("+%1").arg(badges.size() - shown),
              QColor(0x757575), Qt::white};
      moreWidth = metrics.horizontalAdvance(more.text) + 2 * BadgePaddingX;
    };
    makeMore();

    // Make room for it on the last line by dropping trailing badges.
    const auto moreX = [&]() {
      const int last = layout.positions.size() - 1;
      if (last < 0 || layout.positions[last].y() != line) {
        return startX;
      }
      return layout.positions[last].x() + widths[last] + BadgeGap;
    };
    while (shown > 1 && !layout.positions.isEmpty() &&
           layout.positions.last().y() == line &&
           moreX() + moreWidth > maxRight) {
      layout.positions.pop_back();
      --shown;
      makeMore();
    }

    layout.positions.push_back(QPoint(moreX(), line));
  }

  badges.resize(shown);
  widths.resize(shown);
  if (!layout.positions.isEmpty() && layout.positions.size() > shown) {
    badges.push_back(more);
    widths.push_back(moreWidth);
  }

  layout.badges = badges;
  layout.widths = widths;
  layout.lines = layout.positions.isEmpty() ? 0 : line + 1;
  return layout;
}

} // namespace

RefBadgesOverlay::RefBadgesOverlay(QTableView *view)
    : QWidget(view->viewport()), _view(view) {
  setAttribute(Qt::WA_TransparentForMouseEvents, true);
  setAttribute(Qt::WA_NoSystemBackground, true);
  setAttribute(Qt::WA_StyledBackground, false);
  setAutoFillBackground(false);
  setFocusPolicy(Qt::NoFocus);
  setGeometry(view->viewport()->rect());

  view->viewport()->installEventFilter(this);

  // Scrolling moves rows under a still cursor without any mouse event. The
  // view also scrolls its viewport together with its child widgets, so the
  // overlay has to be put back over the viewport every time.
  const auto onScroll = [this]() {
    refresh();
    refreshHoveredRow();
  };
  connect(view->verticalScrollBar(), &QScrollBar::valueChanged, this, onScroll);
  connect(view->horizontalScrollBar(), &QScrollBar::valueChanged, this,
          onScroll);

  _requestTimer.setSingleShot(true);
  _requestTimer.setInterval(RequestDelayMs);
  connect(&_requestTimer, &QTimer::timeout, this,
          [this]() { requestExtraRefs(); });

  raise();
  show();
}

void RefBadgesOverlay::attach() {
  if (QAbstractItemModel *model = _view->model()) {
    connect(model, &QAbstractItemModel::modelReset, this, [this]() {
      clearExtraRefs();
      refresh();
    });

    const auto onChange = [this]() { refresh(); };
    connect(model, &QAbstractItemModel::layoutChanged, this, onChange);
    connect(model, &QAbstractItemModel::rowsInserted, this, onChange);
    connect(model, &QAbstractItemModel::rowsRemoved, this, onChange);
    connect(model, &QAbstractItemModel::dataChanged, this, onChange);
  }

  if (QHeaderView *header = _view->horizontalHeader()) {
    const auto onChange = [this]() { refresh(); };
    connect(header, &QHeaderView::sectionResized, this, onChange);
    connect(header, &QHeaderView::sectionCountChanged, this, onChange);
  }

  refresh();
}

void RefBadgesOverlay::refresh() {
  setGeometry(_view->viewport()->rect());
  update();
}

void RefBadgesOverlay::setHoveredIndex(const QModelIndex &index) {
  // Only the graph column shows the extra badges.
  const QModelIndex first =
      index.isValid() && index.column() == COLUMN_HISTORY_SLICE ? index
                                                                : QModelIndex();

  if (_index.isValid() == first.isValid() &&
      (!first.isValid() || _index.row() == first.row())) {
    return;
  }

  _index = first;

  _requestTimer.stop();
  if (_index.isValid()) {
    _requestTimer.start();
  }

  update();
}

void RefBadgesOverlay::setRefsProvider(RefsProvider provider) {
  _refsProvider = std::move(provider);
}

void RefBadgesOverlay::setExtraRefs(const QString &commit,
                                    const QStringList &refs) {
  _extraRefs.insert(commit, refs);

  if (!refs.isEmpty() && commit == hoveredCommit()) {
    update();
  }
}

void RefBadgesOverlay::clearExtraRefs() {
  _extraRefs.clear();
  _requested.clear();
}

QString RefBadgesOverlay::hoveredCommit() const {
  if (!_index.isValid()) {
    return {};
  }

  return _index.sibling(_index.row(), COLUMN_COMMIT_FULL_SHA)
      .data(Qt::DisplayRole)
      .toString();
}

void RefBadgesOverlay::requestExtraRefs() {
  const QString commit = hoveredCommit();
  if (commit.isEmpty() || !_refsProvider || _requested.contains(commit)) {
    return;
  }

  _requested.insert(commit);
  _refsProvider(commit);
}

void RefBadgesOverlay::clearIfCursorOutside() {
  if (!_view->viewport()->rect().contains(
          _view->viewport()->mapFromGlobal(QCursor::pos()))) {
    setHoveredIndex({});
  }
}

void RefBadgesOverlay::refreshHoveredRow() {
  if (!_index.isValid()) {
    return;
  }

  const QPoint pos = _view->viewport()->mapFromGlobal(QCursor::pos());
  if (!_view->viewport()->rect().contains(pos)) {
    setHoveredIndex({});
    return;
  }

  setHoveredIndex(_view->indexAt(pos));
}

bool RefBadgesOverlay::eventFilter(QObject *watched, QEvent *event) {
  if (watched == parentWidget() && event->type() == QEvent::Resize) {
    setGeometry(parentWidget()->rect());
  }

  return QWidget::eventFilter(watched, event);
}

void RefBadgesOverlay::paintEvent(QPaintEvent *) {
  const QAbstractItemModel *model = _view->model();
  if (!model || _view->isColumnHidden(COLUMN_HISTORY_SLICE)) {
    return;
  }

  int row = _view->rowAt(0);
  if (row < 0) {
    return;
  }

  QFont badgeFont = _view->font();
  if (badgeFont.pointSizeF() > 0) {
    badgeFont.setPointSizeF(badgeFont.pointSizeF() * 0.8);
  } else if (badgeFont.pixelSize() > 0) {
    badgeFont.setPixelSize(std::max(1, badgeFont.pixelSize() * 4 / 5));
  }
  const QFontMetrics metrics(badgeFont);

  const int maxRight = width() - BadgeRightMargin;

  // Layout of the badges of a row; the first line is centered on the row.
  struct RowLayout {
    BadgeLayout layout;
    int top = 0;
  };
  const auto layoutRow = [&](const QModelIndex &index, const QStringList &refs,
                             const int maxLines) -> RowLayout {
    RowLayout result;
    const QRect cell = _view->visualRect(index);
    if (refs.isEmpty() || !cell.isValid() || cell.isEmpty()) {
      return result;
    }

    const int laneCount = index.data(HistorySliceWidthRole).toInt();
    const int laneWidth = Delegates::historyLaneWidth(cell, laneCount);
    const auto graphRow = index.data(GraphRowRole).value<Util::GraphRow>();
    const int startX = cell.left() +
                       static_cast<int>(graphRow.size()) * laneWidth +
                       BadgeLeadingGap;

    result.layout = layoutBadges(refs, metrics, startX, maxRight, maxLines);
    result.layout.badgeHeight =
        std::max(8, std::min(cell.height() - 6, metrics.height() + 2));
    result.top = cell.top() + (cell.height() - result.layout.badgeHeight) / 2;
    return result;
  };

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setFont(badgeFont);

  const auto drawLayout = [&](const RowLayout &row, const int firstTop) {
    const BadgeLayout &layout = row.layout;
    const int lineStep = layout.badgeHeight + BadgeLineGap;
    for (int i = 0; i < layout.badges.size(); ++i) {
      const Badge &badge = layout.badges[i];
      const QRectF rect(layout.positions[i].x() + 0.5,
                        firstTop + layout.positions[i].y() * lineStep + 0.5,
                        layout.widths[i] - 1, layout.badgeHeight - 1);
      // Keycap look: a solid darker base peeks out below a flat face with a
      // light bevel on top.
      constexpr qreal depth = 2.0;
      const qreal radius = std::min<qreal>(4.0, layout.badgeHeight / 2.0);
      const QRectF face = rect.adjusted(0, 0, 0, -depth);

      // Soft shadow beneath the badge.
      painter.setPen(Qt::NoPen);
      painter.setBrush(QColor(0, 0, 0, 75));
      painter.drawRoundedRect(rect.adjusted(-0.5, 0, 0.5, 0).translated(0, 2),
                              radius + 0.5, radius + 0.5);
      painter.setBrush(QColor(0, 0, 0, 120));
      painter.drawRoundedRect(rect.translated(0, 1), radius, radius);

      painter.setPen(QPen(badge.fill.darker(190), 1));
      painter.setBrush(badge.fill.darker(165));
      painter.drawRoundedRect(rect, radius, radius);

      painter.setPen(QPen(badge.fill.lighter(150), 1));
      painter.setBrush(badge.fill);
      painter.drawRoundedRect(face, radius, radius);

      // Inner shade along the bottom of the face.
      painter.setPen(QPen(badge.fill.darker(120), 1));
      painter.drawLine(QPointF(face.left() + radius, face.bottom() - 1),
                       QPointF(face.right() - radius, face.bottom() - 1));

      painter.setPen(badge.textColor);
      painter.drawText(face, Qt::AlignCenter, badge.text);
    }
  };

  // The hovered row also shows the branches containing its commit, wrapped
  // into two lines. The second line goes over the next row, whose own badges
  // are skipped then.
  const int hoveredRow = _index.isValid() ? _index.row() : -1;
  RowLayout hovered;
  int hoveredTop = 0;
  if (hoveredRow >= 0) {
    QStringList refs = _index.data(RefBadgesRole).toStringList();
    for (const QString &ref : _extraRefs.value(hoveredCommit())) {
      if (!refs.contains(ref)) {
        refs.push_back(ref);
      }
    }

    hovered = layoutRow(_index, refs, HoverMaxLines);
    if (hovered.layout.lines > 0) {
      const int lineStep = hovered.layout.badgeHeight + BadgeLineGap;
      const int blockHeight = hovered.layout.lines * lineStep - BadgeLineGap;
      hoveredTop = hovered.top;
      if (hoveredTop + blockHeight > height() - BadgeLineGap) {
        // Keep the last line inside the viewport.
        hoveredTop = std::max(0, height() - BadgeLineGap - blockHeight);
      }
    }
  }
  const int skippedRow = hovered.layout.lines > 1 ? hoveredRow + 1 : -1;

  for (; row < model->rowCount(); ++row) {
    const QModelIndex index = model->index(row, COLUMN_HISTORY_SLICE);
    if (_view->visualRect(index).top() >= height()) {
      break;
    }

    if (row == hoveredRow || row == skippedRow) {
      continue;
    }

    const RowLayout rowLayout =
        layoutRow(index, index.data(RefBadgesRole).toStringList(), 1);
    drawLayout(rowLayout, rowLayout.top);
  }

  if (hovered.layout.lines > 0) {
    drawLayout(hovered, hoveredTop);
  }
}

} // namespace GuiManager::Widgets
