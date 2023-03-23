#pragma once

#include <ConfigManager.h>

#include <QHoverEvent>
#include <QModelIndex>
#include <QObject>
#include <QStyledItemDelegate>

namespace GuiManager {

namespace Delegates {

class HomeRecentItemDelegate : public QStyledItemDelegate {
  Q_OBJECT
public:
  explicit HomeRecentItemDelegate(QObject *parent = nullptr);

  void setHover(const QModelIndex& index);

  // QAbstractItemDelegate interface
public:
  void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
  int _nameFontSize;
  QColor _nameColor;
  QColor _selectedNameColor;
  int _urlFontSize;
  QColor _urlColor;
  QModelIndex _indexHovered;
  ConfigManager::ConfigManager& _configManager;
};

} // namespace Delegates
} // namespace GuiManager
