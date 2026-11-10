// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "StylesheetKey.h"

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>
#include <QVector>

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
  // Colors used to tell branches apart in the history graph. The count and
  // order are the same for every theme.
  static QVector<QColor> graphLaneColors();
  static QString issueTrackerToolbarStyleSheet();
  static QIcon issueTrackerToolbarBackIcon();
  static QIcon issueTrackerToolbarForwardIcon();
  static QIcon issueTrackerToolbarRefreshIcon();
  static QPixmap recolorPixmap(const QPixmap &source, const QColor &color);
  static QPixmap renderSvgToPixmap(const QString &path, const QColor &color);
  static void applyStylesheet(QApplication *app = nullptr);
  static void refreshMenuBarButtonIcons(QWidget *root = nullptr);

private:
  StylesheetManager();
  ~StylesheetManager();

  StylesheetManager(const StylesheetManager &) = delete;
  StylesheetManager &operator=(const StylesheetManager &) = delete;
};

} // namespace StylesheetManager
