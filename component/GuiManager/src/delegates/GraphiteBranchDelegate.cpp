#include "delegates/GraphiteBranchDelegate.h"
#include "models/BranchesModel.h"

#include <GraphiteUtils.h>
#include <StylesheetManager.h>

#include <QApplication>
#include <QPainter>
#include <QPainterPath>

namespace GuiManager::Delegates {

namespace {

constexpr int kLeftMargin = 4;
constexpr int kTextGap = 6;
constexpr int kBadgeGap = 4;
constexpr int kBadgePadding = 5;
constexpr qreal kLineWidth = 1.5;

QColor laneColor(int level) {
  const QVector<QColor> palette =
      StylesheetManager::StylesheetManager::graphLaneColors();
  if (palette.isEmpty()) {
    return QColor(Qt::gray);
  }
  return palette.at(level % palette.size());
}

QColor pullRequestColor(const GitRepo::GraphitePullRequest &pullRequest) {
  if (pullRequest.state == QStringLiteral("MERGED")) {
    return QColor(0x82, 0x50, 0xdf);
  }
  if (pullRequest.state == QStringLiteral("CLOSED")) {
    return QColor(0xcf, 0x22, 0x2e);
  }
  if (pullRequest.draft) {
    return QColor(0x6e, 0x77, 0x81);
  }
  return QColor(0x1a, 0x7f, 0x37);
}

struct Badge {
  QString text;
  QColor fill;
  QColor textColor;
};

} // namespace

GraphiteBranchDelegate::GraphiteBranchDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {}

QSize GraphiteBranchDelegate::sizeHint(const QStyleOptionViewItem &option,
                                       const QModelIndex &index) const {
  QSize size = QStyledItemDelegate::sizeHint(option, index);
  size.setHeight(qMax(size.height(), option.fontMetrics.height() + 8));
  return size;
}

void GraphiteBranchDelegate::paint(QPainter *painter,
                                   const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const {
  const QVariant rowData = index.data(Models::BranchesModel::GraphiteRowRole);
  if (!rowData.isValid()) {
    QStyledItemDelegate::paint(painter, option, index);
    return;
  }

  const auto row = rowData.value<GitRepo::GraphiteBranchRow>();

  QStyleOptionViewItem opt = option;
  initStyleOption(&opt, index);
  const QString text = opt.text;
  opt.text.clear();
  opt.icon = QIcon();
  if (row.kind == GitRepo::GraphiteBranchRow::Kind::UntrackedHeader) {
    opt.state &= ~(QStyle::State_Selected | QStyle::State_MouseOver);
  }

  const QWidget *widget = opt.widget;
  QStyle *style = widget ? widget->style() : QApplication::style();
  style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);

  const QRect rect = opt.rect;
  const bool selected = opt.state & QStyle::State_Selected;
  const QColor textColor =
      opt.palette.color(selected ? QPalette::HighlightedText : QPalette::Text);

  if (row.kind == GitRepo::GraphiteBranchRow::Kind::UntrackedHeader) {
    QFont font = opt.font;
    font.setPointSizeF(font.pointSizeF() * 0.9);
    painter->setFont(font);
    QColor muted = opt.palette.color(QPalette::PlaceholderText);
    painter->setPen(muted);
    const QRect textRect = rect.adjusted(kLeftMargin, 0, -kLeftMargin, 0);
    const QString label =
        QFontMetrics(font).elidedText(text, Qt::ElideRight, textRect.width());
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, label);
    const int lineStart = textRect.left() +
                          QFontMetrics(font).horizontalAdvance(label) +
                          kTextGap;
    if (lineStart < textRect.right()) {
      muted.setAlphaF(0.5);
      painter->setPen(QPen(muted, 1));
      painter->drawLine(lineStart, rect.center().y(), textRect.right(),
                        rect.center().y());
    }
    painter->restore();
    return;
  }

  const int unit = qMax(12, opt.fontMetrics.height());
  const qreal radius = unit * 0.25;
  const auto columnX = [&](int column) {
    return rect.left() + kLeftMargin + column * unit + unit / 2.0;
  };
  const qreal centerY = rect.top() + rect.height() / 2.0;
  const qreal top = rect.top();
  const qreal bottom = rect.bottom() + 1;

