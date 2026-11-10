// MIT License
// Copyright (c) 2026 Sourcerer

#ifndef VIEWHOLDER_H
#define VIEWHOLDER_H

#include <QObject>
#include <QPointer>
#include <QWidget>

class ViewHolder : public QObject {
  Q_OBJECT
public:
  explicit ViewHolder(QObject *parent, QWidget *view);
  ~ViewHolder() override;

  QWidget *getItemView() const { return _itemView; }
  int _dataPos;

private:
  QPointer<QWidget> _itemView;
};

#endif // VIEWHOLDER_H
