// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "ResizableVector.h"

#include <functional>

namespace Util {

template <typename T> class ResizableMatrix {
public:
  ResizableMatrix() {}

  const ResizableVector<T> &operator[](qsizetype index) const {
    Q_ASSERT(index >= 0);

    if (index < 0) {
      throw std::out_of_range("Index cannot be negative");
    }

    if (index >= _columns.size()) {
      _columns.resize(index + 1);
    }

    return _columns[index];
  }

  ResizableVector<T> &operator[](qsizetype index) {
    Q_ASSERT(index >= 0);

    if (index < 0) {
      throw std::out_of_range("Index cannot be negative");
    }

    if (index >= _columns.size()) {
      _columns.resize(index + 1);
    }

    return _columns[index];
  }

  const T &at(qsizetype x, qsizetype y) const {
    Q_ASSERT(x >= 0 && y >= 0);

    if (x < 0 || y < 0) {
      throw std::out_of_range("Index cannot be negative");
    }

    if (x >= _columns.size()) {
      _columns.resize(x + 1);
    }

    return _columns.at(x).at(y);
  }

  void resize(const qsizetype width, const qsizetype height) {
    Q_ASSERT(width >= 0 && height >= 0);

    if (width < 0 || height < 0) {
      throw std::out_of_range("Index cannot be negative");
    }

    _columns.resize(width);

    for (auto &column : _columns) {
      column.resize(height);
    }
  }

  qsizetype sizeX() const { return _columns.size(); }

  qsizetype sizeY() const {
    qsizetype maxSize = 0;
    for (qsizetype i = 0; i < _columns.size(); ++i) {
      if (_columns[i].size() > maxSize) {
        maxSize = _columns[i].size();
      }
    }

    return maxSize;
  }

  void invertHorizontaly() const {
    for (int i = 0; i < sizeX() / 2; ++i) {
      std::swap(_columns[i], _columns[sizeX() - i - 1]);
    }
  }

  void
  invertVertically(const bool &quitFlag,
                   const std::function<void(int)> &progressCallback = nullptr) {
    resize(sizeX(), sizeY());
    for (int i = 0; i < sizeY() / 2 && !quitFlag; ++i) {
      if (progressCallback) {
        const double progress = (i * 100.0) / (sizeY() / 2);
        progressCallback(static_cast<int>(progress));
      }

      for (int j = 0; j < sizeX() && !quitFlag; ++j) {
        std::swap(_columns[j][i], _columns[j][sizeY() - i - 1]);
      }
    }
  }

  void clear() { _columns.clear(); }

  void insertColumn(int x) { _columns.insert(x, Util::ResizableVector<T>()); }

  void insertRow(int y) {
    for (int i = 0; i < sizeX(); ++i) {
      _columns[i].insert(y, T());
    }
  }

private:
  mutable ResizableVector<ResizableVector<T>> _columns;
};

} // namespace Util
