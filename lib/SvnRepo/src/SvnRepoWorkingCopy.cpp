#include "SvnRepo.h"
#include "SvnRepoPrivate.h"

#include <svn_props.h>
#include <svn_string.h>
#include <svn_wc.h>

#include <Log.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryFile>

#include <algorithm>

#define THREAD_ID "SvnRepo"

namespace SvnRepo {

using namespace Private;

namespace {

struct PatchBaton {
  QStringList rejectedFiles;
};

// Subversion calls the patch callback for every file in the patch, whether or
// not it has rejected hunks, so it can't tell rejections apart. A hunk
// notification is sent only after the file's rejects are known.
static void patchNotifyCallback(void *baton, const svn_wc_notify_t *notify,
                                apr_pool_t *) {
  auto *payload = static_cast<PatchBaton *>(baton);
  if (!payload || !notify || !notify->path ||
      notify->action != svn_wc_notify_patch_rejected_hunk) {
    return;
  }

  const QString path = QString::fromUtf8(notify->path);
  if (!payload->rejectedFiles.contains(path)) {
    payload->rejectedFiles.append(path);
  }
}

static bool usesCrLf(const QString &absolutePath) {
  QFile file(absolutePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }
  return file.read(65536).contains("\r\n");
}

static QString stripPathComponents(QString path, const int count) {
  for (int i = 0; i < count; ++i) {
    const int slash = path.indexOf(QLatin1Char('/'));
    if (slash < 0) {
      break;
    }
    path = path.mid(slash + 1);
  }
  return path;
}

// Subversion rejects Git style headers that end with "(revision N)", so the
// suffix is removed from the file headers. Hunk lines get a CR when the
// target uses CRLF, because Subversion matches hunk lines byte for byte.
static QString normalizePatch(const QString &patch, const QString &wcPath,
                              const int stripCount) {
  static const QRegularExpression hunkHeader(
      QStringLiteral("^@@ -\\d+(?:,(\\d+))? \\+\\d+(?:,(\\d+))? @@"));
  static const QRegularExpression revisionSuffix(
      QStringLiteral("\\s+\\(revision\\s.*$"));

  const QStringList lines = patch.split(QLatin1Char('\n'));
  QStringList result;
  result.reserve(lines.size());

  const QDir wcDir(wcPath);
  QString target;
  bool gitStyle = false;
  bool crlf = false;
  int oldLeft = 0;
  int newLeft = 0;

  for (const QString &line : lines) {
    if (oldLeft > 0 || newLeft > 0) {
      const QChar first = line.isEmpty() ? QLatin1Char(' ') : line.at(0);
      if (first == QLatin1Char(' ')) {
        --oldLeft;
        --newLeft;
      } else if (first == QLatin1Char('-')) {
        --oldLeft;
      } else if (first == QLatin1Char('+')) {
        --newLeft;
      }

      const bool needsCr = crlf && !line.endsWith(QLatin1Char('\r')) &&
                           first != QLatin1Char('\\');
      result.append(needsCr ? line + QLatin1Char('\r') : line);
      continue;
    }

    QString text = line;
    if (text.startsWith(QStringLiteral("--- ")) ||
        text.startsWith(QStringLiteral("+++ "))) {
      text.remove(revisionSuffix);
    }

    if (text.startsWith(QStringLiteral("diff --git "))) {
      const QString rest = text.mid(11);
      const int split = rest.indexOf(QStringLiteral(" b/"));
      QString first = split >= 0 ? rest.left(split) : rest;
      target = first.startsWith(QStringLiteral("a/")) ? first.mid(2) : first;
      gitStyle = true;
      crlf = !target.isEmpty() && usesCrLf(wcDir.filePath(target));
    } else if (text.startsWith(QStringLiteral("Index: "))) {
      gitStyle = false;
    } else if (!gitStyle && text.startsWith(QStringLiteral("+++ "))) {
      QString header = text.mid(4);
      const int tab = header.indexOf(QLatin1Char('\t'));
      if (tab >= 0) {
        header = header.left(tab);
      }
      target = stripPathComponents(header.trimmed(), stripCount);
      crlf = !target.isEmpty() && usesCrLf(wcDir.filePath(target));
    } else {
      const QRegularExpressionMatch match = hunkHeader.match(line);
      if (match.hasMatch()) {
        oldLeft = match.captured(1).isEmpty() ? 1 : match.captured(1).toInt();
        newLeft = match.captured(2).isEmpty() ? 1 : match.captured(2).toInt();
      }
    }

    result.append(text);
  }

  return result.join(QLatin1Char('\n'));
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
  svn_error_t *error = svn_client_status6(
      &youngest, ctx, pathBytes.constData(), &revision, svn_depth_empty, TRUE,
      FALSE, TRUE, TRUE, TRUE, FALSE, nullptr, nodeStatusCallback, result, pool);

  // Subversion does not know the paths below an unversioned directory.
  if (error && error->apr_err == SVN_ERR_WC_PATH_NOT_FOUND) {
    svn_error_clear(error);
    const QFileInfo info(absolutePath);
    if (info.exists() || info.isSymLink()) {
      result->status = svn_wc_status_unversioned;
      result->kind = info.isDir() ? svn_node_dir : svn_node_file;
    }
    return SVN_NO_ERROR;
  }
  return error;
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

  // The staging area is a changelist. New files are scheduled for addition
  // and missing files for deletion, then every selected file is added to the
  // changelist, so commitStaged commits only these files.
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

          const QByteArray pathBytes = absolutePath.toUtf8();
          if (node.status == svn_wc_status_unversioned) {
            if (!reportError(SvnError::AddFailed,
                             svn_client_add5(pathBytes.constData(),
                                             svn_depth_infinity, FALSE, FALSE,
                                             FALSE, TRUE, ctx, pool))) {
              return false;
            }
          } else if (node.status == svn_wc_status_missing) {
            const PathArray deletion = makePathArray(pool, {absolutePath});
            if (!reportError(SvnError::DeleteFailed,
                             svn_client_delete4(deletion.array, TRUE, FALSE,
                                                nullptr, nullptr, nullptr, ctx,
                                                pool))) {
              return false;
            }
          } else if (node.status == svn_wc_status_ignored) {
            continue;
          } else if (node.status == svn_wc_status_none &&
                     !QFileInfo(absolutePath).exists()) {
            // Missing on disk and unknown to Subversion, e.g. below a deleted
            // folder. Nothing can be scheduled or added to a changelist.
            continue;
          }
          targets.append(absolutePath);
        }

