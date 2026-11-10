// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include <QStyledItemDelegate>

namespace GuiManager::Delegates {

class FixedHeightDelegate : public QStyledItemDelegate {
public:
  FixedHeightDelegate(QObject *parent = nullptr)
      : QStyledItemDelegate(parent) {}

  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override {
    return QSize(200, 48); // Set your desired height here
  }
};

} // namespace GuiManager::Delegates
