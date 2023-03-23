#include "delegates/HomeRecentItemDelegate.h"

#include <Log.h>

#include <QTableView>

#define THREAD_ID "HomeRecentItemDelegate"

namespace GuiManager {

namespace Delegates {

HomeRecentItemDelegate::HomeRecentItemDelegate(QObject *parent)
    : QStyledItemDelegate{parent},
      _configManager(ConfigManager::ConfigManager::getInstance()) {
  _nameFontSize = _configManager.getLong(ConfigManager::ConfigKey::HomeContainerRecentTableNameFontSize);
  _urlFontSize = _configManager.getLong(ConfigManager::ConfigKey::HomeContainerRecentTableUrlFontSize);

  LOG_DEBUG("Name column font size is %1 pixels.", _nameFontSize);
  LOG_DEBUG("Url column font size is %1 pixels.", _urlFontSize);

  _nameColor = QColor::fromString(_configManager.getString(ConfigManager::ConfigKey::HomeContainerRecentTableNameColor));
  _selectedNameColor = QColor::fromString(_configManager.getString(ConfigManager::ConfigKey::HomeContainerRecentTableSelectedNameColor));
  _urlColor = QColor::fromString(_configManager.getString(ConfigManager::ConfigKey::HomeContainerRecentTableUrlColor));

  LOG_DEBUG("Name column color is '%1'", _nameColor.name());
  LOG_DEBUG("Selected Name column color is '%1'", _selectedNameColor.name());
  LOG_DEBUG("Url column color is '%1'", _urlColor.name());
}

void HomeRecentItemDelegate::setHover(const QModelIndex &index) {
  _indexHovered = index;
}

void HomeRecentItemDelegate::paint(QPainter *painter,
                                   const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const {
  QStyleOptionViewItem _option = option;

  if(index.column() == 0) {
    _option.font.setPixelSize(_nameFontSize);
    _option.palette.setColor(QPalette::ColorRole::Text, _nameColor);
  } else {
    _option.font.setPixelSize(_urlFontSize);
    _option.palette.setColor(QPalette::ColorRole::Text, _urlColor);
  }

  if (_indexHovered == index) {
    _option.font.setUnderline(true);
    _option.palette.setColor(QPalette::ColorRole::Text, _selectedNameColor);
  }

  QStyledItemDelegate::paint(painter, _option, index);
}

} // namespace Delegates
} // namespace GuiManager
