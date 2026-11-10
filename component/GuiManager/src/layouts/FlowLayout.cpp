// MIT License
// Copyright (c) 2026 Sourcerer

#include "layouts/FlowLayout.h"

namespace GuiManager::Layouts {

FlowLayout::FlowLayout(QWidget *parent, FlowLayoutDirectionType direction,
                       const int margin, const int hSpacing, const int vSpacing)
    : QLayout(parent), _hSpace(hSpacing), _vSpace(vSpacing),
      _direction(direction) {
  setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::FlowLayout(const int margin, const int hSpacing, const int vSpacing)
    : _hSpace(hSpacing), _vSpace(vSpacing) {
  setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::~FlowLayout() {
  QLayoutItem *item;
  while ((item = takeAt(0)))
    delete item;
}

void FlowLayout::addItem(QLayoutItem *item) {
  _itemList.append(item);
}

int FlowLayout::horizontalSpacing() const {
  if (_hSpace >= 0) {
    return _hSpace;
  } else {
    return smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
  }
}

int FlowLayout::verticalSpacing() const {
  if (_vSpace >= 0) {
    return _vSpace;
  } else {
    return smartSpacing(QStyle::PM_LayoutVerticalSpacing);
  }
}

int FlowLayout::count() const {
  return _itemList.size();
}

QLayoutItem *FlowLayout::itemAt(const int index) const {
  return _itemList.value(index);
}

QLayoutItem *FlowLayout::takeAt(const int index) {
  if (index >= 0 && index < _itemList.size())
    return _itemList.takeAt(index);
  return nullptr;
}

int FlowLayout::doLayout(const QRect &rect, bool testOnly) const {
  if (_direction == FlowLayoutDirectionType::Horizontal) {
    return doLayoutHorizontalFirst(rect, testOnly);
  }

  return doLayoutVerticalFirst(rect, testOnly);
}

Qt::Orientations FlowLayout::expandingDirections() const {
  return {};
}

bool FlowLayout::hasHeightForWidth() const {
  return true;
}

int FlowLayout::heightForWidth(const int width) const {
  const int height = doLayout(QRect(0, 0, width, 0), true);
  return height;
}

void FlowLayout::setGeometry(const QRect &rect) {
  QLayout::setGeometry(rect);
  doLayout(rect, false);
}

QSize FlowLayout::sizeHint() const {
  return minimumSize();
}

QSize FlowLayout::minimumSize() const {
  QSize size;
  for (const QLayoutItem *item : std::as_const(_itemList)) {
    if (item->isEmpty())
      continue;
    size = size.expandedTo(item->minimumSize());
  }

  const QMargins margins = contentsMargins();
  size +=
      QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
  return size;
}

int FlowLayout::doLayoutHorizontalFirst(const QRect &rect,
                                        const bool testOnly) const {
  int x = rect.x();
  int y = rect.y();
  const int maxWidth = rect.width();
  int lineHeight = 0;

  for (int i = 0; i < _itemList.size(); ++i) {
    QLayoutItem *item = _itemList.at(i);
    // Hidden widgets take no space, so they must not reserve a slot either.
    if (item->isEmpty())
      continue;

    QSize itemSize = item->sizeHint();

    int spaceX = horizontalSpacing();
    int spaceY = verticalSpacing();

    if (spaceX == -1)
      spaceX = smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
    if (spaceY == -1)
      spaceY = smartSpacing(QStyle::PM_LayoutVerticalSpacing);

    // Wrap to next line if widget doesn't fit
    if (x + itemSize.width() > rect.x() + maxWidth && lineHeight > 0) {
      x = rect.x();
      y += lineHeight + spaceY;
      lineHeight = 0;
    }

    if (!testOnly) {
      QRect itemRect(QPoint(x, y), itemSize);
      item->setGeometry(itemRect);
    }

    x += itemSize.width() + spaceX;
    lineHeight = qMax(lineHeight, itemSize.height());
  }

  return y + lineHeight - rect.y();
}

int FlowLayout::smartSpacing(QStyle::PixelMetric pm) const {
  if (QObject *parent = this->parent(); !parent) {
    return -1;
  } else if (parent->isWidgetType()) {
    const QWidget *pw = static_cast<QWidget *>(parent);
    return pw->style()->pixelMetric(pm, nullptr, pw);
  } else {
    return static_cast<QLayout *>(parent)->spacing();
  }
}

int FlowLayout::doLayoutVerticalFirst(const QRect &rect, bool testOnly) const {
  int x = rect.x();
  int y = rect.y();
  int lineHeight = 0;

  for (int i = 0; i < _itemList.size(); ++i) {
    QLayoutItem *item = _itemList.at(i);
    if (item->isEmpty())
      continue;

    QSize itemSize = item->sizeHint();
    int spaceX = horizontalSpacing();
    int spaceY = verticalSpacing();

    if (spaceX == -1)
      spaceX = smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
    if (spaceY == -1)
      spaceY = smartSpacing(QStyle::PM_LayoutVerticalSpacing);

    int nextX = x + itemSize.width() + spaceX;
    if (nextX - spaceX > rect.right() && lineHeight > 0) {
      x = rect.x();
      y += lineHeight + spaceY;
      nextX = x + itemSize.width() + spaceX;
      lineHeight = 0;
    }

    if (!testOnly)
      item->setGeometry(QRect(QPoint(x, y), itemSize));

    x = nextX;
    lineHeight = qMax(lineHeight, itemSize.height());
  }

  return y + lineHeight - rect.y();
}

} // namespace GuiManager::Layouts