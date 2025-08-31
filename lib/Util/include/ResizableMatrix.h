#pragma once

#include "ResizableVector.h"

namespace Util {

template <typename T> class ResizableMatrix {
public:
  ResizableMatrix() {}

  const ResizableVector<T> &operator[](qsizetype index) const {
    if (index >= _columns.size()) {
      _columns.resize(index + 1);
    }

    const auto& col = _columns[index];
    if (col.size() > _maxSizeY) {
      _maxSizeY = col.size();
    }

    return col;;
  }

  ResizableVector<T> &operator[](qsizetype index) {
    if (index >= _columns.size()) {
      _columns.resize(index + 1);
    }

    auto& col = _columns[index];
    if (col.size() > _maxSizeY) {
      _maxSizeY = col.size();
    }

    return col;;
  }

  const T &at(qsizetype x, qsizetype y) const {
    if (x >= _columns.size()) {
      _columns.resize(x + 1);
    }

    const T& element = _columns.at(x).at(y);

    if (_columns.at(x).size() > _maxSizeY) {
      _maxSizeY = _columns.at(x).size();
    }

    return element;
  }

  void resize(const qsizetype x, const qsizetype y) {
    _columns.resize(x + 1);

    _maxSizeY = y + 1;

    for (auto &column : _columns) {
      column.resize(y + 1);
    }
  }

  qsizetype sizeX() const { return _columns.size(); }

  qsizetype sizeY() const { return _maxSizeY; }

  void clear() {
    _columns.clear();
    _maxSizeY = 0;
  }

private:
  mutable qsizetype _maxSizeY = 0;
  mutable ResizableVector<ResizableVector<T>> _columns;
};

} // namespace Util