// MIT License
// Copyright (c) 2026 Sourcerer

#include "SvnRepo.h"
#include "SvnRepoPrivate.h"

#include <svn_path.h>

#include <Log.h>

#include <QDir>
#include <QFileInfo>

#include <algorithm>

#define THREAD_ID "SvnRepo"

namespace SvnRepo {

using namespace Private;

namespace {

// Directories directly below the repository root, or empty with an error.
static bool readRootEntries(const WorkingCopyInfo &info, QStringList *entries,
                            svn_client_ctx_t *ctx, apr_pool_t *pool) {
  return reportError(
      SvnError::ErrorWhileOpening,
      listDirectories(entries, trimmedUrl(info.layoutRootUrl), ctx, pool));
}

// Names of the directories below <root>/<directory>, empty if it does not
// exist.
static bool readChildren(const WorkingCopyInfo &info, const QString &directory,
                         QStringList *names, svn_client_ctx_t *ctx,
                         apr_pool_t *pool) {
  svn_error_t *error = listDirectories(
      names, trimmedUrl(info.layoutRootUrl) + QStringLiteral("/") + directory,
      ctx, pool);
  if (isMissingPathError(error)) {
    svn_error_clear(error);
    names->clear();
    return true;
  }

  return reportError(SvnError::ErrorWhileOpening, error);
}

// Whether name can name a branch (Local) or tag: not empty, not "." or "..",
// no slash or backslash, and not trunk for a branch. Subversion has no
// nested branches the way Git does: every folder below branches/ is one
// branch, so "feature/x" would be a folder inside the branch "feature".
// Records InvalidArgument when it can't.
static bool checkRefName(const QString &name, BranchType type) {
  QString reason;
  if (name.trimmed().isEmpty()) {
    reason = QStringLiteral("The name is empty.");
  } else if (name.contains(QLatin1Char('/')) ||
             name.contains(QLatin1Char('\\'))) {
    reason = QStringLiteral("'%1' can't contain a slash.").arg(name);
  } else if (name == QLatin1String(".") || name == QLatin1String("..")) {
    reason = QStringLiteral("'%1' isn't a name.").arg(name);
  } else if (type != BranchType::Tag && name == QLatin1String("trunk")) {
    reason = QStringLiteral("trunk isn't a branch that can be created, "
                            "renamed or deleted.");
  }
  if (!reason.isEmpty()) {
    setLastError(SvnError::InvalidArgument, -1, reason);
    return false;
  }
  return true;
}

// The URL of the trunk, branch or tag the working copy is in, without the
// subfolder it may have checked out.
static QString branchRootUrl(const WorkingCopyInfo &info) {
  const QString url = trimmedUrl(info.url);
  return url.left(url.size() - branchSubpath(info).size());
}

// Records InvalidArgument when the working copy isn't on trunk, a branch or a
// tag (the project or repository root, a repository without trunk).
static bool checkOnBranch(const WorkingCopyInfo &info) {
  if (branchNameFromUrl(info.url, info.layoutRootUrl, BranchType::Local)
          .isEmpty() &&
      branchNameFromUrl(info.url, info.layoutRootUrl, BranchType::Tag)
          .isEmpty()) {
    setLastError(SvnError::InvalidArgument, -1,
                 QStringLiteral("The working copy isn't on trunk, a branch or "
                                "a tag."));
    return false;
  }
  return true;
}

// Runs a server side operation that commits with message.
static bool runRepositoryCommit(
    const QString &wcPath, const QString &message,
    const std::function<svn_error_t *(svn_client_ctx_t *, apr_pool_t *,
                                      const WorkingCopyInfo &)> &operation,
    SvnError errorKind) {
  return runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        WorkingCopyInfo info;
        if (!reportError(
                SvnError::InfoFailed,
                readInfo(&info, wcPath, revisionUnspecified(), ctx, pool))) {
          return false;
        }

        const LogMessageScope logMessage(ctx, message);
        return reportError(errorKind, operation(ctx, pool, info));
      },
      wcPath);
}

static svn_error_t *copyUrl(const QString &sourceUrl,
                            const svn_opt_revision_t &revision,
                            const QString &destinationUrl,
                            svn_client_ctx_t *ctx, apr_pool_t *pool) {
  const QByteArray sourceBytes = sourceUrl.toUtf8();
  const QByteArray destinationBytes = destinationUrl.toUtf8();

  auto *source = static_cast<svn_client_copy_source_t *>(
      apr_pcalloc(pool, sizeof(svn_client_copy_source_t)));
  auto *sourceRevision = static_cast<svn_opt_revision_t *>(
      apr_pmemdup(pool, &revision, sizeof(svn_opt_revision_t)));
  source->path = apr_pstrdup(pool, sourceBytes.constData());
  source->revision = sourceRevision;
  source->peg_revision = sourceRevision;

  apr_array_header_t *sources =
      apr_array_make(pool, 1, sizeof(svn_client_copy_source_t *));
  APR_ARRAY_PUSH(sources, svn_client_copy_source_t *) = source;

  return svn_client_copy7(sources, destinationBytes.constData(), FALSE, TRUE,
                          FALSE, FALSE, FALSE, nullptr, nullptr, nullptr,
                          nullptr, ctx, pool);
}