  const auto drawLine = [&](const QColor &color, QPointF from, QPointF to) {
    painter->setPen(QPen(color, kLineWidth, Qt::SolidLine, Qt::FlatCap));
    painter->drawLine(from, to);
  };

  const bool active = index.data(Models::BranchesModel::ActiveRole).toBool();
  qreal textLeft = 0;

  if (row.tracked) {
    for (int column = 0; column < row.throughLines.size(); ++column) {
      if (row.throughLines.at(column)) {
        drawLine(laneColor(column + 1), {columnX(column), top},
                 {columnX(column), bottom});
      }
    }

    const QColor color = laneColor(row.depth);
    const qreal dotX = columnX(row.depth);
    if (row.depth > 0) {
      const qreal elbowX = columnX(row.depth - 1);
      drawLine(color, {elbowX, top}, {elbowX, centerY});
      if (!row.lastChild) {
        drawLine(color, {elbowX, centerY}, {elbowX, bottom});
      }
      drawLine(color, {elbowX, centerY}, {dotX - radius, centerY});
    }
    if (row.hasChildren) {
      drawLine(laneColor(row.depth + 1), {dotX, centerY + radius},
               {dotX, bottom});
    }

    painter->setPen(QPen(color, kLineWidth));
    painter->setBrush(active ? QBrush(color)
                             : QBrush(opt.palette.color(QPalette::Base)));
    painter->drawEllipse(QPointF(dotX, centerY), radius, radius);
    textLeft = dotX + radius + kTextGap;
  } else {
    QColor muted = opt.palette.color(QPalette::PlaceholderText);
    painter->setPen(QPen(muted, kLineWidth));
    painter->setBrush(active ? QBrush(muted) : Qt::NoBrush);
    painter->drawEllipse(QPointF(columnX(0), centerY), radius * 0.8,
                         radius * 0.8);
    textLeft = columnX(0) + radius + kTextGap;
  }

  // Badges, right aligned.
  QVector<Badge> badges;
  const auto branch = index.data(Models::BranchesModel::GraphiteBranchRole)
                          .value<GitRepo::GraphiteBranch>();
  if (branch.needsRestack) {
    badges.append({tr("restack"), QColor(0xbf, 0x87, 0x00), Qt::white});
  }
  if (branch.pullRequest.number > 0) {
    QString label = QStringLiteral("#%1").arg(branch.pullRequest.number);
    if (branch.pullRequest.state == QStringLiteral("MERGED")) {
      label += QLatin1Char(' ') + tr("merged");
    } else if (branch.pullRequest.state == QStringLiteral("CLOSED")) {
      label += QLatin1Char(' ') + tr("closed");
    } else if (branch.pullRequest.draft) {
      label += QLatin1Char(' ') + tr("draft");
    }
    badges.append({label, pullRequestColor(branch.pullRequest), Qt::white});
  }

  QFont badgeFont = opt.font;
  badgeFont.setPointSizeF(badgeFont.pointSizeF() * 0.85);
  const QFontMetrics badgeMetrics(badgeFont);
  const int badgeHeight = badgeMetrics.height() + 2;
  qreal right = rect.right() - kLeftMargin;
  painter->setFont(badgeFont);
  for (const Badge &badge : std::as_const(badges)) {
    const int width =
        badgeMetrics.horizontalAdvance(badge.text) + 2 * kBadgePadding;
    if (right - width < textLeft + unit) {
      break;
    }
    const QRectF badgeRect(right - width, centerY - badgeHeight / 2.0, width,
                           badgeHeight);
    QPainterPath path;
    path.addRoundedRect(badgeRect, badgeHeight / 2.0, badgeHeight / 2.0);
    painter->fillPath(path, badge.fill);
    painter->setPen(badge.textColor);
    painter->drawText(badgeRect, Qt::AlignCenter, badge.text);
    right -= width + kBadgeGap;
  }

  QFont nameFont = opt.font;
  nameFont.setBold(active);
  painter->setFont(nameFont);
  painter->setPen(textColor);
  const QRectF textRect(textLeft, rect.top(), qMax(0.0, right - textLeft),
                        rect.height());
  painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(nameFont).elidedText(text, Qt::ElideMiddle,
                                                      int(textRect.width())));

  painter->restore();
}

} // namespace GuiManager::Delegates
