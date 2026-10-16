#ifndef UTIL_COMMIT_GRAPH_BUILDER_H
#define UTIL_COMMIT_GRAPH_BUILDER_H

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

#include <algorithm>

namespace Util {

/// A commit as seen by the graph builder: only identity and ancestry matter.
struct GraphCommit {
  QString sha;
  QStringList parents;
};

/// One cell of the history graph. Each edge holds a color index (>= 0)
/// or -1 when the edge is absent. `node` is the color of the commit dot
/// drawn in the cell or -1 when the cell has no commit.
struct GraphSlice {
  qint16 top = -1;
  qint16 bottom = -1;
  qint16 left = -1;
  qint16 right = -1;
  qint16 node = -1;

  bool operator==(const GraphSlice &o) const {
    return top == o.top && bottom == o.bottom && left == o.left &&
           right == o.right && node == o.node;
  }
  bool operator!=(const GraphSlice &o) const { return !(*this == o); }
};

using GraphRow = QVector<GraphSlice>;

/// Incremental commit graph builder.
///
/// Commits must be fed newest to oldest (children before parents, i.e. in a
/// topological order). The builder keeps the lane state between calls, so
/// the graph can be extended page by page; feeding a history in any number
/// of chunks yields exactly the same rows as feeding it at once.
///
/// Colors are deterministic: they depend only on the fed commits and on the
/// ref names passed for tip commits, never on process-randomized hashes.
class CommitGraphBuilder {
public:
  explicit CommitGraphBuilder(int paletteSize = 10)
      : _paletteSize(std::max(1, paletteSize)) {}

  void reset() {
    _lanes.clear();
    _maxLanes = 0;
  }

  int paletteSize() const { return _paletteSize; }
  int maxLanes() const { return _maxLanes; }

  /// Number of lanes currently waiting for a not yet seen commit.
  int openLanes() const {
    int n = 0;
    for (const Lane &l : _lanes) {
      if (!l.waiting.isEmpty()) {
        ++n;
      }
    }
    return n;
  }

  QVector<GraphRow>
  addCommits(const QVector<GraphCommit> &commits,
             const QHash<QString, QStringList> &refTips = {}) {
    QVector<GraphRow> rows;
    rows.reserve(commits.size());
    for (const GraphCommit &c : commits) {
      rows.push_back(addCommit(c, refTips.value(c.sha)));
    }
    return rows;
  }

  /// `refKeys` are names of refs pointing to the commit; they are used to
  /// choose the color of a new lane started by a branch tip.
  GraphRow addCommit(const GraphCommit &commit,
                     const QStringList &refKeys = {}) {
    GraphRow row(_lanes.size());
    QVector<bool> usedThisRow(_lanes.size(), false);

    // Lanes waiting for this commit.
    QVector<int> incoming;
    for (int i = 0; i < _lanes.size(); ++i) {
      const Lane &lane = _lanes[i];
      if (lane.waiting.isEmpty()) {
        continue;
      }
      if (lane.waiting == commit.sha) {
        incoming.push_back(i);
      } else {
        row[i].top = row[i].bottom = static_cast<qint16>(lane.color);
      }
      usedThisRow[i] = true;
    }

    int col = -1;
    int color = 0;
    if (incoming.isEmpty()) {
      // A branch tip (or an otherwise unreachable-from-above commit).
      col = firstFreeLane(0, usedThisRow);
      const QString key = refKeys.isEmpty() ? commit.sha : refKeys.first();
      color = pickColor(key);
      if (col == _lanes.size()) {
        _lanes.push_back(Lane());
      }
      _lanes[col].color = color;
      ensure(row, usedThisRow, col);
    } else {
      col = incoming.first();
      color = _lanes[col].color;
      row[col].top = static_cast<qint16>(color);
      for (int k = 1; k < incoming.size(); ++k) {
        const int i = incoming[k];
        const int laneColor = _lanes[i].color;
        row[i].top = static_cast<qint16>(laneColor);
        hconnect(row, i, col, laneColor);
        _lanes[i].waiting.clear();
      }
    }
    usedThisRow[col] = true;
    row[col].node = static_cast<qint16>(color);

    if (commit.parents.isEmpty()) {
      _lanes[col].waiting.clear();
    } else {
      _lanes[col].waiting = commit.parents.first();
      row[col].bottom = static_cast<qint16>(color);

      for (int k = 1; k < commit.parents.size(); ++k) {
        const QString &parent = commit.parents[k];

        int existing = -1;
        for (int i = 0; i < _lanes.size(); ++i) {
          if (i != col && _lanes[i].waiting == parent) {
            existing = i;
            break;
          }
        }
        if (existing >= 0) {
          // The lane may have been opened on this very row.
          ensure(row, usedThisRow, existing);
          if (row[existing].bottom < 0) {
            row[existing].bottom = static_cast<qint16>(_lanes[existing].color);
          }
          hconnect(row, col, existing, _lanes[existing].color);
          continue;
        }

        const int lane = firstFreeLane(col + 1, usedThisRow);
        const int laneColor = pickColor(parent);
        if (lane == _lanes.size()) {
          _lanes.push_back(Lane());
        }
        _lanes[lane].waiting = parent;
        _lanes[lane].color = laneColor;
        ensure(row, usedThisRow, lane);
        usedThisRow[lane] = true;
        row[lane].bottom = static_cast<qint16>(laneColor);
        hconnect(row, col, lane, laneColor);
      }
    }

    _maxLanes = std::max(_maxLanes, static_cast<int>(row.size()));

    while (!_lanes.isEmpty() && _lanes.last().waiting.isEmpty()) {
      _lanes.removeLast();
    }
    return row;
  }

private:
  struct Lane {
    QString waiting;
    int color = 0;
  };