static svn_error_t *switchWorkingCopy(const QString &wcPath, const QString &url,
                                      svn_client_ctx_t *ctx, apr_pool_t *pool) {
  const QByteArray pathBytes = wcPath.toUtf8();
  const QByteArray urlBytes = url.toUtf8();
  const svn_opt_revision_t head = revisionHead();
  svn_revnum_t resultRevision = SVN_INVALID_REVNUM;
  return svn_client_switch3(
      &resultRevision, pathBytes.constData(), urlBytes.constData(), &head,
      &head, svn_depth_infinity, FALSE, FALSE, TRUE, FALSE, ctx, pool);
}

// Merges the changes of source between start and end into the working copy.
// Without a range, every revision not merged yet is merged.
//
// Merge tracking considers the revisions of the target's own history merged,
// so changes of the checked out branch itself are applied as a plain diff,
// like a Git cherry-pick.
static svn_error_t *mergeIntoWorkingCopy(
    const QString &wcPath, const QString &sourceUrl,
    const svn_opt_revision_t &peg, const svn_opt_revision_t *start,
    const svn_opt_revision_t *end, svn_client_ctx_t *ctx, apr_pool_t *pool,
    const bool dryRun = false) {
  WorkingCopyInfo target;
  SVN_ERR(readInfo(&target, wcPath, revisionUnspecified(), ctx, pool));
  const svn_boolean_t ignoreMergeinfo =
      trimmedUrl(target.url) == trimmedUrl(sourceUrl);

  apr_array_header_t *ranges = nullptr;
  if (start && end) {
    auto *range = static_cast<svn_opt_revision_range_t *>(
        apr_pcalloc(pool, sizeof(svn_opt_revision_range_t)));
    range->start = *start;
    range->end = *end;
    ranges = apr_array_make(pool, 1, sizeof(svn_opt_revision_range_t *));
    APR_ARRAY_PUSH(ranges, svn_opt_revision_range_t *) = range;
  }

  const QByteArray sourceBytes = sourceUrl.toUtf8();
  const QByteArray targetBytes = wcPath.toUtf8();
  return svn_client_merge_peg5(sourceBytes.constData(), ranges, &peg,
                               targetBytes.constData(), svn_depth_infinity,
                               ignoreMergeinfo, FALSE, FALSE, FALSE, dryRun,
                               TRUE, nullptr, ctx, pool);
}

// What a merge, cherry-pick or revert changes, from its notifications.
struct OperationNotifyBaton {
  QStringList paths;
  QStringList added;
  QStringList conflicted;
  // Paths the operation can't change: obstructed, missing or conflicted.
  QStringList skipped;
};

static void appendOnce(QStringList *list, const QString &path) {
  if (!list->contains(path)) {
    list->append(path);
  }
}

static void operationNotifyCallback(void *baton, const svn_wc_notify_t *notify,
                                    apr_pool_t *) {
  auto *payload = static_cast<OperationNotifyBaton *>(baton);
  if (!payload || !notify || !notify->path ||
      svn_path_is_url(notify->path)) {
    return;
  }

  const QString path =
      QDir::fromNativeSeparators(QString::fromUtf8(notify->path));
  switch (notify->action) {
  case svn_wc_notify_update_add:
  case svn_wc_notify_update_shadowed_add:
    appendOnce(&payload->added, path);
    appendOnce(&payload->paths, path);
    break;
  case svn_wc_notify_update_update:
  case svn_wc_notify_update_delete:
  case svn_wc_notify_update_replace:
  case svn_wc_notify_update_shadowed_update:
  case svn_wc_notify_update_shadowed_delete:
  case svn_wc_notify_exists:
    appendOnce(&payload->paths, path);
    break;
  case svn_wc_notify_tree_conflict:
    appendOnce(&payload->paths, path);
    appendOnce(&payload->conflicted, path);
    break;
  case svn_wc_notify_skip:
  case svn_wc_notify_skip_conflicted:
  case svn_wc_notify_update_skip_obstruction:
  case svn_wc_notify_update_skip_working_only:
    appendOnce(&payload->skipped, path);
    break;
  default:
    return;
  }

  if (notify->content_state == svn_wc_notify_state_conflicted ||
      notify->prop_state == svn_wc_notify_state_conflicted) {
    appendOnce(&payload->conflicted, path);
  }
}

// Installs a notification callback while alive.
class NotifyScope {
public:
  NotifyScope(svn_client_ctx_t *ctx, svn_wc_notify_func2_t func, void *baton)
      : _ctx(ctx), _func(ctx->notify_func2), _baton(ctx->notify_baton2) {
    _ctx->notify_func2 = func;
    _ctx->notify_baton2 = baton;
  }
  ~NotifyScope() {
    _ctx->notify_func2 = _func;
    _ctx->notify_baton2 = _baton;
  }

  NotifyScope(const NotifyScope &) = delete;
  NotifyScope &operator=(const NotifyScope &) = delete;

private:
  svn_client_ctx_t *_ctx;
  svn_wc_notify_func2_t _func;
  void *_baton;
};

