#include "StylesheetManager.h"

#include <Log.h>

#include <QMap>

#define THREAD_ID "StylesheetManager"

namespace StylesheetManager {

QMap<StylesheetKey, QString> STYLESHEETS = {
    {StylesheetKey::Default,
     "QColorDialog{font-size:14px;}"
     "QPushButton[flat=true]{color:white;text-decoration:none;}"
     "QPushButton[flat=true]:hover{color:#74897F;text-decoration:"
     "underline;}"
     "QTabBar::tab:selected{font-weight:bold;}"
     "GuiManager--Widgets--RecentTableView,GuiManager--Widgets--"
     "RecentTableView * {font-size:14px;}"
     "GuiManager--Widgets--TitleBarMenuButton::menu-indicator{width:0px;}"
     "GuiManager--Widgets--TitlebarWidget,GuiManager--Widgets--TitlebarWidget "
     "* {font-size:14px;}"
     "GuiManager--Containers--SettingsContainer{font-size:14px;background-color:#353535;}"
     "GuiManager--Containers--SettingsContainer "
     "QSplitter{margin-top:30px;}"
     "GuiManager--Containers--SettingsContainer "
     "QSplitter::handle:horizontal{border:1px solid #353535;}"
     "GuiManager--Containers--SettingsContainer "
     "QScrollArea{margin-top:30px;}"
     "GuiManager--Containers--SettingsContainer "
     "QTreeView{font-size:14px;margin-top:30px;}"
     "GuiManager--Containers--SettingsContainer "
     "QDateTimeEdit#dateTimeEdit{max-width:145px;}"
     "GuiManager--Containers--SettingsContainer "
     "QLabel#groupLabel{font-size:20px;font-weight:bold;background-color:#202020;padding:8px;}"
     "GuiManager--Containers--SettingsContainer "
     "QLabel#nameLabel{font-weight:bold;}"
     "GuiManager--Containers--SettingsContainer "
     "GuiManager--Widgets--ColorButton{max-width:75px;}"
     "GuiManager--Containers--SettingsContainer "
     "QSpinBox#longSpinBox{max-width:75px;}"
     "GuiManager--Containers--SettingsContainer "
     "GuiManager--Widgets--SettingsPaneItemWidget{padding:0px;qproperty-"
     "hoverColor:#282828;}"}};

StylesheetManager::StylesheetManager() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

StylesheetManager::~StylesheetManager() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

StylesheetManager &StylesheetManager::getInstance() {
  static StylesheetManager instance;
  return instance;
}

const QString &StylesheetManager::stylesheet(StylesheetKey key) const {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Stylesheet %1 requested.", (int)key);

  LOG_PERF(0, "Exit");

  return STYLESHEETS[key];
}

} // namespace StylesheetManager
