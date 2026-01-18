#include "qvtlayout.h"

QVTLayout::QVTLayout() {
  appendLine();
}

int QVTLayout::lineCount() const {
  return _lines.count();
}

QVTLine &QVTLayout::lineAt(int i) {
  if (i >= _lines.length()) {
    _lines.resize(i + 1);
  }

  return _lines[i];
}

void QVTLayout::appendLine() {
  _lines.append(QVTLine());
}

void QVTLayout::clear() {
  _lines.clear();
  appendLine();
}
