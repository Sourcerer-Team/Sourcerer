// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include <QSortFilterProxyModel>

namespace GuiManager::Widgets {

class MoveProxy : public QSortFilterProxyModel {
public:
  using QSortFilterProxyModel::QSortFilterProxyModel;

  Qt::DropActions supportedDropActions() const override {
    return Qt::CopyAction;
  }

  Qt::ItemFlags flags(const QModelIndex &index) const override {
    Qt::ItemFlags f = QSortFilterProxyModel::flags(index);
    if (index.isValid())
      f |= Qt::ItemIsDropEnabled;
    else
      f |= Qt::ItemIsDropEnabled;
    return f;
  }

  bool moveRows(const QModelIndex &srcParent, int srcRow, int count,
                const QModelIndex &dstParent, int dstRow) override {
    return sourceModel()->moveRows(mapToSource(srcParent), srcRow, count,
                                   mapToSource(dstParent), dstRow);
  }
};

} // namespace GuiManager::Widgets
