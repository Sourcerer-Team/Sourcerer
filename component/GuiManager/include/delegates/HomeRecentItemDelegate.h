#pragma once

#include <QModelIndex>
#include <QObject>
#include <QStyledItemDelegate>

namespace GuiManager::Delegates {

class HomeRecentItemDelegate : public QStyledItemDelegate {
  Q_OBJECT
public:
  explicit HomeRecentItemDelegate(QObject *parent = nullptr);

  void setHover(const QModelIndex &index);

  // QAbstractItemDelegate interface
public:
  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;

private:
  QModelIndex _indexHovered;
};

} // namespace GuiManager::Delegates
