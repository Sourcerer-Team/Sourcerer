#pragma once

#include "StylesheetKey.h"

#include <QColor>
#include <QPixmap>
#include <QString>

class QApplication;
class QWidget;

namespace StylesheetManager {

class StylesheetManager {
public:
  static StylesheetManager &getInstance();

  static const QString &stylesheet(StylesheetKey key);
  static StylesheetKey currentStylesheet();
  static void setCurrentStylesheet(StylesheetKey key);
  static QString stylesheetKeyName(StylesheetKey key);
  static StylesheetKey stylesheetKeyFromName(const QString &name);
  static QColor menuBarButtonIconColor();
  static QColor historyBadgeFillColor();
  static QColor historyBadgeTextColor();
  static QPixmap recolorPixmap(const QPixmap &source, const QColor &color);
  static void applyStylesheet(QApplication *app = nullptr);
  static void refreshMenuBarButtonIcons(QWidget *root = nullptr);

private:
  StylesheetManager();
  ~StylesheetManager();

  StylesheetManager(const StylesheetManager &) = delete;
  StylesheetManager &operator=(const StylesheetManager &) = delete;
};

} // namespace StylesheetManager
