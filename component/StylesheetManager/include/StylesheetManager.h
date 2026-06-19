#pragma once

#include "StylesheetKey.h"

#include <QString>

class QApplication;

namespace StylesheetManager {

class StylesheetManager {
public:
  static StylesheetManager &getInstance();

  static const QString &stylesheet(StylesheetKey key);
  static StylesheetKey currentStylesheet();
  static void setCurrentStylesheet(StylesheetKey key);
  static void applyStylesheet(QApplication *app = nullptr);

private:
  StylesheetManager();
  ~StylesheetManager();

  StylesheetManager(const StylesheetManager &) = delete;
  StylesheetManager &operator=(const StylesheetManager &) = delete;
};

} // namespace StylesheetManager
