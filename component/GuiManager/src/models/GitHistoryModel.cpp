#include "models/GitHistoryModel.h"

#include <Log.h>

#include <QBrush>
#include <QDateTime>
#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QLocale>
#include <QSet>

#define THREAD_ID "GitHistoryModel"

#define MULTIPLIER 5

namespace GuiManager {
namespace Models {

static StatusFieldsMap createStatusFieldsMap(const GitRepo::GitStatus &status) {
  LOG_PERF(0, "Enter");

  if (!status.hasChanges()) {
    LOG_PERF(0, "Exit");
    return {};
  }

  const StatusFieldsMap statusMap{
      {StatusFields::New, status.newFiles.size()},
      {StatusFields::Modified, status.modifiedFiles.size()},
      {StatusFields::Deleted, status.deletedFiles.size()},
      {StatusFields::TypeChanged, status.typeChangedFiles.size()},
      {StatusFields::Renamed, status.renamedFiles.size()},
      {StatusFields::Unreadable, status.unreadableFiles.size()},
      {StatusFields::NewStaged, status.newStagedFiles.size()},
      {StatusFields::ModifiedStaged, status.modifiedStagedFiles.size()},
      {StatusFields::DeletedStaged, status.deletedStagedFiles.size()},
      {StatusFields::TypeChangedStaged, status.typeChangedStagedFiles.size()},
      {StatusFields::RenamedStaged, status.renamedStagedFiles.size()},
      {StatusFields::Ignored, status.ignoredFiles.size()},
      {StatusFields::Conflicted, status.conflictedFiles.size()}};

  LOG_PERF(0, "Exit");

  return statusMap;
}

static QStringList headerTitles() {
  return {QObject::tr("Graph"), QObject::tr("Summary"), QObject::tr("Author"),
          QObject::tr("Email"), QObject::tr("Date"),    QObject::tr("SHA")};
}

// Minimal column widths: wide enough for the header titles.
static QVector<int> headerColumnWidths() {
  QVector<int> widths(COLUMN_LAST + 1, 1);
  const QFontMetrics metrics(QGuiApplication::font());
  const QStringList headers = headerTitles();
  const qsizetype one0 = metrics.horizontalAdvance("0");
  for (int column = COLUMN_FIRST; column <= COLUMN_LAST; ++column) {
    widths[column] = std::max(qsizetype(1),
                              metrics.horizontalAdvance(headers.value(column)) +
                                  MULTIPLIER * one0);
  }
  return widths;
}

GitHistoryModel::GitHistoryModel(QObject *parent)
    : QAbstractTableModel(parent), _graphBuilder(GRAPH_PALETTE_SIZE),
      _selectionModel(nullptr), _generation(0), _hasMore(false),
      _fetchInFlight(false), _fetchAll(false), _lowPriorityInFlight(false),
      _nextPage(1) {
  LOG_PERF(0, "Enter");

  _columnWidths = headerColumnWidths();

  LOG_PERF(0, "Exit");
}

GitHistoryModel::~GitHistoryModel() {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

void GitHistoryModel::quitUpdate() {
  LOCKGUARD(_mutex);

  ++_generation;
  _fetchInFlight = false;
  finishFetchAll();
}

void GitHistoryModel::fetchAll() {
  LOCKGUARD(_mutex);

  if (_fetchAll) {
    return;
  }

  if (!_hasMore || !_pageRequester) {
    emit fetchAllFinished();
    return;
  }

  _fetchAll = true;

  // With a page already in flight the chain continues when it arrives.
  fetchMore(QModelIndex());
}

void GitHistoryModel::cancelFetchAll() {
  LOCKGUARD(_mutex);

  if (!_fetchAll) {
    return;
  }

  if (_fetchInFlight && _lowPriorityInFlight) {
    // A low priority page can wait behind other requests for a long time.
    // Its answer is ignored, so the view can ask for the page again.
    ++_generation;
    _fetchInFlight = false;
  }

  finishFetchAll();
}

bool GitHistoryModel::isCommitLoaded(const QString &commit) const {
  LOCKGUARD(_mutex);
  return _historyWithStatus.history.commits.contains(commit);
}

bool GitHistoryModel::isCurrentGeneration(const quint64 generation) const {
  LOCKGUARD(_mutex);
  return generation == _generation;
}

void GitHistoryModel::finishFetchAll() {
  if (!_fetchAll) {
    return;
  }

  _fetchAll = false;
  emit fetchAllFinished();
}

void GitHistoryModel::updateStatus(const GitRepo::GitStatus &status) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_mutex);

