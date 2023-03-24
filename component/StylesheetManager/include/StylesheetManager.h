#pragma once

#include "StylesheetKey.h"

#include <QVector>

namespace StylesheetManager {

class StylesheetManager {
public:
  static StylesheetManager &getInstance();

  const QString &stylesheet(StylesheetKey key) const;

private:
  StylesheetManager();
  ~StylesheetManager();

  StylesheetManager(const StylesheetManager &) = delete;
  StylesheetManager &operator=(const StylesheetManager &) = delete;
};

} // namespace StylesheetManager
