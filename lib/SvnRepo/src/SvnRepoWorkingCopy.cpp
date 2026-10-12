#include "SvnRepo.h"
#include "SvnRepoPrivate.h"

#include <svn_props.h>
#include <svn_string.h>
#include <svn_wc.h>

#include <Log.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>

#define THREAD_ID "SvnRepo"

namespace SvnRepo {

using namespace Private;

namespace {

struct PatchBaton {
  QStringList rejectedFiles;
};

static svn_error_t *patchCallback(void *baton, svn_boolean_t *filtered,
                                  const char *canonPathFromPatchfile,
                                  const char *, const char *rejectAbspath,
                                  apr_pool_t *) {
  if (filtered) {
    *filtered = FALSE;
  }

  // The reject file holds the hunks that could not be applied.
  auto *payload = static_cast<PatchBaton *>(baton);
  if (payload && rejectAbspath &&
      QFileInfo(QString::fromUtf8(rejectAbspath)).size() > 0) {
    payload->rejectedFiles.append(QString::fromUtf8(
        canonPathFromPatchfile ? canonPathFromPatchfile : ""));
  }
  return SVN_NO_ERROR;
}

struct NodeStatusBaton {
  svn_wc_status_kind status = svn_wc_status_none;
  svn_node_kind_t kind = svn_node_unknown;
  bool copied = false;
};

static svn_error_t *nodeStatusCallback(void *baton, const char *,
                                       const svn_client_status_t *status,
                                       apr_pool_t *) {
  auto *payload = static_cast<NodeStatusBaton *>(baton);
  if (payload && status) {
    payload->status = status->node_status;
    payload->kind = status->kind;
    payload->copied = status->copied;
  }
  return SVN_NO_ERROR;
}

// Status of a single working copy path.
static svn_error_t *readNodeStatus(NodeStatusBaton *result,
                                   const QString &absolutePath,
                                   svn_client_ctx_t *ctx, apr_pool_t *pool) {
  const QByteArray pathBytes = absolutePath.toUtf8();
  const svn_opt_revision_t revision = revisionWorking();
  svn_revnum_t youngest = SVN_INVALID_REVNUM;
  return svn_client_status6(&youngest, ctx, pathBytes.constData(), &revision,
                            svn_depth_empty, TRUE, FALSE, TRUE, TRUE, TRUE,
                            FALSE, nullptr, nodeStatusCallback, result, pool);
}

static bool removeFromDisk(const QString &absolutePath) {
  const QFileInfo info(absolutePath);
  if (info.isDir() && !info.isSymLink()) {
    return QDir(absolutePath).removeRecursively();
  }

  return !info.exists() || QFile::remove(absolutePath);
}

} // namespace

SvnStatus getStatus(const QString &path, const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  if (quitFlag) {
    LOG_PERF(0, "Exit");
    return {};
  }

  const SvnStatus status = readStatus(path, &quitFlag);

  LOG_PERF(0, "Exit");
  return status;
}

bool hasConflicts(const QString &path) {
  return !readStatus(path, nullptr).conflictedFiles.isEmpty();
}

bool stageFiles(const QString &path, const QStringList &files) {
  LOG_PERF(0, "Enter");

  // There is no staging area: unversioned files are scheduled for addition
  // and missing files for deletion. Modified files are always committed.
  const QString wcPath = normalizedPath(path);
  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        for (const QString &file : files) {
          const QString absolutePath = absoluteFilePath(wcPath, file);

          NodeStatusBaton node;
          if (!reportError(SvnError::StatusFailed,
                           readNodeStatus(&node, absolutePath, ctx, pool))) {
            return false;
          }

          const QByteArray pathBytes = absolutePath.toUtf8();
          if (node.status == svn_wc_status_unversioned) {
            if (!reportError(SvnError::AddFailed,
                             svn_client_add5(pathBytes.constData(),
                                             svn_depth_infinity, FALSE, FALSE,
                                             FALSE, TRUE, ctx, pool))) {
              return false;
            }
          } else if (node.status == svn_wc_status_missing) {
            const PathArray targets = makePathArray(pool, {absolutePath});
            if (!reportError(SvnError::DeleteFailed,
                             svn_client_delete4(targets.array, TRUE, FALSE,
                                                nullptr, nullptr, nullptr, ctx,
                                                pool))) {
              return false;
            }
          }
        }

        return true;
      },
      wcPath);

  LOG_PERF(0, "Exit");
  return success;
}