  static quint32 fnv1a(const QString &s) {
    const QByteArray bytes = s.toUtf8();
    quint32 h = 2166136261u;
    for (char ch : bytes) {
      h ^= static_cast<quint8>(ch);
      h *= 16777619u;
    }
    return h;
  }

  /// Picks a color for a new lane, preferring one not used by any active
  /// lane so neighbouring branches are told apart.
  int pickColor(const QString &key) const {
    const int start = static_cast<int>(fnv1a(key) % _paletteSize);
    for (int k = 0; k < _paletteSize; ++k) {
      const int candidate = (start + k) % _paletteSize;
      bool taken = false;
      for (const Lane &lane : _lanes) {
        if (!lane.waiting.isEmpty() && lane.color == candidate) {
          taken = true;
          break;
        }
      }
      if (!taken) {
        return candidate;
      }
    }
    return start;
  }

  /// First lane index >= from that is free and was not touched on this row.
  int firstFreeLane(int from, const QVector<bool> &usedThisRow) const {
    for (int i = from; i < _lanes.size(); ++i) {
      const bool used = i < usedThisRow.size() && usedThisRow[i];
      if (_lanes[i].waiting.isEmpty() && !used) {
        return i;
      }
    }
    return std::max<int>(from, _lanes.size());
  }

  static void ensure(GraphRow &row, QVector<bool> &used, int index) {
    if (row.size() <= index) {
      row.resize(index + 1);
    }
    if (used.size() <= index) {
      used.resize(index + 1);
    }
  }

  /// Draws a horizontal connector between two cells of a row. Cells that
  /// already have a horizontal edge keep it.
  static void hconnect(GraphRow &row, int a, int b, int color) {
    const int lo = std::min(a, b);
    const int hi = std::max(a, b);
    if (lo == hi) {
      return;
    }
    if (row.size() <= hi) {
      row.resize(hi + 1);
    }
    const qint16 c = static_cast<qint16>(color);
    if (row[lo].right < 0) {
      row[lo].right = c;
    }
    if (row[hi].left < 0) {
      row[hi].left = c;
    }
    for (int i = lo + 1; i < hi; ++i) {
      if (row[i].left < 0) {
        row[i].left = c;
      }
      if (row[i].right < 0) {
        row[i].right = c;
      }
    }
  }

  int _paletteSize;
  int _maxLanes = 0;
  QVector<Lane> _lanes;
};

} // namespace Util

#endif // UTIL_COMMIT_GRAPH_BUILDER_H