struct PathStatus {
  svn_wc_status_kind status = svn_wc_status_none;
  svn_wc_status_kind propStatus = svn_wc_status_none;
  bool conflicted = false;
};

static svn_error_t *pathStatusCallback(void *baton, const char *,
                                       const svn_client_status_t *status,
                                       apr_pool_t *) {
  auto *payload = static_cast<PathStatus *>(baton);
  if (payload && status) {
    payload->status = status->node_status;
    payload->propStatus = status->prop_status;
    payload->conflicted = status->conflicted;
  }
  return SVN_NO_ERROR;
}

// Status of one path; a path below an unversioned folder is unversioned.
static svn_error_t *readPathStatus(PathStatus *result, const QString &path,
                                   svn_client_ctx_t *ctx, apr_pool_t *pool) {
  const QByteArray pathBytes = path.toUtf8();
  const svn_opt_revision_t revision = revisionWorking();
  svn_revnum_t youngest = SVN_INVALID_REVNUM;
  svn_error_t *error = svn_client_status6(
      &youngest, ctx, pathBytes.constData(), &revision, svn_depth_empty, TRUE,
      FALSE, TRUE, TRUE, TRUE, FALSE, nullptr, pathStatusCallback, result,
      pool);
  if (error && error->apr_err == SVN_ERR_WC_PATH_NOT_FOUND) {
    svn_error_clear(error);
    result->status = QFileInfo::exists(path) ? svn_wc_status_unversioned
                                             : svn_wc_status_none;
    return SVN_NO_ERROR;
  }
  return error;
}

static QString relativePaths(const QString &wcPath, QStringList paths) {
  for (QString &path : paths) {
    const QString relativePath = relativeFilePath(wcPath, path);
    path = relativePath.isEmpty() ? QStringLiteral(".") : relativePath;
  }
  paths.sort();
  return paths.join(QStringLiteral(", "));
}

// A merge, cherry-pick or revert (docs/SVN_TEST_PLAN.md 3.11.1).
struct OperationRequest {
  QString kind; // "merge", "cherry-pick" or "revert"
  // The branch the changes come from; empty for the checked out one.
  QString source;
  // Without a range, every revision not merged yet.
  bool hasRange = false;
  svn_revnum_t start = SVN_INVALID_REVNUM;
  svn_revnum_t end = SVN_INVALID_REVNUM;
  QString message;
  SvnError conflictsError = SvnError::MergeFailedWithConflicts;
  SvnError emptyError = SvnError::MergeIsEmpty;
};

