#pragma once

#include "containers/RepoContainer.h"

#include <CommitGraphBuilder.h>
#include <EventManager.h>
#include <GitRepo.h>
#include <LockGuard.h>

#include <QColor>
#include <QDate>
#include <QEnableSharedFromThis>

#include <QHash>
#include <QStringList>
#include <QVector>
#include <functional>
#include <qitemselectionmodel.h>

#define COLUMN_HISTORY_SLICE 0
#define COLUMN_SUMMARY 1
#define COLUMN_AUTHOR_NAME 2
#define COLUMN_AUTHOR_EMAIL 3
#define COLUMN_COMMIT_DATETIME 4
#define COLUMN_COMMIT_SHORT_SHA 5
#define COLUMN_COMMIT_FULL_SHA 6
#define COLUMN_FIRST COLUMN_HISTORY_SLICE
#define COLUMN_LAST COLUMN_COMMIT_SHORT_SHA

// Number of lane colors the graph is drawn with, see
// StylesheetManager::graphLaneColors().
#define GRAPH_PALETTE_SIZE 10

// Number of lanes of the graph, int.
#define HistorySliceWidthRole Qt::UserRole
#define StatusRole (Qt::UserRole + 1)
#define BisectMarkRole (Qt::UserRole + 3)
// Util::GraphRow of a commit row.
#define GraphRowRole (Qt::UserRole + 4)
// bool, true for the commit the working tree status row is attached to.
#define WorkingTreeAnchorRole (Qt::UserRole + 5)
// QStringList of the full names (refs/heads/..., refs/remotes/...,
// refs/tags/...) of the refs pointing to the commit of the row.
#define RefBadgesRole (Qt::UserRole + 6)

namespace GuiManager {
namespace Models {

enum class StatusFields {
  Unknown = 0,

  New,
  Modified,
  Deleted,
  TypeChanged,
  Renamed,
  Unreadable,
  NewStaged,
  ModifiedStaged,
  DeletedStaged,
  TypeChangedStaged,
  RenamedStaged,
  Ignored,
  Conflicted,

