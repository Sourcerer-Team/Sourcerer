#pragma once

#include <QVector>

namespace Util {

template <typename T>
class ResizableVector : public QVector<T> {
public:
  ResizableVector() : QVector<T>() {}

  const T& operator[](qsizetype index) const {
    if (index >= QVector<T>::size()) {
      const_cast<ResizableVector *>(this)->resize(index + 1);
    }

    return QVector<T>::operator[](index);
  }

  T& operator[](qsizetype index) {
    if (index >= QVector<T>::size()) {
      QVector<T>::resize(index + 1);
    }

    return QVector<T>::operator[](index);
  }

  const T& at(qsizetype index) const {
    if (index >= QVector<T>::size()) {
      QVector<T>::resize(index + 1);
    }

    return QVector<T>::at(index);
  }


};

}