        if (targets.isEmpty()) {
          return true;
        }

        const PathArray array = makePathArray(pool, targets);
        return reportError(
            SvnError::AddFailed,
            svn_client_add_to_changelist(array.array, kStagingChangelist,
                                         svn_depth_infinity, nullptr, ctx,
                                         pool));
      },
      wcPath);

  LOG_PERF(0, "Exit");
  return success;
}

bool unstageFiles(const QString &path, const QStringList &files) {
  LOG_PERF(0, "Enter");

  // Only removes the selection. The changes stay in the working copy and are
  // reported as unstaged.
  const QString wcPath = normalizedPath(path);
  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        QStringList targets;
        for (const QString &file : files) {
          targets.append(absoluteFilePath(wcPath, file));
        }
        if (targets.isEmpty()) {
          return true;
        }

        const PathArray array = makePathArray(pool, targets);
        apr_array_header_t *changelists =
            apr_array_make(pool, 1, sizeof(const char *));
        APR_ARRAY_PUSH(changelists, const char *) = kStagingChangelist;
        return reportError(
            SvnError::RevertFailed,
            svn_client_remove_from_changelists(array.array, svn_depth_infinity,
                                               changelists, ctx, pool));
      },
      wcPath);

  LOG_PERF(0, "Exit");
  return success;
}