// Applies the operation and commits exactly its changes, in one revision.
// A local change in a path it changes refuses it before anything is touched.
// With conflicts, or when the commit fails, it stays in progress until
// commitStaged or abortMerge ends it.
static bool runOperation(const QString &wcPath,
                         const OperationRequest &request) {
  const svn_opt_revision_t peg =
      request.hasRange ? revisionNumber((std::max)(request.start, request.end))
                       : revisionHead();
  const svn_opt_revision_t start = revisionNumber(request.start);
  const svn_opt_revision_t end = revisionNumber(request.end);
  const svn_opt_revision_t *startRevision = request.hasRange ? &start : nullptr;
  const svn_opt_revision_t *endRevision = request.hasRange ? &end : nullptr;

  OperationNotifyBaton dryRun;
  OperationNotifyBaton applied;
  QString sourceUrl;
  QString destination;
  bool recordsMergeinfo = false;
  const bool applies = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        WorkingCopyInfo info;
        if (!reportError(
                SvnError::InfoFailed,
                readInfo(&info, wcPath, revisionUnspecified(), ctx, pool))) {
          return false;
        }
        sourceUrl = request.source.isEmpty()
                        ? info.url
                        : branchUrl(info.layoutRootUrl, request.source,
                                    BranchType::Local) +
                              branchSubpath(info);
        recordsMergeinfo = trimmedUrl(info.url) != trimmedUrl(sourceUrl);
        destination =
            branchNameFromUrl(info.url, info.layoutRootUrl, BranchType::Local);

        // What it will change, without changing anything.
        {
          const NotifyScope scope(ctx, operationNotifyCallback, &dryRun);
          if (!reportError(SvnError::MergeFailed,
                           mergeIntoWorkingCopy(wcPath, sourceUrl, peg,
                                                startRevision, endRevision,
                                                ctx, pool, true))) {
            return false;
          }
        }

        // The data-loss guard: local changes in those paths, or in the
        // root's properties when the mergeinfo is recorded there.
        QStringList inTheWay = dryRun.skipped;
        for (const QString &operationPath : dryRun.paths) {
          PathStatus local;
          if (!reportError(SvnError::StatusFailed,
                           readPathStatus(&local, operationPath, ctx, pool))) {
            return false;
          }
          if (local.conflicted || (local.status != svn_wc_status_normal &&
                                   local.status != svn_wc_status_none)) {
            appendOnce(&inTheWay, operationPath);
          }
        }
        if (recordsMergeinfo) {
          PathStatus root;
          if (!reportError(SvnError::StatusFailed,
                           readPathStatus(&root, wcPath, ctx, pool))) {
            return false;
          }
          if (root.conflicted || root.propStatus == svn_wc_status_modified) {
            appendOnce(&inTheWay, wcPath);
          }
        }
        if (!inTheWay.isEmpty()) {
          setLastError(SvnError::LocalChangesInTheWay, -1,
                       QStringLiteral("Local changes are in the way of the "
                                      "%1: %2")
                           .arg(request.kind, relativePaths(wcPath, inTheWay)));
          return false;
        }

        // Only those paths, and the root, come to HEAD. They have no local
        // changes, so this can't conflict; everything else keeps its BASE.
        QStringList toUpdate{wcPath};
        for (const QString &operationPath : dryRun.paths) {
          appendOnce(&toUpdate, operationPath);
        }
        const PathArray updateTargets = makePathArray(pool, toUpdate);
        const svn_opt_revision_t head = revisionHead();
        apr_array_header_t *resultRevisions = nullptr;
        if (!reportError(SvnError::UpdateFailed,
                         svn_client_update4(&resultRevisions,
                                            updateTargets.array, &head,
                                            svn_depth_empty, FALSE, TRUE,
                                            FALSE, TRUE, FALSE, ctx, pool))) {
          return false;
        }

        const NotifyScope scope(ctx, operationNotifyCallback, &applied);
        return reportError(SvnError::MergeFailed,
                           mergeIntoWorkingCopy(wcPath, sourceUrl, peg,
                                                startRevision, endRevision, ctx,
                                                pool));
      },
      wcPath);
  if (!applies) {
    return false;
  }

  // Nothing but the mergeinfo: undone, and no revision.
  QStringList changed = applied.paths;
  changed.removeAll(wcPath);
  if (changed.isEmpty()) {
    if (recordsMergeinfo) {
      runWithClientContext(
          [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
            const PathArray root = makePathArray(pool, {wcPath});
            return reportError(SvnError::RevertFailed,
                               svn_client_revert4(root.array, svn_depth_empty,
                                                  nullptr, FALSE, FALSE, TRUE,
                                                  ctx, pool));
          },
          wcPath);
    }
    setLastError(request.emptyError, -1,
                 QStringLiteral("The %1 has nothing to change.")
                     .arg(request.kind));
    return false;
  }

  OperationState state;
  state.kind = request.kind;
  state.source = request.source;
  state.sourceUrl = sourceUrl;
  state.start = request.start;
  state.end = request.end;
  state.destination = destination;
  state.message = request.message;
  state.paths = changed;
  if (recordsMergeinfo) {
    state.paths.append(wcPath);
  }
  state.added = applied.added;
  state.conflicted = applied.conflicted;

  // Staged: what it changed without a conflict. The user's own changes stay
  // unstaged.
  QStringList toStage;
  for (const QString &operationPath : state.paths) {
    if (!state.conflicted.contains(operationPath)) {
      toStage.append(operationPath);
    }
  }
  if (!toStage.isEmpty() && !stageFiles(wcPath, toStage)) {
    writeOperationState(wcPath, state);
    return false;
  }

  if (!state.conflicted.isEmpty()) {
    writeOperationState(wcPath, state);
    setLastError(request.conflictsError, -1,
                 QStringLiteral("The %1 left conflicts in: %2")
                     .arg(request.kind,
                          relativePaths(wcPath, state.conflicted)));
    return false;
  }

  const bool committed = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        return reportError(SvnError::CommitFailed,
                           commitPaths(state.paths, request.message, ctx,
                                       pool));
      },
      wcPath);
  if (!committed) {
    writeOperationState(wcPath, state);
    return false;
  }

  QSet<QString> stagedDirectories = readStagedDirectories(wcPath);
  for (const QString &operationPath : state.paths) {
    stagedDirectories.remove(operationPath);
  }
  writeStagedDirectories(wcPath, stagedDirectories);
  return true;
}

// Branch whose history contains revision. A revision can change several
// branches: the working copy's own branch is taken when preferActive is set
// (a revert) and avoided otherwise (a cherry-pick); among the others the
// first by name, so the choice doesn't depend on hash order.
static QString branchOfRevision(const QString &wcPath,
                                const svn_revnum_t revision,
                                const bool preferActive) {
  const bool quit = false;
  QStringList branches =
      getCommitBranches(wcPath, QString::number(revision), quit);
  if (branches.isEmpty()) {
    return {};
  }

  branches.sort();
  const QString active = getActiveBranch(wcPath, BranchType::Local, quit);
  if (branches.contains(active)) {
    if (preferActive) {
      return active;
    }
    if (branches.size() > 1) {
      branches.removeOne(active);
    }
  }
  return branches.first();
}

static QString commitSummary(const QString &wcPath,
                             const svn_revnum_t revision) {
  return getCommitStatus(wcPath, QString::number(revision))
      .message.section('\n', 0, 0)
      .trimmed();
}

} // namespace

QString getActiveBranch(const QString &path, BranchType type,
                        const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  if (quitFlag) {
    LOG_PERF(0, "Exit");
    return {};
  }

  WorkingCopyInfo info;
  if (!readInfo(&info, normalizedPath(path))) {
    LOG_PERF(0, "Exit");
    return {};
  }

  const QString activeBranch =
      branchNameFromUrl(info.url, info.layoutRootUrl, type);

  LOG_PERF(0, "Exit");
  return activeBranch;
}

