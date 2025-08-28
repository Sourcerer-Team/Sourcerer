#ifndef RECYCLERVIEW_H
#define RECYCLERVIEW_H

#include "recyclerviewadapter.h"
#include "recyclerviewpool.h"

#include <QAbstractScrollArea>
#include <QVector>

class RecyclerView : public QAbstractScrollArea {
  Q_OBJECT

public:
  explicit RecyclerView(QWidget *parent = nullptr);
  explicit RecyclerView(RecyclerViewAdapter *adapter = nullptr,
                        QWidget *parent = nullptr);

  void setAdapter(RecyclerViewAdapter *adapter);

  void populateItems();

protected:
  void resizeEvent(QResizeEvent *event) override;
  void scrollContentsBy(int dx, int dy) override;

private:
  ViewHolder *populateItem(int dataPos, int y);
  void populateItemsBelow(int startDataPos);
  void populateItemsAbove(int startDataPos);

  int getItemPos(int dataPos) const;
  int totalItemHeight() const;

  void calculateItemHeight();

  RecyclerViewPool _pool;
  RecyclerViewAdapter *_adapter;

  const int _numExtraActive = 0;
  int _itemHeight;
};

#endif // RECYCLERVIEW_H
