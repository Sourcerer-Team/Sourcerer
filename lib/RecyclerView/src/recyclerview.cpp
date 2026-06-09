#include "recyclerview.h"

#include <QScrollBar>

RecyclerView::RecyclerView(QWidget *parent)
    : QAbstractScrollArea(parent), _adapter(nullptr), _itemHeight(-1) {
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}

RecyclerView::RecyclerView(RecyclerViewAdapter *adapter, QWidget *parent)
    : QAbstractScrollArea(parent) {
  setAdapter(adapter);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}

void RecyclerView::setAdapter(RecyclerViewAdapter *adapter) {
  this->_adapter = adapter;

  if (adapter) {
    this->_adapter->setParent(this);
    calculateItemHeight();
  }
}

void RecyclerView::calculateItemHeight() {
  Q_ASSERT(_adapter != nullptr);

  if (!_adapter) {
    return;
  }

  const ViewHolder *vh = _adapter->createViewHolder();

  this->_itemHeight = vh->getItemView()->height();

  delete vh;

  verticalScrollBar()->setSingleStep(this->_itemHeight / 4);

  _pool.maxSize = viewport()->height() / this->_itemHeight + _numExtraActive;
}

void RecyclerView::populateItems() {
  populateItemsBelow(0);
}

void RecyclerView::transformItems(
    const std::function<void(ViewHolder *)> &func) const {
  for (auto it = _adapter->_activeViewMap.begin();
       it != _adapter->_activeViewMap.end(); ++it) {
    func(it.value());
  }
}

int RecyclerView::totalItemHeight() const {
  return _adapter->model()->rowCount() * _itemHeight;
}

int RecyclerView::getItemPos(const int dataPos) const {
  return viewport()->rect().top() - verticalScrollBar()->value() +
         _itemHeight * dataPos;
}

ViewHolder *RecyclerView::populateItem(const int dataPos, int y) {
  ViewHolder *vh = _pool.getRecycledView();
  if (vh == nullptr)
    vh = _adapter->createViewHolder();

  QWidget *view = vh->getItemView();
  view->setParent(viewport());
  _adapter->bindViewHolder(vh, dataPos);

  view->resize(viewport()->width(), _itemHeight);
  view->move(viewport()->rect().left(), y);
  view->show();

  return vh;
}

// TODO: refactor the following into one function that works both ways
void RecyclerView::populateItemsBelow(const int startDataPos) {
  int bottom = viewport()->rect().bottom();

  if (getItemPos(startDataPos) > bottom || startDataPos < 0)
    return;

  for (int i = startDataPos, numExtra = 0; i < _adapter->model()->rowCount();
       i++) {
    int newY = getItemPos(i);

    if (newY > bottom) {
      if (numExtra == _numExtraActive) {
        break;
      }
      numExtra++;
    }
    populateItem(i, newY);
  }
}
void RecyclerView::populateItemsAbove(int startDataPos) {
  int top = viewport()->rect().top();
  if (getItemPos(startDataPos) + _itemHeight < top || startDataPos < 0)
    return;

  for (int i = startDataPos, numExtra = 0; i >= 0; i--) {
    int newY = getItemPos(i);

    if (newY + _itemHeight < top) {
      if (numExtra == _numExtraActive) {
        break;
      }
      numExtra++;
    }
    populateItem(i, newY);
  }
}

void RecyclerView::resizeEvent(QResizeEvent *event) {
  verticalScrollBar()->setPageStep(viewport()->height());
  verticalScrollBar()->setRange(0, totalItemHeight() - viewport()->height());
  _pool.maxSize = viewport()->height() / _itemHeight + _numExtraActive;

  for (auto it = _adapter->_activeViewMap.begin();
       it != _adapter->_activeViewMap.end(); it++) {
    it.value()->getItemView()->resize(viewport()->width(), _itemHeight);
  }

  if (_adapter->_activeViewMap.isEmpty()) {
    int startDataPos = verticalScrollBar()->value() / _itemHeight;
    populateItemsBelow(startDataPos);

    // Adjust our minWidth to that of an item if this is the first item we have
    // created
    if (!_adapter->_activeViewMap.isEmpty()) {
      QWidget *widget = _adapter->_activeViewMap.first()->getItemView();
      if (widget->minimumSizeHint().isValid()) {
        setMinimumWidth(widget->minimumSizeHint().width() +
                        verticalScrollBar()->sizeHint().width());
      } else {
        setMinimumWidth(widget->minimumWidth() +
                        verticalScrollBar()->sizeHint().width());
      }
    }

  } else if (_adapter->_activeViewMap.last()->getItemView()->rect().bottom() <
             viewport()->rect().bottom()) {
    populateItemsBelow(_adapter->_activeViewMap.lastKey() + 1);
  }
}

void RecyclerView::scrollContentsBy(const int dx, const int dy) {
  int vvalue = verticalScrollBar()->value();

  // Special case for when we "jump" positions in the scrollbar
  if (qAbs(dy) > _adapter->_activeViewMap.size() * _itemHeight) {
    // Recycle everything
    for (auto it = _adapter->_activeViewMap.begin();
         it != _adapter->_activeViewMap.end(); it++)
      _pool.putRecycledView(it.value());
    _adapter->_activeViewMap.clear();

    const int startDataPos = vvalue / _itemHeight;
    populateItemsBelow(startDataPos);
    return;
  }

  viewport()->scroll(dx, dy);

  // Recycle views out of sight
  for (auto it = _adapter->_activeViewMap.begin();
       it != _adapter->_activeViewMap.end();) {
    if (!viewport()->rect().intersects(it.value()->getItemView()->geometry())) {
      _pool.putRecycledView(_adapter->recycleViewHolder(it.value()));
      it = _adapter->_activeViewMap.erase(it);
    } else {
      it++;
    }
  }

  if (_adapter->_activeViewMap.isEmpty())
    return;

  if (_adapter->_activeViewMap.first()->getItemView()->y() >
      viewport()->rect().top()) {
    populateItemsAbove(_adapter->_activeViewMap.firstKey() - 1);
  } else if (_adapter->_activeViewMap.last()->getItemView()->rect().bottom() <
             viewport()->rect().bottom()) {
    populateItemsBelow(_adapter->_activeViewMap.lastKey() + 1);
  }
}
