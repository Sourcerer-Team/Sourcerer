#include "SvnRepo.h"
#include "SvnRepoPrivate.h"

#include <Log.h>

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
  if (error && error->apr_err != SVN_ERR_CANCELLED) {
    svn_error_clear(error);
    names->clear();
    return true;
  }

  return reportError(SvnError::ErrorWhileOpening, error);
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

static svn_error_t *updateWorkingCopy(const QString &wcPath,
                                      svn_client_ctx_t *ctx, apr_pool_t *pool) {
  const PathArray paths = makePathArray(pool, {wcPath});
  const svn_opt_revision_t head = revisionHead();
  apr_array_header_t *resultRevisions = nullptr;
  return svn_client_update4(&resultRevisions, paths.array, &head,
                            svn_depth_infinity, FALSE, FALSE, TRUE, TRUE, FALSE,
                            ctx, pool);
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
    const svn_opt_revision_t *end, svn_client_ctx_t *ctx, apr_pool_t *pool) {
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
                               ignoreMergeinfo, FALSE, FALSE, FALSE, FALSE,
                               TRUE, nullptr, ctx, pool);
}

// Commits a merge result unless it left conflicts. A conflicted merge is
// completed by committing after the conflicts are resolved.
static bool commitMergeResult(const QString &wcPath, const QString &message) {
  if (hasConflicts(wcPath)) {
    return true;
  }

  return runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        return reportError(SvnError::CommitFailed,
                           commitWorkingCopy(wcPath, message, ctx, pool));
      },
      wcPath);
}

// Branch whose history contains revision.
static QString branchOfRevision(const QString &wcPath,
                                const svn_revnum_t revision) {
  const bool quit = false;
  const QStringList branches =
      getCommitBranches(wcPath, QString::number(revision), quit);
  return branches.isEmpty() ? QString() : branches.first();
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

  WorkingCopyInfo head;
  SVN_ERR(readInfo(&head, info.url, revisionHead(), ctx, pool));
  if (!SVN_IS_VALID_REVNUM(info.revision) ||
      head.lastChangedRevision <= info.revision) {
    return SVN_NO_ERROR;
  }

  const PathArray targets = makePathArray(pool, {info.url});
  const svn_opt_revision_t peg = revisionHead();
  auto *range = static_cast<svn_opt_revision_range_t *>(
      apr_pcalloc(pool, sizeof(svn_opt_revision_range_t)));
  range->start = revisionHead();
  range->end = revisionNumber(info.revision + 1);
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

  const QString wcPath = normalizedPath(path);
  const QStringList remoteBranches = listRemoteBranches(wcPath, quitFlag);
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

        // Local changes are carried over to the switched working copy.
        return reportError(
            SvnError::SwitchFailed,
            switchWorkingCopy(wcPath,
                              branchUrl(info.layoutRootUrl, branchOrTagName,
                                        type == BranchType::Tag
                                            ? BranchType::Tag
                                            : BranchType::Local),
                              ctx, pool));
      },
      wcPath);

  LOG_PERF(0, "Exit");
  return success;
}

