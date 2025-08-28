#include "recyclerviewadapter.h"
#include "recyclerview.h"

#include <QScrollBar>

RecyclerViewAdapter::RecyclerViewAdapter(QAbstractItemModel *dataModel,
                                         QObject *parent)
    : QObject{parent} {
  Q_ASSERT(dataModel != nullptr);
  this->_dataModel = dataModel;

  connect(this->_dataModel, &QAbstractItemModel::modelReset, this,
          &RecyclerViewAdapter::modelReset);
}

ViewHolder *RecyclerViewAdapter::recycleViewHolder(ViewHolder *vh) const {
  Q_ASSERT(vh != nullptr);

  onRecycleViewHolder(vh);
  vh->getItemView()->hide();
  return vh;
}

void RecyclerViewAdapter::bindViewHolder(ViewHolder *vh, const int dataPos) {
  Q_ASSERT(vh != nullptr);

  vh->getItemView()->setBackgroundRole(QPalette::Window);

  onBindViewHolder(vh, dataPos);
  _activeViewMap.insert(dataPos, vh);
}

ViewHolder *RecyclerViewAdapter::findViewHolder(const int dataPos) const {
  return _activeViewMap.value(dataPos, nullptr);
}

void RecyclerViewAdapter::modelReset() {
  _activeViewMap.clear();
  if (const auto rv = qobject_cast<RecyclerView *>(parent())) {
    rv->populateItems();
  }
}