QStringList listLocalBranches(const QString &path, const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  if (quitFlag) {
    LOG_PERF(0, "Exit");
    return {};
  }

  // Every branch of the repository can be switched to: trunk and the
  // directories in branches.
  const QString wcPath = normalizedPath(path);
  const QuitScope quitScope(&quitFlag);

  QStringList branches;
  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        WorkingCopyInfo info;
        if (!reportError(
                SvnError::InfoFailed,
                readInfo(&info, wcPath, revisionUnspecified(), ctx, pool))) {
          return false;
        }

        QStringList rootEntries;
        if (!readRootEntries(info, &rootEntries, ctx, pool)) {
          return false;
        }

        if (rootEntries.contains(QStringLiteral("branches"))) {
          if (!readChildren(info, QStringLiteral("branches"), &branches, ctx,
                            pool)) {
            return false;
          }
        }

        if (rootEntries.contains(QStringLiteral("trunk"))) {
          branches.prepend(QStringLiteral("trunk"));
        }

        return true;
      },
      wcPath);

  LOG_PERF(0, "Exit");
  return success ? branches : QStringList();
}

QStringList listRemoteBranches(const QString &path, const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  if (quitFlag) {
    LOG_PERF(0, "Exit");
    return {};
  }

  const QString wcPath = normalizedPath(path);
  const QuitScope quitScope(&quitFlag);

  QStringList remoteBranches;
  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        WorkingCopyInfo info;
        if (!reportError(
                SvnError::InfoFailed,
                readInfo(&info, wcPath, revisionUnspecified(), ctx, pool))) {
          return false;
        }

        QStringList rootEntries;
        if (!readRootEntries(info, &rootEntries, ctx, pool)) {
          return false;
        }

        if (rootEntries.contains(QStringLiteral("branches"))) {
          return readChildren(info, QStringLiteral("branches"), &remoteBranches,
                              ctx, pool);
        }

        // Repositories without the standard layout: the top level
        // directories are the branches.
        for (const QString &entry : rootEntries) {
          if (entry != QStringLiteral("trunk") &&
              entry != QStringLiteral("tags") &&
              entry != QStringLiteral("branches")) {
            remoteBranches.append(entry);
          }
        }

        return true;
      },
      wcPath);

  LOG_PERF(0, "Exit");
  return success ? remoteBranches : QStringList();
}

namespace {

struct CountBaton {
  int count = 0;
};

static svn_error_t *countLogEntry(void *baton, svn_log_entry_t *entry,
                                  apr_pool_t *) {
  if (entry && SVN_IS_VALID_REVNUM(entry->revision)) {
    ++static_cast<CountBaton *>(baton)->count;
  }
  return SVN_NO_ERROR;
}

// Number of revisions committed to the branch of the working copy since it
// was updated.
static svn_error_t *countRevisionsBehind(int *count,
                                         const WorkingCopyInfo &info,
                                         svn_client_ctx_t *ctx,
                                         apr_pool_t *pool) {
  *count = 0;

  // The newest revision the working copy has, also of its own commits.
  svn_revnum_t newest = SVN_INVALID_REVNUM;
  SVN_ERR(readMaxRevision(&newest, info.wcRoot, false, ctx, pool));
  WorkingCopyInfo head;
  SVN_ERR(readInfo(&head, info.url, revisionHead(), ctx, pool));
  if (!SVN_IS_VALID_REVNUM(newest) || head.lastChangedRevision <= newest) {
    return SVN_NO_ERROR;
  }

  const PathArray targets = makePathArray(pool, {info.url});
  const svn_opt_revision_t peg = revisionHead();
  auto *range = static_cast<svn_opt_revision_range_t *>(
      apr_pcalloc(pool, sizeof(svn_opt_revision_range_t)));
  range->start = revisionHead();
  range->end = revisionNumber(newest + 1);
  apr_array_header_t *ranges =
      apr_array_make(pool, 1, sizeof(svn_opt_revision_range_t *));
  APR_ARRAY_PUSH(ranges, svn_opt_revision_range_t *) = range;
  apr_array_header_t *revisionProperties =
      apr_array_make(pool, 0, sizeof(const char *));

  CountBaton baton;
  SVN_ERR(svn_client_log5(targets.array, &peg, ranges, 0, FALSE, FALSE, FALSE,
                          revisionProperties, countLogEntry, &baton, ctx,
                          pool));
  *count = baton.count;
  return SVN_NO_ERROR;
}

} // namespace