  beginResetModel();

  _historyWithStatus.status = status;
  rebuildVisibleRows();

  endResetModel();

  LOG_PERF(0, "Exit");
}

void GitHistoryModel::setDateRange(const QDate &fromDate, const QDate &toDate) {
  LOCKGUARD(_mutex);

  if (_fromDate == fromDate && _toDate == toDate) {
    return;
  }

  _fromDate = fromDate;
  _toDate = toDate;

  beginResetModel();
  rebuildVisibleRows();
  endResetModel();
}

void GitHistoryModel::setFilter(const int column, const QString &filter) {
  if (column <= COLUMN_HISTORY_SLICE || column == COLUMN_COMMIT_DATETIME ||
      column > COLUMN_COMMIT_SHORT_SHA) {
    return;
  }

  const QString normalizedFilter = filter.trimmed();

  LOCKGUARD(_mutex);
  if (normalizedFilter.isEmpty()) {
    _filters.remove(column);
  } else {
    _filters.insert(column, normalizedFilter);
  }

  beginResetModel();
  rebuildVisibleRows();
  endResetModel();
}

int GitHistoryModel::commitIndexForSourceRow(const int sourceRow) const {
  const int indexRow =
      _historyWithStatus.status.hasChanges() ? sourceRow - 1 : sourceRow;
  if (indexRow < 0 || indexRow >= _historyWithStatus.history.commits.count()) {
    return -1;
  }

  return indexRow;
}

int GitHistoryModel::bisectMarkForSourceRow(const int sourceRow) const {
  if (_bisectMarks.isEmpty()) {
    return static_cast<int>(GitRepo::BisectMark::None);
  }

  const int indexRow = commitIndexForSourceRow(sourceRow);
  if (indexRow < 0) {
    return static_cast<int>(GitRepo::BisectMark::None);
  }

  return _bisectMarks.value(_historyWithStatus.history.commits.at(indexRow),
                            static_cast<int>(GitRepo::BisectMark::None));
}

bool GitHistoryModel::commitMatchesFilters(const QString &commit) const {
  const GitRepo::GitCommitInfo &commitInfo =
      _historyWithStatus.history.commitInfos.value(commit);

  const auto matches = [this](const int column, const QString &value) {
    const QString filter = _filters.value(column);
    return filter.isEmpty() || value.contains(filter, Qt::CaseInsensitive);
  };

  if (!matches(COLUMN_SUMMARY, commitInfo.summary.trimmed()) ||
      !matches(COLUMN_AUTHOR_NAME, commitInfo.authorName.trimmed()) ||
      !matches(COLUMN_AUTHOR_EMAIL, commitInfo.authorEmail.trimmed())) {
    return false;
  }

  const QDate commitDate =
      QDateTime::fromMSecsSinceEpoch(commitInfo.timeStamp).toUTC().date();
  if ((_fromDate.isValid() && commitDate < _fromDate) ||
      (_toDate.isValid() && commitDate > _toDate)) {
    return false;
  }

  const QString shortSha = commit.left(7);
  const QString shaFilter = _filters.value(COLUMN_COMMIT_SHORT_SHA);
  return shaFilter.isEmpty() ||
         shortSha.contains(shaFilter, Qt::CaseInsensitive) ||
         commit.contains(shaFilter, Qt::CaseInsensitive);
}

void GitHistoryModel::rebuildVisibleRows() {
  _visibleRows.clear();
  if (_historyWithStatus.status.hasChanges()) {
    _visibleRows.append(0);
  }

  const int sourceRowOffset = _historyWithStatus.status.hasChanges() ? 1 : 0;
  for (int row = 0; row < _historyWithStatus.history.commits.size(); ++row) {
    if (commitMatchesFilters(_historyWithStatus.history.commits.at(row))) {
      _visibleRows.append(row + sourceRowOffset);
    }
  }
}

bool GitHistoryModel::isSameFirstPage(
    const GitRepo::GitHistoryWithStatus &other) const {
  const GitRepo::GitHistory &current = _historyWithStatus.history;
  const GitRepo::GitHistory &incoming = other.history;

  if (!current.valid || !incoming.valid ||
      _historyWithStatus.branchName != other.branchName ||
      _historyWithStatus.branchType != other.branchType ||
      current.total != incoming.total || _headCommit != incoming.headCommit ||
      _refTips != incoming.refTips || _refLabels != incoming.refLabels ||
      current.commits.size() < incoming.commits.size()) {
    return false;
  }

  for (int i = 0; i < incoming.commits.size(); ++i) {
    const QString &sha = incoming.commits.at(i);
    if (current.commits.at(i) != sha ||
        !(current.commitInfos.value(sha) == incoming.commitInfos.value(sha))) {
      return false;
    }
  }

  return true;
}

void GitHistoryModel::appendGraphAndWidths(const int firstCommitIndex) {
  LOG_PERF(0, "Enter");

  const GitRepo::GitHistory &history = _historyWithStatus.history;

  const QFontMetrics metrics(QGuiApplication::font());
  const qsizetype one0 = metrics.horizontalAdvance("0");
  const auto textWidth = [&](const QString &text) {
    return static_cast<int>(std::max(
        qsizetype(1), metrics.horizontalAdvance(text) + MULTIPLIER * one0));
  };
  const auto growWidth = [this](const int column, const int width) {
    if (column >= 0 && column < _columnWidths.size()) {
      _columnWidths[column] = std::max(_columnWidths[column], width);
    }
  };

  QVector<Util::GraphCommit> graphCommits;
  graphCommits.reserve(history.commits.size() - firstCommitIndex);

  for (int i = firstCommitIndex; i < history.commits.size(); ++i) {
    const QString &sha = history.commits.at(i);
    const GitRepo::GitCommitInfo commitInfo = history.commitInfos.value(sha);

    graphCommits.push_back(Util::GraphCommit{sha, commitInfo.parents});

    growWidth(COLUMN_SUMMARY, textWidth(commitInfo.summary.trimmed()));
    growWidth(COLUMN_AUTHOR_NAME, textWidth(commitInfo.authorName.trimmed()));
    growWidth(COLUMN_AUTHOR_EMAIL, textWidth(commitInfo.authorEmail.trimmed()));
    growWidth(COLUMN_COMMIT_DATETIME,
              textWidth(QLocale::system().toString(
                  QDateTime::fromMSecsSinceEpoch(commitInfo.timeStamp).toUTC(),
                  QLocale::ShortFormat)));
    growWidth(COLUMN_COMMIT_SHORT_SHA, textWidth(sha.left(7)));
    growWidth(COLUMN_COMMIT_FULL_SHA, textWidth(sha));
  }

  _graphRows += _graphBuilder.addCommits(graphCommits, _refTips);

  growWidth(COLUMN_HISTORY_SLICE,
            _graphBuilder.maxLanes() * std::max(1, metrics.height()));

  LOG_PERF(0, "Exit");
}

void GitHistoryModel::updateHistoryAndStatus(
    const GitRepo::GitHistoryWithStatus &historyWithStatus) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_mutex);

  // Marks are not part of the history comparison below, so a change in them
  // only needs a repaint and not a full rebuild.
  const bool marksChanged =
      _bisectMarks != historyWithStatus.history.bisectMarks;
  _bisectMarks = historyWithStatus.history.bisectMarks;

  // Save us from updates with same data. Pages loaded after the first one
  // are kept.
  if (isSameFirstPage(historyWithStatus)) {
    if (!(_historyWithStatus.status == historyWithStatus.status)) {
      updateStatus(historyWithStatus.status);
    } else if (marksChanged && !_visibleRows.isEmpty()) {
      emit dataChanged(createIndex(0, COLUMN_FIRST),
                       createIndex(_visibleRows.size() - 1, COLUMN_LAST));
    }

    LOG_PERF(0, "Exit");
    return;
  }

  beginResetModel();

  // Answers to requests made for the previous history are ignored from now on.
  ++_generation;
  _fetchInFlight = false;

  // Loading the whole history continues for a refreshed history of the same
  // branch, a different branch has to be asked for again.
  const bool continueFetchAll =
      _fetchAll &&
      _historyWithStatus.branchName == historyWithStatus.branchName &&
      _historyWithStatus.branchType == historyWithStatus.branchType;

  _historyWithStatus = historyWithStatus;
  _historyWithStatus.page = 0;
  _refTips = historyWithStatus.history.refTips;
  _refLabels = historyWithStatus.history.refLabels;
  _headCommit = historyWithStatus.history.headCommit;
  _hasMore = historyWithStatus.history.hasMore;
  _nextPage = 1;

  _graphRows.clear();
  _graphBuilder.reset();
  _columnWidths = headerColumnWidths();
  appendGraphAndWidths(0);

  rebuildVisibleRows();

  endResetModel();

  if (continueFetchAll && _hasMore) {
    fetchMore(QModelIndex());
  } else {
    finishFetchAll();
  }

  LOG_PERF(0, "Exit");
}