// Appends lines to a multi-line ignore property of a directory, keeping the
// lines already there. Nothing is written when every line is present.
static svn_error_t *addIgnorePropertyLines(const QString &wcPath,
                                           const QString &directory,
                                           const char *propertyName,
                                           const QStringList &lines,
                                           svn_client_ctx_t *ctx,
                                           apr_pool_t *pool) {
  const QString directoryPath = absoluteFilePath(wcPath, directory);
  const QByteArray targetBytes = directoryPath.toUtf8();
  const svn_opt_revision_t peg = revisionUnspecified();
  const svn_opt_revision_t revision = revisionWorking();
  apr_hash_t *properties = nullptr;
  SVN_ERR(svn_client_propget5(&properties, nullptr, propertyName,
                              targetBytes.constData(), &peg, &revision, nullptr,
                              svn_depth_empty, nullptr, ctx, pool, pool));

  QStringList current;
  for (apr_hash_index_t *index = apr_hash_first(pool, properties); index;
       index = apr_hash_next(index)) {
    const void *key = nullptr;
    void *value = nullptr;
    apr_hash_this(index, &key, nullptr, &value);
    const auto *propertyValue = static_cast<const svn_string_t *>(value);
    current = QString::fromUtf8(propertyValue->data,
                                static_cast<int>(propertyValue->len))
                  .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
  }

  QStringList updated = current;
  for (const QString &line : lines) {
    if (!updated.contains(line)) {
      updated.append(line);
    }
  }

  if (updated.size() == current.size()) {
    return SVN_NO_ERROR;
  }

  const QByteArray value = updated.join(QLatin1Char('\n')).toUtf8();
  const svn_string_t *propertyValue = svn_string_ncreate(
      value.constData(), static_cast<apr_size_t>(value.size()), pool);
  const PathArray targets = makePathArray(pool, {directoryPath});
  return svn_client_propset_local(propertyName, propertyValue, targets.array,
                                  svn_depth_empty, FALSE, nullptr, ctx, pool);
}

// Subversion cannot set properties on unversioned paths.
static bool isVersionedPath(const QString &wcPath, const QString &path,
                            svn_client_ctx_t *ctx, apr_pool_t *pool) {
  WorkingCopyInfo info;
  svn_error_t *error = readInfo(&info, absoluteFilePath(wcPath, path),
                                revisionUnspecified(), ctx, pool);
  if (error) {
    svn_error_clear(error);
    return false;
  }
  return true;
}