QVector<BranchInfo> listRemoteBranchesWithStatus(const QString &path,
                                                 const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  // Every branch, trunk too.
  const QString wcPath = normalizedPath(path);
  const QStringList remoteBranches = listLocalBranches(wcPath, quitFlag);
  if (quitFlag) {
    LOG_PERF(0, "Exit");
    return {};
  }

  // Local commits do not exist, so a branch can only be behind: the
  // checked out branch has revisions the working copy has not been updated
  // to.
  const QuitScope quitScope(&quitFlag);
  QString activeBranch;
  int behind = 0;
  runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        WorkingCopyInfo info;
        if (!reportError(
                SvnError::InfoFailed,
                readInfo(&info, wcPath, revisionUnspecified(), ctx, pool))) {
          return false;
        }

        activeBranch =
            branchNameFromUrl(info.url, info.layoutRootUrl, BranchType::Local);
        return reportError(SvnError::HistoryFailed,
                           countRevisionsBehind(&behind, info, ctx, pool));
      },
      wcPath);

  QVector<BranchInfo> branchesWithStatus;
  for (const QString &remoteBranchName : remoteBranches) {
    BranchInfo branchInfo;
    branchInfo.name = remoteBranchName;
    branchInfo.statusInfo.status = BranchStatusType::UpToDate;
    if (remoteBranchName == activeBranch && behind > 0) {
      branchInfo.statusInfo.status = BranchStatusType::Behind;
      branchInfo.statusInfo.behindCount = static_cast<size_t>(behind);
    }
    branchesWithStatus.append(branchInfo);
  }

  LOG_PERF(0, "Exit");
  return branchesWithStatus;
}

bool checkoutBranch(const QString &path, const QString &branchOrTagName,
                    BranchType type) {
  LOG_PERF(0, "Enter");

  const QString wcPath = normalizedPath(path);
  if (refuseWhileOperationInProgress(wcPath)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        WorkingCopyInfo info;
        if (!reportError(
                SvnError::InfoFailed,
                readInfo(&info, wcPath, revisionUnspecified(), ctx, pool))) {
          return false;
        }

        // A working copy checked out at the project root holds trunk, branches
        // and tags side by side. Subversion can only switch to URLs that share
        // history with the working copy URL, and branches/X does not share
        // history with the project root, so the switch would fail.
        if (trimmedUrl(info.url) == trimmedUrl(info.layoutRootUrl)) {
          setLastError(
              SvnError::SwitchFailed, -1,
              QStringLiteral("The working copy is at the project root '%1'. "
                             "Check out trunk or a branch to switch between "
                             "branches.")
                  .arg(info.url));
          return false;
        }

        // Local changes are carried over to the switched working copy; a
        // conflict with them is reported.
        const ConflictWatch conflicts(ctx);
        return reportError(
                   SvnError::SwitchFailed,
                   switchWorkingCopy(wcPath,
                                     branchUrl(info.layoutRootUrl,
                                               branchOrTagName,
                                               type == BranchType::Tag
                                                   ? BranchType::Tag
                                                   : BranchType::Local) +
                                         branchSubpath(info),
                                     ctx, pool)) &&
               conflicts.reportConflicts(SvnError::SwitchFailed, wcPath,
                                         QStringLiteral("The switch"));
      },
      wcPath);

  LOG_PERF(0, "Exit");
  return success;
}

bool createBranchFromCommit(const QString &path, const QString &commit,
                            const QString &branchName) {
  LOG_PERF(0, "Enter");

  svn_revnum_t revision = SVN_INVALID_REVNUM;
  if (!checkRefName(branchName, BranchType::Local) ||
      (!commit.isEmpty() && !parseRevision(commit, &revision))) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString wcPath = normalizedPath(path);
  WorkingCopyInfo wcInfo;
  if (!readInfo(&wcInfo, wcPath) || !checkOnBranch(wcInfo)) {
    LOG_PERF(0, "Exit");
    return false;
  }
  const QString message =
      SVN_IS_VALID_REVNUM(revision)
          ? QStringLiteral("Create branch %1 from r%2")
                .arg(branchName)
                .arg(revision)
          : QStringLiteral("Create branch %1").arg(branchName);

  const bool success = runRepositoryCommit(
      wcPath, message,
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool,
          const WorkingCopyInfo &info) -> svn_error_t * {
        // The whole branch, also from a working copy of one of its folders.
        return copyUrl(
            branchRootUrl(info),
            SVN_IS_VALID_REVNUM(revision) ? revisionNumber(revision)
                                          : revisionHead(),
            branchUrl(info.layoutRootUrl, branchName, BranchType::Local), ctx,
            pool);
      },
      SvnError::CopyFailed);

  LOG_PERF(0, "Exit");
  return success;
}