bool unstageFiles(const QString &path, const QStringList &files) {
  LOG_PERF(0, "Enter");

  // Undoes scheduled additions and deletions and keeps the files on disk.
  const QString wcPath = normalizedPath(path);
  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        QStringList targets;
        for (const QString &file : files) {
          const QString absolutePath = absoluteFilePath(wcPath, file);

          NodeStatusBaton node;
          if (!reportError(SvnError::StatusFailed,
                           readNodeStatus(&node, absolutePath, ctx, pool))) {
            return false;
          }

          switch (node.status) {
          case svn_wc_status_added:
          case svn_wc_status_deleted:
          case svn_wc_status_replaced:
            targets.append(absolutePath);
            break;
          case svn_wc_status_modified:
          case svn_wc_status_merged:
          case svn_wc_status_conflicted:
            setLastError(SvnError::RevertFailed, -1,
                         QStringLiteral("Subversion has no staging area, "
                                        "modifications of '%1' are always "
                                        "committed. Discard them instead.")
                             .arg(file));
            return false;
          default:
            break;
          }
        }

        if (targets.isEmpty()) {
          return true;
        }

        const PathArray array = makePathArray(pool, targets);
        return reportError(SvnError::RevertFailed,
                           svn_client_revert4(array.array, svn_depth_infinity,
                                              nullptr, FALSE, TRUE, TRUE, ctx,
                                              pool));
      },
      wcPath);

  LOG_PERF(0, "Exit");
  return success;
}

bool discardFiles(const QString &path, const QStringList &files) {
  LOG_PERF(0, "Enter");

  const QString wcPath = normalizedPath(path);
  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        QStringList versioned;
        for (const QString &file : files) {
          const QString absolutePath = absoluteFilePath(wcPath, file);

          NodeStatusBaton node;
          if (!reportError(SvnError::StatusFailed,
                           readNodeStatus(&node, absolutePath, ctx, pool))) {
            return false;
          }

          if (node.status == svn_wc_status_unversioned ||
              node.status == svn_wc_status_ignored) {
            if (!removeFromDisk(absolutePath)) {
              setLastError(SvnError::RevertFailed, -1,
                           QStringLiteral("Failed to remove '%1'.").arg(file));
              return false;
            }
          } else if (node.status != svn_wc_status_none &&
                     node.status != svn_wc_status_normal) {
            versioned.append(absolutePath);
          }
        }

        if (versioned.isEmpty()) {
          return true;
        }

        // Added files are removed from disk, like a discarded new file in Git.
        const PathArray array = makePathArray(pool, versioned);
        return reportError(SvnError::RevertFailed,
                           svn_client_revert4(array.array, svn_depth_infinity,
                                              nullptr, FALSE, FALSE, FALSE, ctx,
                                              pool));
      },
      wcPath);

  LOG_PERF(0, "Exit");
  return success;
}

bool commitStaged(const QString &path, const QString &summary,
                  const QString &message, const QString &author,
                  const QString &email, bool cleanup, bool signoff,
                  bool signCommit) {
  LOG_PERF(0, "Enter");

  if (signCommit) {
    LOG_WARN("Subversion revisions cannot be signed, committing unsigned.");
  }

  const QString logMessage =
      makeLogMessage(summary, message, cleanup, signoff, author, email);
  if (logMessage.trimmed().isEmpty()) {
    setLastError(SvnError::CommitFailed, -1,
                 QStringLiteral("The commit message is empty."));
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString wcPath = normalizedPath(path);
  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        return reportError(SvnError::CommitFailed,
                           commitWorkingCopy(wcPath, logMessage, ctx, pool));
      },
      wcPath);

  LOG_PERF(0, "Exit");
  return success;
}