bool createBranchFromCommit(const QString &path, const QString &commit,
                            const QString &branchName) {
  LOG_PERF(0, "Enter");

  svn_revnum_t revision = SVN_INVALID_REVNUM;
  if (!commit.isEmpty() && !parseRevision(commit, &revision)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString wcPath = normalizedPath(path);
  const QString message =
      SVN_IS_VALID_REVNUM(revision)
          ? QStringLiteral("Create branch %1 from r%2")
                .arg(branchName)
                .arg(revision)
          : QStringLiteral("Create branch %1").arg(branchName);

  const bool success = runRepositoryCommit(
      wcPath, message,
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool,
          const WorkingCopyInfo &info) {
        return copyUrl(
            info.url,
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
          const WorkingCopyInfo &info) {
        const PathArray urls = makePathArray(
            pool,
            {branchUrl(info.layoutRootUrl, branchName, BranchType::Local)});
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
          SVN_ERR(switchWorkingCopy(wcPath, destination, ctx, pool));
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
  if (!commit.isEmpty() && !parseRevision(commit, &revision)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString wcPath = normalizedPath(path);
  const QString logMessage =
      message.trimmed().isEmpty()
          ? QStringLiteral("Create tag %1").arg(tagName)
          : makeLogMessage(message, QString(), true, false, author, email);

  const bool success = runRepositoryCommit(
      wcPath, logMessage,
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool,
          const WorkingCopyInfo &info) {
        return copyUrl(info.url,
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
  const bool quit = false;
  if (!destinationBranch.isEmpty() &&
      getActiveBranch(wcPath, BranchType::Local, quit) != destinationBranch &&
      !checkoutBranch(wcPath, destinationBranch, BranchType::Local)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool merged = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        WorkingCopyInfo info;
        if (!reportError(
                SvnError::InfoFailed,
                readInfo(&info, wcPath, revisionUnspecified(), ctx, pool))) {
          return false;
        }

        // Merges need a working copy at a single revision.
        if (!reportError(SvnError::UpdateFailed,
                         updateWorkingCopy(wcPath, ctx, pool))) {
          return false;
        }

        return reportError(
            SvnError::MergeFailed,
            mergeIntoWorkingCopy(
                wcPath,
                branchUrl(info.layoutRootUrl, sourceBranch, BranchType::Local),
                revisionHead(), nullptr, nullptr, ctx, pool));
      },
      wcPath);

  const bool success =
      merged &&
      commitMergeResult(
          wcPath, makeLogMessage(QStringLiteral("Merge branch '%1' into %2")
                                     .arg(sourceBranch,
                                          getActiveBranch(
                                              wcPath, BranchType::Local, quit)),
                                 QString(), true, false, author, email));

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
  const QString sourceBranch = branchOfRevision(wcPath, revision);
  const QString summary = commitSummary(wcPath, revision);

  const bool merged = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        WorkingCopyInfo info;
        if (!reportError(
                SvnError::InfoFailed,
                readInfo(&info, wcPath, revisionUnspecified(), ctx, pool))) {
          return false;
        }

        if (!reportError(SvnError::UpdateFailed,
                         updateWorkingCopy(wcPath, ctx, pool))) {
          return false;
        }

        const QString sourceUrl =
            sourceBranch.isEmpty() ? info.url
                                   : branchUrl(info.layoutRootUrl, sourceBranch,
                                               BranchType::Local);
        const svn_opt_revision_t peg = revisionNumber(revision);
        const svn_opt_revision_t start = revisionNumber(revision - 1);
        return reportError(SvnError::MergeFailed,
                           mergeIntoWorkingCopy(wcPath, sourceUrl, peg, &start,
                                                &peg, ctx, pool));
      },
      wcPath);

  const bool success =
      merged &&
      commitMergeResult(
          wcPath,
          makeLogMessage(
              summary, QStringLiteral("(cherry picked from r%1)").arg(revision),
              true, false, author, email));

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
  const QString sourceBranch = branchOfRevision(wcPath, revision);
  const QString logMessage =
      message.trimmed().isEmpty()
          ? makeLogMessage(
                QStringLiteral("Revert \"%1\"")
                    .arg(commitSummary(wcPath, revision)),
                QStringLiteral("This reverts revision r%1.").arg(revision),
                true, false, author, email)
          : makeLogMessage(message, QString(), true, false, author, email);

  const bool merged = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        WorkingCopyInfo info;
        if (!reportError(
                SvnError::InfoFailed,
                readInfo(&info, wcPath, revisionUnspecified(), ctx, pool))) {
          return false;
        }

        if (!reportError(SvnError::UpdateFailed,
                         updateWorkingCopy(wcPath, ctx, pool))) {
          return false;
        }

        // A reverse merge of the revision.
        const QString sourceUrl =
            sourceBranch.isEmpty() ? info.url
                                   : branchUrl(info.layoutRootUrl, sourceBranch,
                                               BranchType::Local);
        const svn_opt_revision_t peg = revisionNumber(revision);
        const svn_opt_revision_t end = revisionNumber(revision - 1);
        return reportError(SvnError::MergeFailed,
                           mergeIntoWorkingCopy(wcPath, sourceUrl, peg, &peg,
                                                &end, ctx, pool));
      },
      wcPath);

  const bool success = merged && commitMergeResult(wcPath, logMessage);

  LOG_PERF(0, "Exit");
  return success;
}

} // namespace SvnRepo