bool deleteBranch(const QString &path, const QString &branchName) {
  LOG_PERF(0, "Enter");

  if (!checkRefName(branchName, BranchType::Local)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString wcPath = normalizedPath(path);
  const bool quit = false;
  if (getActiveBranch(wcPath, BranchType::Local, quit) == branchName) {
    setLastError(SvnError::DeleteFailed, -1,
                 QStringLiteral("The checked out branch '%1' cannot be "
                                "deleted.")
                     .arg(branchName));
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool success = runRepositoryCommit(
      wcPath, QStringLiteral("Delete branch %1").arg(branchName),
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool,
          const WorkingCopyInfo &info) -> svn_error_t * {
        // Not the folder the working copy is in, at any depth.
        const QString url =
            branchUrl(info.layoutRootUrl, branchName, BranchType::Local);
        const QString wcUrl = trimmedUrl(info.url);
        if (wcUrl == url || wcUrl.startsWith(url + QLatin1Char('/'))) {
          return svn_error_createf(
              SVN_ERR_ILLEGAL_TARGET, nullptr,
              "The working copy is in '%s', which can't be deleted.",
              branchName.toUtf8().constData());
        }
        const PathArray urls = makePathArray(pool, {url});
        return svn_client_delete4(urls.array, FALSE, FALSE, nullptr, nullptr,
                                  nullptr, ctx, pool);
      },
      SvnError::DeleteFailed);

  LOG_PERF(0, "Exit");
  return success;
}

bool deleteRemoteBranch(const QString &path, const QString &branchName) {
  // Every branch lives in the repository.
  return deleteBranch(path, branchName);
}

bool renameBranch(const QString &path, const QString &oldBranchName,
                  const QString &newBranchName) {
  LOG_PERF(0, "Enter");

  if (!checkRefName(oldBranchName, BranchType::Local) ||
      !checkRefName(newBranchName, BranchType::Local)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString wcPath = normalizedPath(path);
  const bool quit = false;
  const bool active =
      getActiveBranch(wcPath, BranchType::Local, quit) == oldBranchName;

  const bool success = runRepositoryCommit(
      wcPath,
      QStringLiteral("Rename branch %1 to %2")
          .arg(oldBranchName, newBranchName),
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool,
          const WorkingCopyInfo &info) -> svn_error_t * {
        const PathArray sources = makePathArray(
            pool,
            {branchUrl(info.layoutRootUrl, oldBranchName, BranchType::Local)});
        const QString destination =
            branchUrl(info.layoutRootUrl, newBranchName, BranchType::Local);
        const QByteArray destinationBytes = destination.toUtf8();
        SVN_ERR(svn_client_move7(sources.array, destinationBytes.constData(),
                                 FALSE, TRUE, FALSE, FALSE, nullptr, nullptr,
                                 nullptr, ctx, pool));

        // The working copy has to follow its branch.
        if (active) {
          SVN_ERR(switchWorkingCopy(wcPath, destination + branchSubpath(info),
                                    ctx, pool));
        }

        return SVN_NO_ERROR;
      },
      SvnError::MoveFailed);

  LOG_PERF(0, "Exit");
  return success;
}

QStringList listTags(const QString &path) {
  LOG_PERF(0, "Enter");

  const QString wcPath = normalizedPath(path);
  QStringList tags;
  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        WorkingCopyInfo info;
        if (!reportError(
                SvnError::InfoFailed,
                readInfo(&info, wcPath, revisionUnspecified(), ctx, pool))) {
          return false;
        }

        return readChildren(info, QStringLiteral("tags"), &tags, ctx, pool);
      },
      wcPath);

  LOG_PERF(0, "Exit");
  return success ? tags : QStringList();
}

QStringList listRemoteTags(const QString &path) {
  // Every tag lives in the repository.
  return listTags(path);
}

bool createTagFromCommit(const QString &path, const QString &commit,
                         const QString &tagName, const QString &author,
                         const QString &email, const QString &message) {
  LOG_PERF(0, "Enter");

  svn_revnum_t revision = SVN_INVALID_REVNUM;
  if (!checkRefName(tagName, BranchType::Tag) ||
      (!commit.isEmpty() && !parseRevision(commit, &revision))) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString wcPath = normalizedPath(path);
  WorkingCopyInfo wcInfo;
  if (!readInfo(&wcInfo, wcPath) || !checkOnBranch(wcInfo)) {
    LOG_PERF(0, "Exit");
    return false;
  }
  const QString logMessage =
      message.trimmed().isEmpty()
          ? QStringLiteral("Create tag %1").arg(tagName)
          : makeLogMessage(message, QString(), true, false, author, email);

  const bool success = runRepositoryCommit(
      wcPath, logMessage,
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool,
          const WorkingCopyInfo &info) -> svn_error_t * {
        // The whole branch, also from a working copy of one of its folders.
        return copyUrl(branchRootUrl(info),
                       SVN_IS_VALID_REVNUM(revision) ? revisionNumber(revision)
                                                     : revisionHead(),
                       branchUrl(info.layoutRootUrl, tagName, BranchType::Tag),
                       ctx, pool);
      },
      SvnError::CopyFailed);

  LOG_PERF(0, "Exit");
  return success;
}

bool deleteTag(const QString &path, const QString &tagName) {
  LOG_PERF(0, "Enter");

  if (!checkRefName(tagName, BranchType::Tag)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool success = runRepositoryCommit(
      normalizedPath(path), QStringLiteral("Delete tag %1").arg(tagName),
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool,
          const WorkingCopyInfo &info) {
        const PathArray urls = makePathArray(
            pool, {branchUrl(info.layoutRootUrl, tagName, BranchType::Tag)});
        return svn_client_delete4(urls.array, FALSE, FALSE, nullptr, nullptr,
                                  nullptr, ctx, pool);
      },
      SvnError::DeleteFailed);

  LOG_PERF(0, "Exit");
  return success;
}

bool deleteRemoteTag(const QString &path, const QString &tagName) {
  return deleteTag(path, tagName);
}

