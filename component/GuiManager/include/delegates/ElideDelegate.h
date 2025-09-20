#pragma once

#include <QStyledItemDelegate>

namespace GuiManager::Delegates {

class ElideDelegate final : public QStyledItemDelegate {
public:
  explicit ElideDelegate(Qt::TextElideMode mode = Qt::ElideRight,
                         QObject *parent = nullptr)
      : QStyledItemDelegate(parent), elideMode(mode) {}

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    opt.text =
        opt.fontMetrics.elidedText(opt.text, elideMode, opt.rect.width());
    QStyledItemDelegate::paint(painter, opt, index);
  }

private:
  Qt::TextElideMode elideMode;
};

} // namespace GuiManager::Delegates