  last
};

typedef QMap<StatusFields, int> StatusFieldsMap;

inline QColor bisectMarkColor(const int mark) {
  switch (static_cast<GitRepo::BisectMark>(mark)) {
  case GitRepo::BisectMark::Good:
    return QColor(0x2E7D32);
  case GitRepo::BisectMark::Bad:
    return QColor(0xC62828);
  case GitRepo::BisectMark::Skip:
    return QColor(0x757575);
  case GitRepo::BisectMark::Current:
    return QColor(0x1565C0);
  default:
    return {};
  }
}

/// Table model of the repository history.
///
/// The history is loaded in pages: the first page arrives through
/// updateHistoryAndStatus(), the following ones are requested by the view
/// through fetchMore() and are appended with appendPage(). The graph is
/// extended incrementally with every page.
///
/// All methods must be called from the GUI thread.
class GitHistoryModel final : public QAbstractTableModel,
                              public QEnableSharedFromThis<GitHistoryModel> {
  Q_OBJECT

  typedef QSharedPointer<GitHistoryModel> SharedPtr;
  typedef QWeakPointer<GitHistoryModel> WeakPtr;

public:
  /// Asks for page `page` of the history of the given branch. The answer must
  /// be delivered with appendPage() or pageFailed() using the same
  /// `generation`. `lowPriority` is set for pages requested by fetchAll().
  typedef std::function<void(const QString &branchName,
                             GitRepo::BranchType branchType, int page,
                             quint64 generation, bool lowPriority)>
      PageRequester;

  explicit GitHistoryModel(QObject *parent = nullptr);
  ~GitHistoryModel() override;

  SharedPtr getSelf() { return sharedFromThis(); }

public:
  void setSelectionModel(QItemSelectionModel *selectionModel) {
    _selectionModel = selectionModel;
  }

  void setPageRequester(const PageRequester &requester) {
    _pageRequester = requester;
  }

  void updateStatus(const GitRepo::GitStatus &status);
  void setDateRange(const QDate &fromDate, const QDate &toDate);
  void setFilter(int column, const QString &filter);
  // Sets the title of the SHA column, e.g. "Revision" for Subversion.
  void setShaColumnTitle(const QString &title);

  /// Shows page 0 of a history (and drops everything loaded before).
  void updateHistoryAndStatus(
      const GitRepo::GitHistoryWithStatus &historyWithStatus);

  /// Appends a page requested by fetchMore(). Pages of an outdated
  /// `generation` are ignored.
  void appendPage(const GitRepo::GitHistory &history, int page,
                  quint64 generation);

  /// Tells the model that a requested page could not be loaded.
  void pageFailed(quint64 generation);

  GitRepo::GitHistoryWithStatus historyWithStatus() const {
    LOCKGUARD(_mutex);
    return _historyWithStatus;
  }

  QVector<int> columnWidths() const { return _columnWidths; }
  int columnWidth(int section) const { return _columnWidths.value(section); }

  /// Drops pending page requests; their answers will be ignored.
  void quitUpdate();

  bool hasStatus() const;

  bool hasMoreCommits() const { return _hasMore; }

  /// Requests the remaining pages one after another until the whole history
  /// is loaded, so the filters are applied to all commits of the repository.
  /// fetchAllFinished() is emitted when done, failed or cancelled.
  void fetchAll();

  /// Stops loading started with fetchAll(), the pages loaded so far are kept.
  /// fetchAllFinished() is emitted.
  void cancelFetchAll();

  bool isFetchingAll() const { return _fetchAll; }

  /// Returns true if `commit` is among the loaded commits, whether it is
  /// visible with the current filters or not.
  bool isCommitLoaded(const QString &commit) const;

  /// Returns false once the answer to a page requested with `generation`
  /// would be ignored, so loading it can be skipped.
  bool isCurrentGeneration(quint64 generation) const;

  // QAbstractTableModel interface
public:
  QModelIndex index(int row, int column,
                    const QModelIndex &parent) const override;
  int rowCount(const QModelIndex &parent) const override;
  int columnCount(const QModelIndex &parent) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role) const override;
  bool canFetchMore(const QModelIndex &parent) const override;
  void fetchMore(const QModelIndex &parent) override;

signals:
  /// Emitted after a page has been appended.
  void pageAppended();
  /// Emitted when a requested page could not be loaded.
  void pageLoadFailed();
  /// Emitted when loading started with fetchAll() has ended.
  void fetchAllFinished();
  /// Emitted when the preferred width of a column has grown.
  void columnWidthsChanged();

private:
  int bisectMarkForSourceRow(int sourceRow) const;
  int commitIndexForSourceRow(int sourceRow) const;
  bool commitMatchesFilters(const QString &commit) const;
  void rebuildVisibleRows();
  bool
  isSameFirstPage(const GitRepo::GitHistoryWithStatus &historyWithStatus) const;
  // Adds the commits of `history` starting at `firstCommitIndex` of
  // _historyWithStatus.history.commits to the graph and column widths.
  void appendGraphAndWidths(int firstCommitIndex);
  void finishFetchAll();

private:
  mutable Util::RecursiveMutex _mutex;
  // Everything loaded so far: commits newest first, their infos, the status
  // and the branch the history belongs to.
  GitRepo::GitHistoryWithStatus _historyWithStatus;
  // One graph row per commit of _historyWithStatus.history.commits.
  QVector<Util::GraphRow> _graphRows;
  Util::CommitGraphBuilder _graphBuilder;
  QHash<QString, QStringList> _refTips;
  QHash<QString, QStringList> _refLabels;
  QString _headCommit;
  QVector<int> _columnWidths;
  QString _shaColumnTitle;
  QHash<QString, int> _bisectMarks;
  QHash<int, QString> _filters;
  QDate _fromDate;
  QDate _toDate;
  QVector<int> _visibleRows;
  QItemSelectionModel *_selectionModel;
  PageRequester _pageRequester;
  // Incremented on every reset of the history, answers carry the value they
  // were requested with.
  quint64 _generation;
  bool _hasMore;
  bool _fetchInFlight;
  // Set by fetchAll(): every appended page requests the next one.
  bool _fetchAll;
  // The page in flight was requested with low priority.
  bool _lowPriorityInFlight;
  int _nextPage;
};

} // namespace Models
} // namespace GuiManager
