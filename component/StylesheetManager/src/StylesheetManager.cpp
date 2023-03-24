#include "StylesheetManager.h"

#include <Log.h>

#include <QMap>

#define THREAD_ID "StylesheetManager"

namespace StylesheetManager {

QMap<StylesheetKey, QString> STYLESHEETS = {
    {StylesheetKey::Default,
     "QPushButton[flat=true]{color:rgb(255,255,255);text-decoration:none;}"
     "QPushButton[flat=true]:hover{color:rgb(116,137,127);text-decoration:"
     "underline;}"
     "GuiManager--Widgets--RecentTableView,GuiManager--Widgets--"
     "RecentTableView * {font-size:14px;}"
     "GuiManager--Widgets--TitleBarMenuButton::menu-indicator{width:0px;}"
     "GuiManager--Widgets--TitlebarWidget,GuiManager--Widgets--TitlebarWidget "
     "* {font-size:14px;}"}};

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