bool ignoreFiles(const QString &path, const QStringList &files, bool recursive) {
  LOG_PERF(0, "Enter");

  const QString wcPath = normalizedPath(path);
  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        QMap<QString, QStringList> names;
        QSet<QString> recursiveDirectories;
        QHash<QString, bool> versionedCache;
        auto isVersionedCached = [&](const QString &directory) {
          if (!versionedCache.contains(directory)) {
            versionedCache.insert(
                directory, isVersionedPath(wcPath, directory, ctx, pool));
          }
          return versionedCache.value(directory);
        };

        for (const QString &file : files) {
          const QFileInfo info(file);
          QString directory = info.path();
          if (directory == QLatin1String(".")) {
            directory.clear();
          }

          const bool isDirectory =
              QFileInfo(absoluteFilePath(wcPath, file)).isDir();
          if (recursive) {
            if (isDirectory) {
              recursiveDirectories.insert(QDir::cleanPath(file));
            } else if (!directory.isEmpty()) {
              recursiveDirectories.insert(directory);
            }
          } else if (directory.isEmpty() || isVersionedCached(directory)) {
            names[directory].append(info.fileName());
          } else {
            // The parent is untracked, so its topmost untracked folder is
            // ignored instead of the file.
            recursiveDirectories.insert(directory);
          }
        }

        for (auto it = names.cbegin(); it != names.cend(); ++it) {
          if (!reportError(SvnError::PropertyFailed,
                           addIgnorePropertyLines(wcPath, it.key(),
                                                  SVN_PROP_IGNORE, it.value(),
                                                  ctx, pool))) {
            return false;
          }
        }

        for (const QString &directory : recursiveDirectories) {
          // Untracked folders cannot hold properties, so the topmost untracked
          // folder is ignored by name in its nearest versioned parent.
          QString versionedDirectory = directory;
          QString untracked;
          while (!versionedDirectory.isEmpty() &&
                 !isVersionedPath(wcPath, versionedDirectory, ctx, pool)) {
            untracked = QFileInfo(versionedDirectory).fileName();
            versionedDirectory = QFileInfo(versionedDirectory).path();
            if (versionedDirectory == QLatin1String(".")) {
              versionedDirectory.clear();
            }
          }

          svn_error_t *error = nullptr;
          if (untracked.isEmpty()) {
            error = addIgnorePropertyLines(wcPath, versionedDirectory,
                                           SVN_PROP_INHERITABLE_IGNORES,
                                           {QStringLiteral("*")}, ctx, pool);
          } else {
            error = addIgnorePropertyLines(wcPath, versionedDirectory,
                                           SVN_PROP_IGNORE, {untracked}, ctx,
                                           pool);
          }
          if (!reportError(SvnError::PropertyFailed, error)) {
            return false;
          }
        }

        return true;
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
        // Discarded files also leave the staging area.
        const PathArray array = makePathArray(pool, versioned);
        return reportError(SvnError::RevertFailed,
                           svn_client_revert4(array.array, svn_depth_infinity,
                                              nullptr, TRUE, FALSE, FALSE, ctx,
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
  const SvnStatus current = readStatus(wcPath, nullptr);
  const bool nothingStaged =
      current.newStagedFiles.isEmpty() &&
      current.modifiedStagedFiles.isEmpty() &&
      current.deletedStagedFiles.isEmpty() &&
      current.typeChangedStagedFiles.isEmpty() &&
      current.renamedStagedFiles.isEmpty();
  if (nothingStaged) {
    setLastError(SvnError::CommitFailed, -1,
                 QStringLiteral("Nothing is staged to commit."));
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        // Changelists only hold files, so the added or deleted directories
        // above the staged files are committed with them, as Subversion
        // requires the parents of an added file to be part of the commit.
        QSet<QString> targets;
        QStringList deletedDirectories;
        for (const QStringList *list :
             {&current.newStagedFiles, &current.modifiedStagedFiles,
              &current.deletedStagedFiles, &current.typeChangedStagedFiles,
              &current.renamedStagedFiles}) {
          for (const QString &file : *list) {
            targets.insert(absoluteFilePath(wcPath, file));

            for (QString parent = QFileInfo(file).path();
                 parent != QLatin1String("."); parent = QFileInfo(parent).path()) {
              const QString absoluteParent = absoluteFilePath(wcPath, parent);
              if (targets.contains(absoluteParent)) {
                break;
              }

              NodeStatusBaton node;
              if (!reportError(SvnError::StatusFailed,
                               readNodeStatus(&node, absoluteParent, ctx,
                                              pool))) {
                return false;
              }
              if (node.status != svn_wc_status_added &&
                  node.status != svn_wc_status_replaced &&
                  node.status != svn_wc_status_deleted) {
                break;
              }

              targets.insert(absoluteParent);
              if (node.status == svn_wc_status_deleted) {
                deletedDirectories.append(absoluteParent + QLatin1Char('/'));
              }
            }
          }
        }

        // A deleted directory is committed with everything below it.
        QStringList paths;
        for (const QString &target : targets) {
          const bool belowDeleted = std::any_of(
              deletedDirectories.cbegin(), deletedDirectories.cend(),
              [&](const QString &directory) {
                return target.startsWith(directory);
              });
          if (!belowDeleted) {
            paths.append(target);
          }
        }

        return reportError(SvnError::CommitFailed,
                           commitPaths(paths, logMessage, ctx, pool));
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

  // Unified diffs made by Git prefix the paths with a/ and b/. Subversion
  // removes them itself from Git style diffs ("diff --git a/... b/...").
  const bool gitHeaders = patchContent.contains(QStringLiteral("diff --git "));
  const int stripCount =
      !gitHeaders && (patchContent.contains(QStringLiteral("\n--- a/")) ||
                      patchContent.startsWith(QStringLiteral("--- a/")))
          ? 1
          : 0;

  const QString wcPath = normalizedPath(path);
  const QString patchText = normalizePatch(patchContent, wcPath, stripCount);

  QTemporaryFile patchFile;
  if (!patchFile.open() || patchFile.write(patchText.toUtf8()) < 0) {
    setLastError(SvnError::PatchFailed, -1,
                 QStringLiteral("Failed to write the patch file."));
    LOG_PERF(0, "Exit");
    return false;
  }
  patchFile.close();

  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        const QByteArray patchPath =
            normalizedPath(patchFile.fileName()).toUtf8();
        const QByteArray wcBytes = wcPath.toUtf8();
        PatchBaton baton;
        ctx->notify_func2 = patchNotifyCallback;
        ctx->notify_baton2 = &baton;
        const auto error = svn_client_patch(
            patchPath.constData(), wcBytes.constData(), FALSE, stripCount,
            FALSE, FALSE, TRUE, nullptr, nullptr, ctx, pool);
        if (!reportError(SvnError::PatchFailed, error)) {
          return false;
        }

        if (!baton.rejectedFiles.isEmpty()) {
          QStringList relativePaths;
          const QDir wcDir(wcPath);
          for (const QString &file : baton.rejectedFiles) {
            relativePaths.append(wcDir.relativeFilePath(file));
          }
          setLastError(SvnError::PatchFailed, -1,
                       QStringLiteral("The patch could not be applied to: %1")
                           .arg(relativePaths.join(", ")));
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

bool isTreeConflict(const QString &path, const QString &filePath) {
  const QString absolutePath = absoluteFilePath(normalizedPath(path), filePath);
  bool treeConflicted = false;

  runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        svn_boolean_t textConflicted = FALSE;
        svn_boolean_t propConflicted = FALSE;
        svn_boolean_t conflicted = FALSE;
        svn_error_t *err = svn_wc_conflicted_p3(
            &textConflicted, &propConflicted, &conflicted, ctx->wc_ctx,
            absolutePath.toUtf8().constData(), pool);
        if (err) {
          svn_error_clear(err);
          return true;
        }

        treeConflicted = conflicted;
        return true;
      },
      normalizedPath(path));

  return treeConflicted;
}

bool resolveTreeConflict(const QString &path, const QString &filePath,
                         bool keepLocal) {
  LOG_PERF(0, "Enter");

  const QString wcPath = normalizedPath(path);
  const QString absolutePath = absoluteFilePath(wcPath, filePath);

  // Take incoming: the local file blocks the incoming add, so it is moved
  // aside, the conflict is cleared and the file is restored from the
  // repository. The local copy is kept next to it as "<name>.local".
  QString backupPath;
  if (!keepLocal && QFileInfo::exists(absolutePath)) {
    backupPath = absolutePath + QStringLiteral(".local");
    for (int index = 1; QFileInfo::exists(backupPath); ++index) {
      backupPath =
          absolutePath + QStringLiteral(".local") + QString::number(index);
    }

    if (!QFile::rename(absolutePath, backupPath)) {
      setLastError(SvnError::ResolveFailed, -1,
                   QStringLiteral("Unable to move the local file aside."));
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  const auto restoreBackup = [&]() {
    if (!backupPath.isEmpty() && QFileInfo::exists(backupPath)) {
      QFile::remove(absolutePath);
      QFile::rename(backupPath, absolutePath);
    }
  };

  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        const PathArray paths = makePathArray(pool, {absolutePath});
        // svn_wc_conflict_choose_merged keeps the working file as it is and
        // only clears the conflict, which is what the CLI "working" option does.
        if (!reportError(SvnError::ResolveFailed,
                         svn_client_resolve(absolutePath.toUtf8().constData(),
                                            svn_depth_empty,
                                            svn_wc_conflict_choose_merged, ctx,
                                            pool))) {
          return false;
        }

        if (keepLocal) {
          return true;
        }

        const svn_opt_revision_t revision = revisionHead();
        apr_array_header_t *resultRevs = nullptr;
        return reportError(SvnError::UpdateFailed,
                           svn_client_update4(&resultRevs, paths.array,
                                              &revision, svn_depth_empty, FALSE,
                                              FALSE, TRUE, TRUE, FALSE, ctx,
                                              pool));
      },
      wcPath);

  if (!success) {
    restoreBackup();
  }

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
