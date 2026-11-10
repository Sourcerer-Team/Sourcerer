// MIT License
// Copyright (c) 2026 Sourcerer

#include <CommitGraphBuilder.h>

#include <QRandomGenerator>
#include <QtTest>

using namespace Util;

namespace {

GraphCommit c(const QString &sha, const QStringList &parents = {}) {
  return GraphCommit{sha, parents};
}

QVector<GraphRow> buildInChunks(const QVector<GraphCommit> &commits,
                                const QHash<QString, QStringList> &refs,
                                int chunk, CommitGraphBuilder *out = nullptr) {
  CommitGraphBuilder builder;
  QVector<GraphRow> rows;
  for (int i = 0; i < commits.size(); i += chunk) {
    QVector<GraphCommit> part = commits.mid(i, chunk);
    rows += builder.addCommits(part, refs);
  }
  if (out) {
    *out = builder;
  }
  return rows;
}

// Random topologically ordered history (index 0 is the newest commit).
QVector<GraphCommit> randomHistory(int count, quint32 seed) {
  QRandomGenerator rng(seed);
  QVector<GraphCommit> commits;
  for (int i = 0; i < count; ++i) {
    commits.push_back(c(QStringLiteral("c%1").arg(i)));
  }
  for (int i = 0; i < count - 1; ++i) {
    const int span = std::min(count - 1 - i, 12);
    const int first = i + 1 + static_cast<int>(rng.bounded(span));
    // Make sure every commit is reachable: the next commit is always a
    // parent of someone, usually of the previous one.
    const int kind = static_cast<int>(rng.bounded(10));
    if (kind < 6) {
      commits[i].parents << QStringLiteral("c%1").arg(i + 1);
    } else {
      commits[i].parents << QStringLiteral("c%1").arg(first);
    }
    if (kind == 7 || kind == 8) {
      const int second = i + 1 + static_cast<int>(rng.bounded(span));
      const QString s = QStringLiteral("c%1").arg(second);
      if (!commits[i].parents.contains(s)) {
        commits[i].parents << s;
      }
    }
    if (kind == 9) {
      for (int k = 0; k < 2; ++k) {
        const QString s =
            QStringLiteral("c%1").arg(i + 1 + static_cast<int>(rng.bounded(span)));
        if (!commits[i].parents.contains(s)) {
          commits[i].parents << s;
        }
      }
    }
  }
  return commits;
}

} // namespace

class CommitGraphBuilderTest : public QObject {
  Q_OBJECT

private slots:
  void linearHistory() {
    CommitGraphBuilder b;
    const auto rows =
        b.addCommits({c("A", {"B"}), c("B", {"C"}), c("C")}, {{"A", {"main"}}});
    QCOMPARE(rows.size(), 3);
    QCOMPARE(b.maxLanes(), 1);
    QCOMPARE(b.openLanes(), 0);

    const int color = rows[0][0].node;
    QVERIFY(color >= 0);
    QCOMPARE(rows[0][0].top, qint16(-1));
    QCOMPARE(rows[0][0].bottom, qint16(color));
    QCOMPARE(rows[1][0].top, qint16(color));
    QCOMPARE(rows[1][0].bottom, qint16(color));
    QCOMPARE(rows[1][0].node, qint16(color));
    QCOMPARE(rows[2][0].top, qint16(color));
    QCOMPARE(rows[2][0].bottom, qint16(-1));
    QCOMPARE(rows[2][0].node, qint16(color));
  }

  void forkAndMerge() {
    CommitGraphBuilder b;
    const auto rows = b.addCommits(
        {c("M", {"A", "B"}), c("A", {"R"}), c("B", {"R"}), c("R")},
        {{"M", {"main"}}});
    QCOMPARE(b.maxLanes(), 2);
    QCOMPARE(b.openLanes(), 0);

    const int main = rows[0][0].node;
    const int side = rows[0][1].bottom;
    QVERIFY(side >= 0);
    QVERIFY(side != main);
    // The merge commit connects to the second lane.
    QCOMPARE(rows[0][0].right, qint16(side));
    QCOMPARE(rows[0][1].left, qint16(side));
    QCOMPARE(rows[0][1].top, qint16(-1));

    // A sits on lane 0 while lane 1 passes through.
    QCOMPARE(rows[1][0].node, qint16(main));
    QCOMPARE(rows[1][1].top, qint16(side));
    QCOMPARE(rows[1][1].bottom, qint16(side));
    QCOMPARE(rows[1][1].node, qint16(-1));

    // B sits on lane 1, lane 0 passes through.
    QCOMPARE(rows[2][1].node, qint16(side));
    QCOMPARE(rows[2][0].top, qint16(main));
    QCOMPARE(rows[2][0].bottom, qint16(main));

    // R merges both lanes back, the side lane keeps its color.
    QCOMPARE(rows[3][0].node, qint16(main));
    QCOMPARE(rows[3][1].top, qint16(side));
    QCOMPARE(rows[3][1].left, qint16(side));
    QCOMPARE(rows[3][0].right, qint16(side));
    QCOMPARE(rows[3][1].bottom, qint16(-1));
  }