bool amend(const QString &path, const QString &summary, const QString &message,
           const QString &author, const QString &email, bool signoff,
           bool signCommit) {
  LOG_PERF(0, "Enter");

  Q_UNUSED(signCommit);

  // Only the log message of the newest revision of the branch can be changed.
  // The server has to allow it with a pre-revprop-change hook.
  const QString wcPath = normalizedPath(path);
  const SvnStatus status = readStatus(wcPath, nullptr);
  if (!status.newStagedFiles.isEmpty() ||
      !status.modifiedStagedFiles.isEmpty() ||
      !status.deletedStagedFiles.isEmpty() ||
      !status.typeChangedStagedFiles.isEmpty() ||
      !status.renamedStagedFiles.isEmpty()) {
    setLastError(SvnError::UnsupportedOperation, -1,
                 QStringLiteral("Local changes cannot be added to an existing "
                                "Subversion revision, only its message can be "
                                "changed. Commit them separately."));
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString logMessage =
      makeLogMessage(summary, message, true, signoff, author, email);

  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        WorkingCopyInfo wcInfo;
        if (!reportError(
                SvnError::InfoFailed,
                readInfo(&wcInfo, wcPath, revisionUnspecified(), ctx, pool))) {
          return false;
        }

        WorkingCopyInfo branchInfo;
        if (!reportError(
                SvnError::InfoFailed,
                readInfo(&branchInfo, wcInfo.url, revisionHead(), ctx, pool))) {
          return false;
        }

        const QByteArray urlBytes = wcInfo.url.toUtf8();
        const QByteArray messageBytes = logMessage.toUtf8();
        const svn_string_t *value = svn_string_ncreate(
            messageBytes.constData(),
            static_cast<apr_size_t>(messageBytes.size()), pool);
        const svn_opt_revision_t revision =
            revisionNumber(branchInfo.lastChangedRevision);
        svn_revnum_t setRevision = SVN_INVALID_REVNUM;
        return reportError(
            SvnError::PropertyFailed,
            svn_client_revprop_set2(SVN_PROP_REVISION_LOG, value, nullptr,
                                    urlBytes.constData(), &revision,
                                    &setRevision, FALSE, ctx, pool));
      },
      wcPath);

  LOG_PERF(0, "Exit");
  return success;
}

bool applyPatch(const QString &path, const QString &patchContent) {
  LOG_PERF(0, "Enter");

  QTemporaryFile patchFile;
  if (!patchFile.open() || patchFile.write(patchContent.toUtf8()) < 0) {
    setLastError(SvnError::PatchFailed, -1,
                 QStringLiteral("Failed to write the patch file."));
    LOG_PERF(0, "Exit");
    return false;
  }
  patchFile.close();

  // Unified diffs made by Git prefix the paths with a/ and b/. Subversion
  // removes them itself from Git style diffs ("diff --git a/... b/...").
  const bool gitHeaders = patchContent.contains(QStringLiteral("diff --git "));
  const int stripCount =
      !gitHeaders && (patchContent.contains(QStringLiteral("\n--- a/")) ||
                      patchContent.startsWith(QStringLiteral("--- a/")))
          ? 1
          : 0;

  const QString wcPath = normalizedPath(path);
  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        const QByteArray patchPath =
            normalizedPath(patchFile.fileName()).toUtf8();
        const QByteArray wcBytes = wcPath.toUtf8();
        PatchBaton baton;
        if (!reportError(SvnError::PatchFailed,
                         svn_client_patch(patchPath.constData(),
                                          wcBytes.constData(), FALSE,
                                          stripCount, FALSE, FALSE, TRUE,
                                          patchCallback, &baton, ctx, pool))) {
          return false;
        }

        if (!baton.rejectedFiles.isEmpty()) {
          setLastError(SvnError::PatchFailed, -1,
                       QStringLiteral("The patch could not be applied to: %1")
                           .arg(baton.rejectedFiles.join(", ")));
          return false;
        }

        return true;
      },
      wcPath);

  LOG_PERF(0, "Exit");
  return success;
}

