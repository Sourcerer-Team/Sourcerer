#include "styledrecyclerlistitem.h"
#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QStyleOption>

StyledRecyclerListItem::StyledRecyclerListItem(QWidget *parent)
    : QWidget{parent} {
  setAutoFillBackground(true);
}

void StyledRecyclerListItem::enterEvent(QEnterEvent *event) {
  QWidget::enterEvent(event);

  QPalette pal = palette();
  previousColor = pal.color(backgroundRole());
  pal.setColor(backgroundRole(), previousColor.darker(110));
  setPalette(pal);
}

void StyledRecyclerListItem::leaveEvent(QEvent *event) {
  QWidget::leaveEvent(event);
  QPalette pal = palette();
  pal.setColor(backgroundRole(), previousColor);
  setPalette(pal);
}

void StyledRecyclerListItem::paintEvent(QPaintEvent *event) {
  QStyleOption opt;
  opt.initFrom(this);
  QPainter p(this);
  style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);

  // qDebug() << "ITEM PAINT EVENT" << event->rect();
  // p.fillRect(event->rect(), Qt::red);
}