  void octopusMerge() {
    CommitGraphBuilder b;
    const auto rows = b.addCommits(
        {c("O", {"A", "B", "C"}), c("A", {"R"}), c("B", {"R"}), c("C", {"R"}),
         c("R")},
        {{"O", {"main"}}});
    QCOMPARE(b.maxLanes(), 3);
    QCOMPARE(b.openLanes(), 0);
    const auto &o = rows[0];
    QCOMPARE(o.size(), 3);
    QVERIFY(o[1].bottom >= 0);
    QVERIFY(o[2].bottom >= 0);
    QVERIFY(o[1].bottom != o[2].bottom);
    QVERIFY(o[1].bottom != o[0].node);
    QVERIFY(o[2].bottom != o[0].node);
    // The connector to lane 1 ends there, the one to lane 2 passes through.
    QCOMPARE(o[0].right, qint16(o[1].bottom));
    QCOMPARE(o[1].left, qint16(o[1].bottom));
    QCOMPARE(o[1].right, qint16(o[2].bottom));
    QCOMPARE(o[2].left, qint16(o[2].bottom));
    QCOMPARE(o[2].right, qint16(-1));
  }

  void multipleRoots() {
    CommitGraphBuilder b;
    const auto rows = b.addCommits({c("A"), c("B")});
    QCOMPARE(b.maxLanes(), 1);
    QCOMPARE(rows[0][0].top, qint16(-1));
    QCOMPARE(rows[0][0].bottom, qint16(-1));
    QVERIFY(rows[0][0].node >= 0);
    QCOMPARE(rows[1][0].top, qint16(-1));
    QCOMPARE(rows[1][0].bottom, qint16(-1));
    QVERIFY(rows[1][0].node >= 0);
  }

  void parentMissingFromPage() {
    CommitGraphBuilder b;
    const auto rows = b.addCommits({c("A", {"X"})});
    QCOMPARE(b.openLanes(), 1);
    QCOMPARE(rows[0][0].bottom, rows[0][0].node);

    // The next page continues the same lane.
    const auto next = b.addCommits({c("X")});
    QCOMPARE(next[0][0].top, rows[0][0].bottom);
    QCOMPARE(next[0][0].node, rows[0][0].node);
    QCOMPARE(b.openLanes(), 0);
  }

  void simultaneousTipsGetDifferentColors() {
    CommitGraphBuilder b;
    const auto rows = b.addCommits(
        {c("A", {"R"}), c("B", {"R"}), c("R")},
        {{"A", {"main"}}, {"B", {"dev"}}});
    QCOMPARE(rows[0].size(), 1);
    QCOMPARE(rows[1].size(), 2);
    QVERIFY(rows[0][0].node >= 0);
    QVERIFY(rows[1][1].node >= 0);
    QVERIFY(rows[0][0].node != rows[1][1].node);
    // Lane 0 passes through B's row with its own color.
    QCOMPARE(rows[1][0].top, rows[0][0].node);
    QCOMPARE(rows[1][0].bottom, rows[0][0].node);
  }

  void incrementalEqualsBatch_data() {
    QTest::addColumn<quint32>("seed");
    QTest::addColumn<int>("count");
    QTest::newRow("small") << 1u << 30;
    QTest::newRow("medium") << 7u << 200;
    QTest::newRow("large") << 42u << 600;
  }

  void incrementalEqualsBatch() {
    QFETCH(quint32, seed);
    QFETCH(int, count);
    const auto commits = randomHistory(count, seed);
    QHash<QString, QStringList> refs;
    refs["c0"] = {"main"};
    refs["c5"] = {"feature/x"};
    refs["c9"] = {"origin-branch", "tag"};

    CommitGraphBuilder whole;
    const auto expected = buildInChunks(commits, refs, count, &whole);
    QCOMPARE(expected.size(), count);

    for (int chunk : {1, 2, 7, 64}) {
      CommitGraphBuilder inc;
      const auto rows = buildInChunks(commits, refs, chunk, &inc);
      QCOMPARE(rows, expected);
      QCOMPARE(inc.maxLanes(), whole.maxLanes());
      QCOMPARE(inc.openLanes(), whole.openLanes());
    }
  }

  void verticalContinuity_data() { incrementalEqualsBatch_data(); }

  void verticalContinuity() {
    QFETCH(quint32, seed);
    QFETCH(int, count);
    const auto commits = randomHistory(count, seed);
    const auto rows = buildInChunks(commits, {}, 16);

    for (int i = 0; i < rows.size(); ++i) {
      for (int j = 0; j < rows[i].size(); ++j) {
        const GraphSlice &s = rows[i][j];
        if (s.bottom >= 0) {
          QVERIFY2(i + 1 < rows.size() && j < rows[i + 1].size(),
                   qPrintable(QStringLiteral("row %1 lane %2 dangling").arg(i).arg(j)));
          QCOMPARE(rows[i + 1][j].top, s.bottom);
        }
        if (s.top >= 0) {
          QVERIFY(i > 0 && j < rows[i - 1].size());
          QCOMPARE(rows[i - 1][j].bottom, s.top);
        }
      }
      // Exactly one commit dot per row.
      int nodes = 0;
      for (const GraphSlice &s : rows[i]) {
        nodes += s.node >= 0 ? 1 : 0;
      }
      QCOMPARE(nodes, 1);
    }
  }

  void colorsAreStableAcrossBuilders() {
    const auto commits = randomHistory(150, 99);
    QHash<QString, QStringList> refs;
    refs["c0"] = {"main"};
    refs["c3"] = {"develop"};
    const auto a = buildInChunks(commits, refs, 150);
    const auto b = buildInChunks(commits, refs, 150);
    QCOMPARE(a, b);
  }

  void resetForgetsLanes() {
    CommitGraphBuilder b;
    b.addCommits({c("A", {"X"})});
    QCOMPARE(b.openLanes(), 1);
    b.reset();
    QCOMPARE(b.openLanes(), 0);
    QCOMPARE(b.maxLanes(), 0);
  }
};

QTEST_APPLESS_MAIN(CommitGraphBuilderTest)

#include "tst_commit_graph_builder.moc"
