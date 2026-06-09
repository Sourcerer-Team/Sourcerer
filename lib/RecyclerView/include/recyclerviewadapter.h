#ifndef RECYCLERVIEWADAPTER_H
#define RECYCLERVIEWADAPTER_H

#include "viewholder.h"

#include <QAbstractItemModel>
#include <QMap>
#include <QObject>

class RecyclerViewAdapter : public QObject {
  Q_OBJECT
public:
  explicit RecyclerViewAdapter(QAbstractItemModel *model,
                               QObject *parent = nullptr);

  virtual ViewHolder *createViewHolder() = 0;
  virtual void onBindViewHolder(ViewHolder *vh, int dataPos) = 0;
  virtual void onRecycleViewHolder(ViewHolder *vh) const {}

  QAbstractItemModel *model() const { return _dataModel; }

protected:
  QAbstractItemModel *_dataModel;
  ViewHolder *findViewHolder(int dataPos) const;

private slots:
  void modelReset();

private:
  void bindViewHolder(ViewHolder *vh, int dataPos);
  ViewHolder *recycleViewHolder(ViewHolder *vh) const;

  QMap<int, ViewHolder *> _activeViewMap;

  friend class RecyclerView;
};

#endif // RECYCLERVIEWADAPTER_H
