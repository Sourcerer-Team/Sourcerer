#pragma once

#include <QMap>
#include <QString>
#include <QVector>

namespace Util {

template <typename T> class Graph {
public:
  Graph() {}

  void addEdge(const T &from, const T &to) { _adjacencyList[from].append(to); }

  QVector<T> neighbors(const T &node) const { return _adjacencyList[node]; }

  bool hasNode(const T &node) const { return _adjacencyList.contains(node); }

  QVector<T> nodes() const { return _adjacencyList.keys(); }

private:
  QMap<T, QVector<T>> _adjacencyList;
};

} // namespace Util