bool checkoutCommit(const QString &path, const QString &commit) {
  LOG_PERF(0, "Enter");

  svn_revnum_t revisionNumberValue = SVN_INVALID_REVNUM;
  if (!parseRevision(commit, &revisionNumberValue)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString wcPath = normalizedPath(path);
  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        const PathArray paths = makePathArray(pool, {wcPath});
        const svn_opt_revision_t revision = revisionNumber(revisionNumberValue);
        apr_array_header_t *resultRevs = nullptr;
        return reportError(SvnError::UpdateFailed,
                           svn_client_update4(&resultRevs, paths.array,
                                              &revision, svn_depth_infinity,
                                              FALSE, FALSE, TRUE, TRUE, FALSE,
                                              ctx, pool));
      },
      wcPath);

  LOG_PERF(0, "Exit");
  return success;
}

bool resetToCommit(const QString &path, const QString &commit, ResetType type) {
  LOG_PERF(0, "Enter");

  // Revisions cannot be removed from the repository. A reset updates the
  // working copy to the revision; a hard reset drops local changes first.
  if (type == ResetType::Hard) {
    const QString wcPath = normalizedPath(path);
    const bool reverted = runWithClientContext(
        [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
          const PathArray paths = makePathArray(pool, {wcPath});
          return reportError(SvnError::RevertFailed,
                             svn_client_revert4(paths.array, svn_depth_infinity,
                                                nullptr, FALSE, FALSE, FALSE,
                                                ctx, pool));
        },
        wcPath);
    if (!reverted) {
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  const bool success = checkoutCommit(path, commit);

  LOG_PERF(0, "Exit");
  return success;
}

QString listConflictsForAFile(const QString &path, const QString &filePath) {
  // Subversion writes the conflict markers into the working file.
  return readTextFile(absoluteFilePath(normalizedPath(path), filePath));
}

bool getIsMergeInProgress(const QString &path) {
  return hasConflicts(path);
}

bool abortMerge(const QString &path) {
  LOG_PERF(0, "Enter");

  const QString wcPath = normalizedPath(path);
  if (!hasConflicts(wcPath)) {
    setLastError(SvnError::InvalidArgument, -1,
                 QStringLiteral("There is no conflicted merge to abort."));
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        const PathArray paths = makePathArray(pool, {wcPath});
        return reportError(SvnError::RevertFailed,
                           svn_client_revert4(paths.array, svn_depth_infinity,
                                              nullptr, FALSE, FALSE, FALSE, ctx,
                                              pool));
      },
      wcPath);

  LOG_PERF(0, "Exit");
  return success;
}

bool pull(const QString &path, const QString &activeBranchName, bool rebase) {
  LOG_PERF(0, "Enter");

  // An update merges the repository changes into the local changes.
  Q_UNUSED(activeBranchName);
  Q_UNUSED(rebase);

  const QString wcPath = normalizedPath(path);
  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        const PathArray paths = makePathArray(pool, {wcPath});
        const svn_opt_revision_t revision = revisionHead();
        apr_array_header_t *resultRevs = nullptr;
        return reportError(SvnError::UpdateFailed,
                           svn_client_update4(&resultRevs, paths.array,
                                              &revision, svn_depth_infinity,
                                              FALSE, FALSE, TRUE, TRUE, FALSE,
                                              ctx, pool));
      },
      wcPath);

  LOG_PERF(0, "Exit");
  return success;
}

bool push(const QString &, const QString &, bool) {
  // Commits are made in the repository, there is nothing to push.
  return true;
}

bool fetchAll(const QString &) {
  // Branches and tags are always read from the repository.
  return true;
}

bool fetchRemote(const QString &, const QString &) {
  return true;
}

} // namespace SvnRepo