bool renameTag(const QString &path, const QString &oldTagName,
               const QString &newTagName) {
  LOG_PERF(0, "Enter");

  if (!checkRefName(oldTagName, BranchType::Tag) ||
      !checkRefName(newTagName, BranchType::Tag)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool success = runRepositoryCommit(
      normalizedPath(path),
      QStringLiteral("Rename tag %1 to %2").arg(oldTagName, newTagName),
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool,
          const WorkingCopyInfo &info) {
        const PathArray sources = makePathArray(
            pool, {branchUrl(info.layoutRootUrl, oldTagName, BranchType::Tag)});
        const QByteArray destination =
            branchUrl(info.layoutRootUrl, newTagName, BranchType::Tag).toUtf8();
        return svn_client_move7(sources.array, destination.constData(), FALSE,
                                TRUE, FALSE, FALSE, nullptr, nullptr, nullptr,
                                ctx, pool);
      },
      SvnError::MoveFailed);

  LOG_PERF(0, "Exit");
  return success;
}

bool pushTag(const QString &, const QString &) {
  // Tags are created in the repository.
  return true;
}

bool pushAllTags(const QString &) {
  return true;
}

bool merge(const QString &path, const QString &sourceBranch,
           const QString &destinationBranch, const QString &author,
           const QString &email) {
  LOG_PERF(0, "Enter");

  const QString wcPath = normalizedPath(path);
  if (refuseWhileOperationInProgress(wcPath)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool quit = false;
  const QString active = getActiveBranch(wcPath, BranchType::Local, quit);
  const QString destination =
      destinationBranch.isEmpty() ? active : destinationBranch;
  if (sourceBranch.trimmed().isEmpty() || sourceBranch == destination) {
    setLastError(SvnError::InvalidArgument, -1,
                 sourceBranch.trimmed().isEmpty()
                     ? QStringLiteral("There is no branch to merge.")
                     : QStringLiteral("A branch can't be merged into itself."));
    LOG_PERF(0, "Exit");
    return false;
  }

  // Local changes go along to the destination, as with git checkout; a
  // conflict with them stops the merge there.
  if (destination != active) {
    if (!checkoutBranch(wcPath, destination, BranchType::Local)) {
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  OperationRequest request;
  request.kind = QStringLiteral("merge");
  request.source = sourceBranch;
  request.message = makeLogMessage(QStringLiteral("Merge branch '%1' into '%2'")
                                       .arg(sourceBranch, destination),
                                   QString(), true, false, author, email);
  request.conflictsError = SvnError::MergeFailedWithConflicts;
  request.emptyError = SvnError::MergeIsEmpty;
  const bool success = runOperation(wcPath, request);

  LOG_PERF(0, "Exit");
  return success;
}

bool cherryPickCommit(const QString &path, const QString &commit,
                      const QString &author, const QString &email) {
  LOG_PERF(0, "Enter");

  svn_revnum_t revision = SVN_INVALID_REVNUM;
  if (!parseRevision(commit, &revision) || revision < 1) {
    if (revision == 0) {
      setLastError(SvnError::InvalidRevision, -1,
                   QStringLiteral("Revision 0 has no changes."));
    }
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString wcPath = normalizedPath(path);
  if (refuseWhileOperationInProgress(wcPath)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  OperationRequest request;
  request.kind = QStringLiteral("cherry-pick");
  request.source = branchOfRevision(wcPath, revision, false);
  request.hasRange = true;
  request.start = revision - 1;
  request.end = revision;
  request.message = makeLogMessage(
      commitSummary(wcPath, revision),
      QStringLiteral("(cherry picked from r%1)").arg(revision), true, false,
      author, email);
  request.conflictsError = SvnError::CherryPickFailedWithConflicts;
  request.emptyError = SvnError::CherryPickIsEmpty;
  const bool success = runOperation(wcPath, request);

  LOG_PERF(0, "Exit");
  return success;
}

bool revertCommit(const QString &path, const QString &commit,
                  const QString &author, const QString &email,
                  const QString &message) {
  LOG_PERF(0, "Enter");

  svn_revnum_t revision = SVN_INVALID_REVNUM;
  if (!parseRevision(commit, &revision) || revision < 1) {
    if (revision == 0) {
      setLastError(SvnError::InvalidRevision, -1,
                   QStringLiteral("Revision 0 has no changes."));
    }
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString wcPath = normalizedPath(path);
  if (refuseWhileOperationInProgress(wcPath)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  // A reverse merge of the revision.
  OperationRequest request;
  request.kind = QStringLiteral("revert");
  request.source = branchOfRevision(wcPath, revision, true);
  request.hasRange = true;
  request.start = revision;
  request.end = revision - 1;
  request.message =
      message.trimmed().isEmpty()
          ? makeLogMessage(
                QStringLiteral("Revert \"%1\"")
                    .arg(commitSummary(wcPath, revision)),
                QStringLiteral("This reverts revision r%1.").arg(revision),
                true, false, author, email)
          : makeLogMessage(message, QString(), true, false, author, email);
  request.conflictsError = SvnError::RevertFailedWithConflicts;
  request.emptyError = SvnError::RevertIsEmpty;
  const bool success = runOperation(wcPath, request);

  LOG_PERF(0, "Exit");
  return success;
}

} // namespace SvnRepo