void GitHistoryModel::appendPage(const GitRepo::GitHistory &history,
                                 const int page, const quint64 generation) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_mutex);

  if (generation != _generation) {
    LOG_PERF(0, "Exit");
    return;
  }

  _fetchInFlight = false;

  if (page != _nextPage || history.commits.isEmpty()) {
    // Nothing new (or an unexpected page): stop asking for more.
    _hasMore = false;
    finishFetchAll();
    LOG_PERF(0, "Exit");
    return;
  }

  const int firstCommitIndex = _historyWithStatus.history.commits.size();
  const int sourceRowOffset = _historyWithStatus.status.hasChanges() ? 1 : 0;

  _historyWithStatus.history.commits += history.commits;
  _historyWithStatus.history.commitInfos.insert(history.commitInfos);
  _historyWithStatus.history.hasMore = history.hasMore;
  _historyWithStatus.page = page;

  const QVector<int> oldWidths = _columnWidths;
  appendGraphAndWidths(firstCommitIndex);

  QVector<int> newVisibleRows;
  for (int i = firstCommitIndex; i < _historyWithStatus.history.commits.size();
       ++i) {
    if (commitMatchesFilters(_historyWithStatus.history.commits.at(i))) {
      newVisibleRows.append(i + sourceRowOffset);
    }
  }

  if (!newVisibleRows.isEmpty()) {
    const int firstRow = _visibleRows.size();
    beginInsertRows(QModelIndex(), firstRow,
                    firstRow + newVisibleRows.size() - 1);
    _visibleRows += newVisibleRows;
    endInsertRows();
  }

  _hasMore = history.hasMore;
  _nextPage = page + 1;

  if (_columnWidths != oldWidths) {
    emit columnWidthsChanged();
  }

  emit pageAppended();

  if (!_hasMore) {
    finishFetchAll();
  } else if (_fetchAll || newVisibleRows.isEmpty()) {
    // Either the whole history was asked for, or everything on this page is
    // filtered out, so the view has nothing new to scroll to and would not
    // ask for more by itself.
    fetchMore(QModelIndex());
  }

  LOG_PERF(0, "Exit");
}

