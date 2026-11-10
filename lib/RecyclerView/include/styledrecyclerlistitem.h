// MIT License
// Copyright (c) 2026 Sourcerer

#ifndef STYLEDRECYCLERLISTITEM_H
#define STYLEDRECYCLERLISTITEM_H

#include <QColor>
#include <QWidget>

class StyledRecyclerListItem : public QWidget {
  Q_OBJECT
public:
  explicit StyledRecyclerListItem(QWidget *parent = nullptr);

protected:
  void paintEvent(QPaintEvent *) override;
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;

private:
  QColor previousColor;
};

#endif // STYLEDRECYCLERLISTITEM_H