void GitHistoryModel::pageFailed(const quint64 generation) {
  LOCKGUARD(_mutex);

  if (generation != _generation) {
    return;
  }

  _fetchInFlight = false;
  _hasMore = false;

  emit pageLoadFailed();

  finishFetchAll();
}

bool GitHistoryModel::hasStatus() const {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_mutex);

  LOG_PERF(0, "Exit");
  return _historyWithStatus.status.hasChanges();
}

QModelIndex GitHistoryModel::index(const int row, const int column,
                                   const QModelIndex &parent) const {
  LOCKGUARD(_mutex);

  const int count = _visibleRows.size();

  if (parent.isValid() || row < 0 || row >= count) {
    return {};
  }

  return createIndex(row, column);
}

int GitHistoryModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid()) {
    return 0;
  }

  LOCKGUARD(_mutex);

  return _visibleRows.size();
}

int GitHistoryModel::columnCount(const QModelIndex &parent) const {
  return COLUMN_LAST - COLUMN_FIRST + 1;
}

QVariant GitHistoryModel::data(const QModelIndex &index, const int role) const {
  if (!index.isValid()) {
    return {};
  }

  LOCKGUARD(_mutex);

  if (index.row() < 0 || index.row() >= _visibleRows.size()) {
    return {};
  }

  if (_selectionModel && role == Qt::FontRole &&
      _selectionModel->isSelected(index)) {
    QFont font;
    font.setBold(true);
    return font;
  }

  const int sourceRow = _visibleRows.at(index.row());
  const bool isStatusRow =
      sourceRow == 0 && _historyWithStatus.status.hasChanges();

  if (role == HistorySliceWidthRole) {
    return _graphBuilder.maxLanes();
  }

  if (role == StatusRole) {
    if (isStatusRow) {
      return QVariant::fromValue(
          createStatusFieldsMap(_historyWithStatus.status));
    }

    return {};
  }

  const int commitIndex = isStatusRow ? -1 : commitIndexForSourceRow(sourceRow);

  if (role == GraphRowRole) {
    if (commitIndex < 0 || commitIndex >= _graphRows.size()) {
      return {};
    }

    return QVariant::fromValue(_graphRows.at(commitIndex));
  }

  if (role == WorkingTreeAnchorRole) {
    if (commitIndex < 0) {
      return false;
    }

    if (_headCommit.isEmpty()) {
      return commitIndex == 0;
    }

    return _historyWithStatus.history.commits.at(commitIndex) == _headCommit;
  }

  if (role == RefBadgesRole) {
    if (commitIndex < 0) {
      return {};
    }

    const QString &commit = _historyWithStatus.history.commits.at(commitIndex);
    return _refLabels.value(commit);
  }

  if (role == BisectMarkRole) {
    return bisectMarkForSourceRow(sourceRow);
  }

  if (role == Qt::BackgroundRole) {
    const int mark = bisectMarkForSourceRow(sourceRow);
    if (mark == static_cast<int>(GitRepo::BisectMark::None)) {
      return {};
    }

    QColor color = bisectMarkColor(mark);
    color.setAlpha(45);
    return QBrush(color);
  }

  if (role != Qt::DisplayRole) {
    return {};
  }

  if (commitIndex < 0) {
    return {};
  }

  const QString &commit = _historyWithStatus.history.commits.at(commitIndex);
  const GitRepo::GitCommitInfo &commitInfo =
      _historyWithStatus.history.commitInfos.value(commit);

  QVariant result;

  switch (index.column()) {
  case COLUMN_HISTORY_SLICE: {
    // Drawn by the delegate using GraphRowRole.
    break;
  }
  case COLUMN_SUMMARY: {
    result = commitInfo.summary.trimmed();
    break;
  }
  case COLUMN_AUTHOR_NAME: {
    result = commitInfo.authorName.trimmed();
    break;
  }
  case COLUMN_AUTHOR_EMAIL: {
    result = commitInfo.authorEmail.trimmed();
    break;
  }
  case COLUMN_COMMIT_DATETIME: {
    // commitInfo.timeStamp is already in local time
    result = QLocale::system().toString(
        QDateTime::fromMSecsSinceEpoch(commitInfo.timeStamp).toUTC(),
        QLocale::ShortFormat);
    break;
  }
  case COLUMN_COMMIT_SHORT_SHA: {
    result = commit.left(7); // Short SHA
    break;
  }
  case COLUMN_COMMIT_FULL_SHA: {
    result = commit; // Full SHA
    break;
  }
  default:
    break;
  }

  return result;
}

QVariant GitHistoryModel::headerData(const int section,
                                     const Qt::Orientation orientation,
                                     const int role) const {
  if (role == Qt::DisplayRole) {
    QVariant result;

    if (orientation == Qt::Horizontal) {
      switch (section) {
      case COLUMN_HISTORY_SLICE: {
        result = tr("Graph");
        break;
      }
      case COLUMN_SUMMARY: {
        result = tr("Summary");
        break;
      }
      case COLUMN_AUTHOR_NAME: {
        result = tr("Author");
        break;
      }
      case COLUMN_AUTHOR_EMAIL: {
        result = tr("Email");
        break;
      }
      case COLUMN_COMMIT_DATETIME: {
        result = tr("Date");
        break;
      }
      case COLUMN_COMMIT_SHORT_SHA: {
        result = tr("SHA");
        break;
      }
      default: {
        break;
      }
      }

      return result; // No row names
    }

    return result; // No row names
  }

  return QAbstractTableModel::headerData(section, orientation, role);
}

bool GitHistoryModel::canFetchMore(const QModelIndex &parent) const {
  if (parent.isValid()) {
    return false;
  }

  LOCKGUARD(_mutex);

  return _hasMore && !_fetchInFlight && _pageRequester;
}

void GitHistoryModel::fetchMore(const QModelIndex &parent_) {
  LOG_PERF(0, "Enter");

  if (parent_.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  QString branchName;
  GitRepo::BranchType branchType;
  int page;
  quint64 generation;
  bool lowPriority;

  {
    LOCKGUARD(_mutex);

    if (!_hasMore || _fetchInFlight || !_pageRequester) {
      LOG_PERF(0, "Exit");
      return;
    }

    _fetchInFlight = true;
    _lowPriorityInFlight = _fetchAll;

    branchName = _historyWithStatus.branchName;
    branchType = _historyWithStatus.branchType;
    page = _nextPage;
    generation = _generation;
    lowPriority = _fetchAll;
  }

  // The request is processed in the background, the answer comes back through
  // appendPage() or pageFailed().
  _pageRequester(branchName, branchType, page, generation, lowPriority);

  LOG_PERF(0, "Exit");
}

} // namespace Models
} // namespace GuiManager
