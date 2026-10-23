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
#include <QUrl>

#include <algorithm>
#include <functional>

#define THREAD_ID "SvnRepo"

namespace SvnRepo {

using namespace Private;

namespace {

struct PatchBaton {
  QStringList rejectedFiles;
  // Hunks applied, files added or deleted, properties changed.
  int changes = 0;
};

// Subversion calls the patch callback for every file in the patch, whether or
// not it has rejected hunks, so it can't tell rejections apart. A hunk
// notification is sent only after the file's rejects are known.
static void patchNotifyCallback(void *baton, const svn_wc_notify_t *notify,
                                apr_pool_t *) {
  auto *payload = static_cast<PatchBaton *>(baton);
  if (!payload || !notify || !notify->path) {
    return;
  }

  switch (notify->action) {
  case svn_wc_notify_patch_rejected_hunk: {
    const QString path = QString::fromUtf8(notify->path);
    if (!payload->rejectedFiles.contains(path)) {
      payload->rejectedFiles.append(path);
    }
    break;
  }
  case svn_wc_notify_patch_applied_hunk:
  case svn_wc_notify_add:
  case svn_wc_notify_delete:
    ++payload->changes;
    break;
  case svn_wc_notify_patch:
    // Text changes come as applied hunks; an already applied hunk changes
    // nothing.
    if (notify->prop_state == svn_wc_notify_state_changed ||
        notify->prop_state == svn_wc_notify_state_merged) {
      ++payload->changes;
    }
    break;
  default:
    break;
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
// target uses CRLF, because Subversion matches hunk lines byte for byte, and
// keep Latin-1 bytes when the target isn't UTF-8 (B25). Paths of
// "svn diff --git", which start at the repository root, are made relative to
// the working copy (wcRepoPath, e.g. "trunk").
static QByteArray normalizePatch(const QString &patch, const QString &wcPath,
                                 const QString &wcRepoPath,
                                 const int stripCount) {
  static const QRegularExpression hunkHeader(
      QStringLiteral("^@@ -\\d+(?:,(\\d+))? \\+\\d+(?:,(\\d+))? @@"));
  static const QRegularExpression revisionSuffix(
      QStringLiteral("\\s+\\(revision\\s.*$"));

  const QStringList lines = patch.split(QLatin1Char('\n'));
  QByteArray result;

  const QDir wcDir(wcPath);
  const QString repoPrefix =
      wcRepoPath.isEmpty() ? QString() : wcRepoPath + QLatin1Char('/');
  QString target;
  bool gitStyle = false;
  bool crlf = false;
  bool latin1 = false;
  int oldLeft = 0;
  int newLeft = 0;

  const auto readTarget = [&](const QString &path) {
    target = path;
    const QString absolutePath = wcDir.filePath(target);
    crlf = !target.isEmpty() && usesCrLf(absolutePath);
    QFile file(absolutePath);
    const QByteArray content =
        file.open(QIODevice::ReadOnly) ? file.read(65536) : QByteArray();
    latin1 = QString::fromUtf8(content).toUtf8() != content;
  };
  const auto append = [&](const QString &line) {
    const bool asLatin1 =
        latin1 && std::all_of(line.cbegin(), line.cend(),
                              [](QChar c) { return c.unicode() < 256; });
    result += asLatin1 ? line.toLatin1() : line.toUtf8();
    result += '\n';
  };
  const auto withoutRepoPrefix = [&](QString path) {
    if (!repoPrefix.isEmpty() && path.startsWith(repoPrefix) &&
        !QFileInfo::exists(wcDir.filePath(path))) {
      path = path.mid(repoPrefix.size());
    }
    return path;
  };

  for (qsizetype i = 0; i < lines.size(); ++i) {
    const QString &line = lines[i];
    if (i == lines.size() - 1 && line.isEmpty()) {
      break;
    }
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

      // A hunk line keeps a CR it has; otherwise one is added for CRLF
      // targets.
      const bool needsCr = crlf && !line.endsWith(QLatin1Char('\r')) &&
                           first != QLatin1Char('\\');
      append(needsCr ? line + QLatin1Char('\r') : line);
      continue;
    }

    QString text = line;
    if (text.endsWith(QLatin1Char('\r'))) {
      text.chop(1);
    }
    if (text.startsWith(QStringLiteral("--- ")) ||
        text.startsWith(QStringLiteral("+++ "))) {
      text.remove(revisionSuffix);
    }

    if (text.startsWith(QStringLiteral("diff --git "))) {
      // "a/<path> b/<path>": the path may contain " b/" itself, so the
      // halves are told apart by their length.
      const QString rest = text.mid(11);
      QString path;
      const qsizetype length = (rest.size() - 5) / 2;
      if (rest.startsWith(QStringLiteral("a/")) && length > 0 &&
          rest.mid(2 + length, 3) == QStringLiteral(" b/") &&
          rest.mid(2, length) == rest.mid(5 + length)) {
        path = rest.mid(2, length);
      } else {
        const qsizetype split = rest.lastIndexOf(QStringLiteral(" b/"));
        path = split >= 0 ? rest.left(split) : rest;
        if (path.startsWith(QStringLiteral("a/"))) {
          path = path.mid(2);
        }
      }
      const QString relative = withoutRepoPrefix(path);
      text = QStringLiteral("diff --git a/%1 b/%1").arg(relative);
      gitStyle = true;
      readTarget(relative);
    } else if (gitStyle && (text.startsWith(QStringLiteral("--- a/")) ||
                            text.startsWith(QStringLiteral("+++ b/")))) {
      text = text.left(6) + withoutRepoPrefix(text.mid(6));
    } else if (text.startsWith(QStringLiteral("Index: "))) {
      gitStyle = false;
    } else if (!gitStyle && text.startsWith(QStringLiteral("+++ "))) {
      QString header = text.mid(4);
      const int tab = header.indexOf(QLatin1Char('\t'));
      if (tab >= 0) {
        header = header.left(tab);
      }
      readTarget(stripPathComponents(header.trimmed(), stripCount));
    } else {
      const QRegularExpressionMatch match = hunkHeader.match(text);
      if (match.hasMatch()) {
        oldLeft = match.captured(1).isEmpty() ? 1 : match.captured(1).toInt();
        newLeft = match.captured(2).isEmpty() ? 1 : match.captured(2).toInt();
      }
    }

    append(text);
  }

  return result;
}

struct NodeStatusBaton {
  svn_wc_status_kind status = svn_wc_status_none;
  svn_wc_status_kind propStatus = svn_wc_status_none;
  svn_node_kind_t kind = svn_node_unknown;
  bool copied = false;
};

static svn_error_t *nodeStatusCallback(void *baton, const char *,
                                       const svn_client_status_t *status,
                                       apr_pool_t *) {
  auto *payload = static_cast<NodeStatusBaton *>(baton);
  if (payload && status) {
    payload->status = status->node_status;
    payload->propStatus = status->prop_status;
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

// A folder whose only change is its own properties.
static bool hasOnlyPropertyChanges(const NodeStatusBaton &node) {
  return node.kind == svn_node_dir && node.status == svn_wc_status_modified;
}

// Whether file is inside the working copy at wcPath. Records InvalidArgument
// when it is not.
static bool checkInsideWorkingCopy(const QString &wcPath,
                                   const QStringList &files) {
  for (const QString &file : files) {
    const QString relativePath = QDir::cleanPath(QDir::fromNativeSeparators(
        QFileInfo(file).isAbsolute() ? relativeFilePath(wcPath, file) : file));
    if (relativePath == QLatin1String("..") ||
        relativePath.startsWith(QLatin1String("../")) ||
        QFileInfo(relativePath).isAbsolute()) {
      setLastError(SvnError::InvalidArgument, -1,
                   QStringLiteral("%1 is outside the working copy.").arg(file));
      return false;
    }
  }
  return true;
}

// Whether a file still holds the conflict markers Subversion wrote into it.
static bool hasConflictMarkers(const QString &absolutePath) {
  QFile file(absolutePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }
  bool start = false;
  bool end = false;
  while (!file.atEnd()) {
    const QByteArray line = file.readLine();
    start = start || line.startsWith("<<<<<<< ");
    end = end || line.startsWith(">>>>>>> ");
  }
  return start && end;
}

struct PathsBaton {
  QStringList paths;
  std::function<bool(const svn_client_status_t *)> accept;
};

static svn_error_t *pathsCallback(void *baton, const char *,
                                  const svn_client_status_t *status,
                                  apr_pool_t *) {
  auto *payload = static_cast<PathsBaton *>(baton);
  if (payload && status && payload->accept(status)) {
    payload->paths.append(QString::fromUtf8(status->local_abspath));
  }
  return SVN_NO_ERROR;
}

// The paths at or below absolutePath, down to depth, whose status accept
// takes. Unchanged nodes are only looked at with all.
static svn_error_t *
readPaths(QStringList *paths, const QString &absolutePath, svn_depth_t depth,
          const std::function<bool(const svn_client_status_t *)> &accept,
          svn_client_ctx_t *ctx, apr_pool_t *pool, bool all = false) {
  PathsBaton baton;
  baton.accept = accept;
  const QByteArray pathBytes = absolutePath.toUtf8();
  const svn_opt_revision_t revision = revisionWorking();
  svn_revnum_t youngest = SVN_INVALID_REVNUM;
  SVN_ERR(svn_client_status6(&youngest, ctx, pathBytes.constData(), &revision,
                             depth, all, FALSE, TRUE, FALSE, TRUE, FALSE,
                             nullptr, pathsCallback, &baton, pool));
  *paths = baton.paths;
  return SVN_NO_ERROR;
}

// Folders with a change of their own that a commit must name: added (also
// when empty), copied or replaced folders, and property changes.
static bool isChangedDirectory(const svn_client_status_t *status) {
  return status->kind == svn_node_dir &&
         (status->node_status == svn_wc_status_added ||
          status->node_status == svn_wc_status_replaced || status->copied ||
          status->prop_status == svn_wc_status_modified);
}

static void removeAtOrBelow(QSet<QString> *paths, const QString &path) {
  const QString prefix = path + QLatin1Char('/');
  for (auto it = paths->begin(); it != paths->end();) {
    if (*it == path || it->startsWith(prefix)) {
      it = paths->erase(it);
    } else {
      ++it;
    }
  }
}

static QString parentPath(const QString &absolutePath) {
  return QFileInfo(absolutePath).path();
}

// Escapes the characters Subversion's ignore patterns treat as wildcards.
static QString escapedIgnorePattern(const QString &name) {
  QString pattern;
  for (const QChar c : name) {
    if (c == QLatin1Char('[') || c == QLatin1Char(']') ||
        c == QLatin1Char('*') || c == QLatin1Char('?') ||
        c == QLatin1Char('\\')) {
      pattern += QLatin1Char('\\');
    }
    pattern += c;
  }
  return pattern;
}

} // namespace

namespace Private {

bool activeOperation(const QString &wcPath, OperationState *state) {
  OperationState current;
  if (!readOperationState(wcPath, &current)) {
    return false;
  }

  // Stale when none of its paths has a local change or a conflict any more.
  bool stale = true;
  runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        for (const QString &path : current.paths) {
          NodeStatusBaton node;
          svn_error_t *error = readNodeStatus(&node, path, ctx, pool);
          if (error) {
            svn_error_clear(error);
            stale = false;
            return true;
          }
          // The root stands for the operation's svn:mergeinfo.
          const bool isRoot = path == normalizedPath(wcPath);
          const bool changed =
              isRoot ? node.propStatus == svn_wc_status_modified ||
                           node.status == svn_wc_status_conflicted
                     : node.status != svn_wc_status_normal &&
                        node.status != svn_wc_status_none &&
                        node.status != svn_wc_status_unversioned &&
                        node.status != svn_wc_status_ignored;
          if (changed) {
            stale = false;
            return true;
          }
        }
        return true;
      },
      wcPath);
  if (stale) {
    removeOperationState(wcPath);
    return false;
  }
  if (state) {
    *state = current;
  }
  return true;
}

bool refuseWhileOperationInProgress(const QString &wcPath) {
  OperationState state;
  if (!activeOperation(wcPath, &state)) {
    return false;
  }
  setLastError(SvnError::OperationInProgress, -1,
               QStringLiteral("A %1 is in progress. Commit it or abort it "
                              "first.")
                   .arg(state.kind));
  return true;
}

} // namespace Private

SvnStatus getStatus(const QString &path, const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  if (quitFlag) {
    setLastError(SvnError::Cancelled, SVN_ERR_CANCELLED,
                 QStringLiteral("The status was cancelled."));
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
  // changelist, so commitStaged commits only these files. A folder stages
  // the changes below it; a folder with a change of its own (properties, an
  // empty, added or copied folder) is recorded as a staged folder.
  const QString wcPath = normalizedPath(path);
  if (!checkInsideWorkingCopy(wcPath, files)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        QSet<QString> stagedDirectories = readStagedDirectories(wcPath);
        const QSet<QString> stagedBefore = stagedDirectories;
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
          } else if (node.status == svn_wc_status_conflicted) {
            // Staging a file whose markers the user removed marks its text
            // conflict resolved. Markers that are still there, and property
            // conflicts, must be resolved first. Tree conflicts are resolved
            // separately, so they are left untouched here.
            svn_boolean_t textConflicted = FALSE;
            svn_boolean_t propConflicted = FALSE;
            svn_boolean_t conflicted = FALSE;
            if (!reportError(SvnError::StatusFailed,
                             svn_wc_conflicted_p3(
                                 &textConflicted, &propConflicted, &conflicted,
                                 ctx->wc_ctx, pathBytes.constData(), pool))) {
              return false;
            }

            if (propConflicted) {
              setLastError(SvnError::ResolveFailed, -1,
                           QStringLiteral("The property conflict of %1 must "
                                          "be resolved first.")
                               .arg(file));
              return false;
            }
            if (textConflicted && hasConflictMarkers(absolutePath)) {
              setLastError(SvnError::ResolveFailed, -1,
                           QStringLiteral("%1 still has conflict markers.")
                               .arg(file));
              return false;
            }
            if (textConflicted &&
                !reportError(SvnError::ResolveFailed,
                             svn_client_resolve(pathBytes.constData(),
                                                svn_depth_empty,
                                                svn_wc_conflict_choose_merged,
                                                ctx, pool))) {
              return false;
            }
          } else if (node.status == svn_wc_status_ignored) {
            setLastError(SvnError::AddFailed, -1,
                         QStringLiteral("%1 is ignored.").arg(file));
            return false;
          } else if (node.status == svn_wc_status_none &&
                     !QFileInfo::exists(absolutePath)) {
            // Unknown to Subversion and not on disk: only the files below a
            // deleted folder are like that, and they have nothing to stage.
            NodeStatusBaton parent;
            if (!reportError(SvnError::StatusFailed,
                             readNodeStatus(&parent, parentPath(absolutePath),
                                            ctx, pool))) {
              return false;
            }
            if (parent.status == svn_wc_status_deleted ||
                parent.status == svn_wc_status_missing) {
              continue;
            }
            setLastError(SvnError::AddFailed, -1,
                         QStringLiteral("%1 does not exist.").arg(file));
            return false;
          }

          const bool isDirectory =
              QFileInfo(absolutePath).isDir() || node.kind == svn_node_dir;
          if (isDirectory && node.status != svn_wc_status_deleted &&
              (hasOnlyPropertyChanges(node) || node.copied)) {
            // The folder's own change, not the files below it.
            stagedDirectories.insert(absolutePath);
            continue;
          }
          if (isDirectory && node.status != svn_wc_status_deleted &&
              node.status != svn_wc_status_missing) {
            // Everything below the folder: its new files are added first.
            QStringList changedDirectories;
            if (!reportError(SvnError::AddFailed,
                             svn_client_add5(pathBytes.constData(),
                                             svn_depth_infinity, TRUE, FALSE,
                                             FALSE, FALSE, ctx, pool)) ||
                !reportError(SvnError::StatusFailed,
                             readPaths(&changedDirectories, absolutePath,
                                       svn_depth_infinity, isChangedDirectory,
                                       ctx, pool))) {
              return false;
            }
            for (const QString &directory : changedDirectories) {
              stagedDirectories.insert(directory);
            }
          } else if (isDirectory) {
            stagedDirectories.insert(absolutePath);
          }
          targets.append(absolutePath);
        }

        if (stagedDirectories != stagedBefore) {
          writeStagedDirectories(wcPath, stagedDirectories);
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
        QSet<QString> stagedDirectories = readStagedDirectories(wcPath);
        const QSet<QString> stagedBefore = stagedDirectories;
        QStringList targets;
        for (const QString &file : files) {
          const QString absolutePath = absoluteFilePath(wcPath, file);
          removeAtOrBelow(&stagedDirectories, absolutePath);

          // Unversioned paths have nothing staged.
          NodeStatusBaton node;
          if (!reportError(SvnError::StatusFailed,
                           readNodeStatus(&node, absolutePath, ctx, pool))) {
            return false;
          }
          if (node.status != svn_wc_status_unversioned &&
              node.status != svn_wc_status_ignored &&
              node.status != svn_wc_status_none) {
            targets.append(absolutePath);
          }
        }

        if (stagedDirectories != stagedBefore) {
          writeStagedDirectories(wcPath, stagedDirectories);
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
  for (const QString &file : files) {
    const QString relativePath = QDir::cleanPath(QDir::fromNativeSeparators(
        QFileInfo(file).isAbsolute() ? relativeFilePath(wcPath, file) : file));
    if (relativePath == QLatin1String("..") ||
        relativePath.startsWith(QLatin1String("../")) ||
        QFileInfo(relativePath).isAbsolute()) {
      setLastError(SvnError::InvalidArgument, -1,
                   QStringLiteral("%1 is outside the working copy.").arg(file));
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        QMap<QString, QStringList> names;
        QMap<QString, QStringList> globalNames;
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

          if (recursive) {
            // The name goes into svn:global-ignores of the nearest versioned
            // folder above it, so it is ignored at any depth below, like a
            // .gitignore entry without a slash.
            QString versionedDirectory = directory;
            while (!versionedDirectory.isEmpty() &&
                   !isVersionedCached(versionedDirectory)) {
              const QString parent = QFileInfo(versionedDirectory).path();
              versionedDirectory = parent == QLatin1String(".") ||
                                           parent == versionedDirectory
                                       ? QString()
                                       : parent;
            }
            globalNames[versionedDirectory].append(
                escapedIgnorePattern(QFileInfo(QDir::cleanPath(file)).fileName()));
          } else if (directory.isEmpty() || isVersionedCached(directory)) {
            names[directory].append(escapedIgnorePattern(info.fileName()));
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
        for (auto it = globalNames.cbegin(); it != globalNames.cend(); ++it) {
          if (!reportError(SvnError::PropertyFailed,
                           addIgnorePropertyLines(
                               wcPath, it.key(), SVN_PROP_INHERITABLE_IGNORES,
                               it.value(), ctx, pool))) {
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
            const QString parent = QFileInfo(versionedDirectory).path();
            // A root ("C:/", "/") is its own parent.
            versionedDirectory = parent == QLatin1String(".") ||
                                         parent == versionedDirectory
                                     ? QString()
                                     : parent;
          }

          if (!untracked.isEmpty() &&
              !reportError(SvnError::PropertyFailed,
                           addIgnorePropertyLines(
                               wcPath, versionedDirectory, SVN_PROP_IGNORE,
                               {escapedIgnorePattern(untracked)}, ctx,
                               pool))) {
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

  // Like the Git backend: versioned files go back to BASE; added files are
  // un-added and stay on disk, unversioned files are left alone. The whole
  // working copy is never one file to discard.
  const QString wcPath = normalizedPath(path);
  for (const QString &file : files) {
    if (absoluteFilePath(wcPath, file) == wcPath) {
      setLastError(SvnError::InvalidArgument, -1,
                   QStringLiteral("The whole working copy can't be discarded "
                                  "as one file."));
      LOG_PERF(0, "Exit");
      return false;
    }
  }
  if (!checkInsideWorkingCopy(wcPath, files)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        const auto revert = [&](const QStringList &paths, svn_depth_t depth) {
          if (paths.isEmpty()) {
            return true;
          }
          const PathArray array = makePathArray(pool, paths);
          return reportError(SvnError::RevertFailed,
                             svn_client_revert4(array.array, depth, nullptr,
                                                TRUE, FALSE, TRUE, ctx, pool));
        };

        QSet<QString> stagedDirectories = readStagedDirectories(wcPath);
        const QSet<QString> stagedBefore = stagedDirectories;
        QStringList folders;
        QStringList others;
        for (const QString &file : files) {
          const QString absolutePath = absoluteFilePath(wcPath, file);

          NodeStatusBaton node;
          if (!reportError(SvnError::StatusFailed,
                           readNodeStatus(&node, absolutePath, ctx, pool))) {
            return false;
          }
          if (node.status == svn_wc_status_unversioned ||
              node.status == svn_wc_status_ignored ||
              node.status == svn_wc_status_none) {
            continue;
          }
          removeAtOrBelow(&stagedDirectories, absolutePath);

          // A file of a deleted folder can't be reverted on its own: the
          // topmost deleted folder is restored, and what else was deleted in
          // it is deleted again.
          QString deletedRoot;
          for (QString parent = parentPath(absolutePath);
               parent.size() > wcPath.size(); parent = parentPath(parent)) {
            NodeStatusBaton parentNode;
            if (!reportError(SvnError::StatusFailed,
                             readNodeStatus(&parentNode, parent, ctx, pool))) {
              return false;
            }
            if (parentNode.status != svn_wc_status_deleted) {
              break;
            }
            deletedRoot = parent;
          }
          if (!deletedRoot.isEmpty()) {
            QStringList deleted;
            if (!reportError(SvnError::StatusFailed,
                             readPaths(&deleted, deletedRoot,
                                       svn_depth_infinity,
                                       [](const svn_client_status_t *status) {
                                         return status->node_status ==
                                                svn_wc_status_deleted;
                                       },
                                       ctx, pool, true))) {
              return false;
            }
            QStringList deleteAgain;
            for (const QString &deletedPath : deleted) {
              const bool keep = absolutePath == deletedPath ||
                                absolutePath.startsWith(deletedPath + '/') ||
                                deletedPath.startsWith(absolutePath + '/');
              const bool parentListed = std::any_of(
                  deleteAgain.cbegin(), deleteAgain.cend(),
                  [&](const QString &listed) {
                    return deletedPath.startsWith(listed + '/');
                  });
              if (!keep && !parentListed) {
                deleteAgain.append(deletedPath);
              }
            }
            if (!revert({deletedRoot}, svn_depth_infinity)) {
              return false;
            }
            if (!deleteAgain.isEmpty()) {
              const PathArray deletion = makePathArray(pool, deleteAgain);
              if (!reportError(SvnError::RevertFailed,
                               svn_client_delete4(deletion.array, TRUE, FALSE,
                                                  nullptr, nullptr, nullptr,
                                                  ctx, pool))) {
                return false;
              }
            }
            continue;
          }

          // A folder listed for its property change loses only that change.
          (hasOnlyPropertyChanges(node) ? folders : others).append(absolutePath);
        }

        if (!revert(folders, svn_depth_empty) ||
            !revert(others, svn_depth_infinity)) {
          return false;
        }

        // An added folder without anything added in it any more isn't added
        // either, like a folder in Git that only holds untracked files.
        for (const QString &discarded : folders + others) {
          for (QString parent = parentPath(discarded);
               parent.size() > wcPath.size(); parent = parentPath(parent)) {
            NodeStatusBaton parentNode;
            QStringList versionedChildren;
            if (!reportError(SvnError::StatusFailed,
                             readNodeStatus(&parentNode, parent, ctx, pool)) ||
                parentNode.status != svn_wc_status_added || parentNode.copied ||
                !reportError(
                    SvnError::StatusFailed,
                    readPaths(&versionedChildren, parent, svn_depth_infinity,
                              [](const svn_client_status_t *status) {
                                return status->node_status !=
                                           svn_wc_status_unversioned &&
                                       status->node_status !=
                                           svn_wc_status_ignored &&
                                       status->node_status !=
                                           svn_wc_status_none;
                              },
                              ctx, pool))) {
              break;
            }
            versionedChildren.removeAll(parent);
            if (!versionedChildren.isEmpty() ||
                !revert({parent}, svn_depth_empty)) {
              break;
            }
            stagedDirectories.remove(parent);
          }
        }

        if (stagedDirectories != stagedBefore) {
          writeStagedDirectories(wcPath, stagedDirectories);
        }
        return true;
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
  WorkingCopyInfo info;
  if (!readInfo(&info, wcPath)) {
    LOG_PERF(0, "Exit");
    return false;
  }
  if (!branchNameFromUrl(info.url, info.layoutRootUrl, BranchType::Tag)
           .isEmpty()) {
    setLastError(SvnError::CommitFailed, -1,
                 QStringLiteral("A tag can't be committed to. Check out a "
                                "branch to commit."));
    LOG_PERF(0, "Exit");
    return false;
  }

  OperationState operation;
  const bool inOperation = activeOperation(wcPath, &operation);
  if (inOperation) {
    QStringList stillConflicted;
    runWithClientContext(
        [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
          for (const QString &conflictedPath : operation.conflicted) {
            svn_boolean_t textConflicted = FALSE;
            svn_boolean_t propConflicted = FALSE;
            svn_boolean_t treeConflicted = FALSE;
            svn_error_t *error = svn_wc_conflicted_p3(
                &textConflicted, &propConflicted, &treeConflicted,
                ctx->wc_ctx, conflictedPath.toUtf8().constData(), pool);
            svn_error_clear(error);
            if (!error && (textConflicted || propConflicted || treeConflicted)) {
              stillConflicted.append(relativeFilePath(wcPath, conflictedPath));
            }
          }
          return true;
        },
        wcPath);
    if (!stillConflicted.isEmpty()) {
      setLastError(SvnError::CommitFailed, -1,
                   QStringLiteral("Resolve the conflicts first: %1")
                       .arg(stillConflicted.join(QStringLiteral(", "))));
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  const SvnStatus current = readStatus(wcPath, nullptr);
  if (current.path.isEmpty()) {
    // The status failed; its error is the one reported.
    LOG_PERF(0, "Exit");
    return false;
  }
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

        // A copied folder is committed with everything in it.
        for (const QString &file : current.renamedStagedFiles) {
          const QString absolutePath = absoluteFilePath(wcPath, file);
          if (!QFileInfo(absolutePath).isDir()) {
            continue;
          }
          QStringList copied;
          if (!reportError(SvnError::StatusFailed,
                           readPaths(&copied, absolutePath, svn_depth_infinity,
                                     [](const svn_client_status_t *status) {
                                       return status->node_status !=
                                                  svn_wc_status_unversioned &&
                                              status->node_status !=
                                                  svn_wc_status_ignored &&
                                              status->node_status !=
                                                  svn_wc_status_none;
                                     },
                                     ctx, pool, true))) {
            return false;
          }
          for (const QString &copiedPath : copied) {
            targets.insert(copiedPath);
          }
        }

        // A folder's property change can only be committed at HEAD: the
        // staged folders come there, on their own (depth empty).
        QStringList propertyFolders;
        for (const QString &target : targets) {
          NodeStatusBaton node;
          if (!reportError(SvnError::StatusFailed,
                           readNodeStatus(&node, target, ctx, pool))) {
            return false;
          }
          if (hasOnlyPropertyChanges(node)) {
            propertyFolders.append(target);
          }
        }
        if (!propertyFolders.isEmpty()) {
          const PathArray folders = makePathArray(pool, propertyFolders);
          const svn_opt_revision_t head = revisionHead();
          apr_array_header_t *resultRevisions = nullptr;
          const ConflictWatch conflicts(ctx);
          if (!reportError(SvnError::CommitFailed,
                           svn_client_update4(&resultRevisions, folders.array,
                                              &head, svn_depth_empty, FALSE,
                                              FALSE, TRUE, TRUE, FALSE, ctx,
                                              pool)) ||
              !conflicts.reportConflicts(
                  SvnError::CommitFailed, wcPath,
                  QStringLiteral("Bringing the folders to HEAD"))) {
            return false;
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

        if (!reportError(SvnError::CommitFailed,
                         commitPaths(paths, logMessage, ctx, pool))) {
          return false;
        }

        QSet<QString> stagedDirectories = readStagedDirectories(wcPath);
        if (!stagedDirectories.isEmpty()) {
          for (const QString &target : targets) {
            stagedDirectories.remove(target);
          }
          writeStagedDirectories(wcPath, stagedDirectories);
        }
        return true;
      },
      wcPath);

  if (success && inOperation) {
    removeOperationState(wcPath);
  }

  LOG_PERF(0, "Exit");
  return success;
}

bool applyPatch(const QString &path, const QString &patchContent) {
  LOG_PERF(0, "Enter");

  if (patchContent.trimmed().isEmpty()) {
    setLastError(SvnError::PatchFailed, -1, QStringLiteral("The patch is empty."));
    LOG_PERF(0, "Exit");
    return false;
  }

  // Unified diffs made by Git prefix the paths with a/ and b/. Subversion
  // removes them itself from Git style diffs ("diff --git a/... b/...").
  const bool gitHeaders = patchContent.contains(QStringLiteral("diff --git "));
  const int stripCount =
      !gitHeaders && (patchContent.contains(QStringLiteral("\n--- a/")) ||
                      patchContent.startsWith(QStringLiteral("--- a/")))
          ? 1
          : 0;

  const QString wcPath = normalizedPath(path);
  WorkingCopyInfo info;
  if (!readInfo(&info, wcPath)) {
    LOG_PERF(0, "Exit");
    return false;
  }
  QString wcRepoPath =
      trimmedUrl(info.url).mid(trimmedUrl(info.reposRootUrl).size());
  while (wcRepoPath.startsWith(QLatin1Char('/'))) {
    wcRepoPath.remove(0, 1);
  }
  wcRepoPath = QUrl::fromPercentEncoding(wcRepoPath.toUtf8());
  const QByteArray patchText =
      normalizePatch(patchContent, wcPath, wcRepoPath, stripCount);

  QTemporaryFile patchFile;
  if (!patchFile.open() || patchFile.write(patchText) < 0) {
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
        ctx->notify_func2 = nullptr;
        ctx->notify_baton2 = nullptr;
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

        // Already applied, or not a patch at all.
        if (baton.changes == 0) {
          setLastError(SvnError::PatchFailed, -1,
                       QStringLiteral("The patch changes nothing."));
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
  if (revisionNumberValue < 1) {
    setLastError(SvnError::InvalidRevision, -1,
                 QStringLiteral("Revision 0 is the empty repository."));
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString wcPath = normalizedPath(path);
  if (refuseWhileOperationInProgress(wcPath)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        const PathArray paths = makePathArray(pool, {wcPath});
        const svn_opt_revision_t revision = revisionNumber(revisionNumberValue);
        apr_array_header_t *resultRevs = nullptr;
        const ConflictWatch conflicts(ctx);
        svn_error_t *error = svn_client_update4(
            &resultRevs, paths.array, &revision, svn_depth_infinity, FALSE,
            FALSE, TRUE, TRUE, FALSE, ctx, pool);
        // A revision the repository doesn't have, or one older than the
        // checked out branch.
        // ("Target path does not exist" is an FS_PATH_SYNTAX.)
        if (error && (svn_error_find_cause(error, SVN_ERR_FS_NO_SUCH_REVISION) ||
                      svn_error_find_cause(error, SVN_ERR_FS_PATH_SYNTAX) ||
                      isMissingPathError(error))) {
          svn_error_clear(error);
          setLastError(SvnError::InvalidRevision, -1,
                       QStringLiteral("r%1 doesn't exist on this branch.")
                           .arg(revisionNumberValue));
          return false;
        }
        return reportError(SvnError::UpdateFailed, error) &&
               conflicts.reportConflicts(SvnError::UpdateFailed, wcPath,
                                         QStringLiteral("The update"));
      },
      wcPath);

  LOG_PERF(0, "Exit");
  return success;
}

QString listConflictsForAFile(const QString &path, const QString &filePath) {
  // Subversion writes the conflict markers into the working file.
  const QString wcPath = normalizedPath(path);
  const QString absolutePath = absoluteFilePath(wcPath, filePath);
  bool textConflicted = false;
  runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        svn_boolean_t text = FALSE;
        svn_boolean_t properties = FALSE;
        svn_boolean_t tree = FALSE;
        svn_error_t *error = svn_wc_conflicted_p3(
            &text, &properties, &tree, ctx->wc_ctx,
            absolutePath.toUtf8().constData(), pool);
        svn_error_clear(error);
        textConflicted = !error && text;
        return true;
      },
      wcPath);
  return textConflicted ? readTextFile(absolutePath) : QString();
}

bool getIsMergeInProgress(const QString &path) {
  return activeOperation(normalizedPath(path));
}

bool abortMerge(const QString &path) {
  LOG_PERF(0, "Enter");

  // Only a merge, cherry-pick or revert can be aborted; an update conflict
  // is the user's to resolve.
  const QString wcPath = normalizedPath(path);
  OperationState operation;
  if (!activeOperation(wcPath, &operation)) {
    setLastError(SvnError::AbortMergeNothingToAbort, -1,
                 QStringLiteral("There is no merge, cherry-pick or revert to "
                                "abort."));
    LOG_PERF(0, "Exit");
    return false;
  }

  // The operation's paths go back to BASE and leave the staging changelist;
  // what it added is removed from disk. Added folders go as a whole, first.
  QStringList addedFolders;
  for (const QString &added : operation.added) {
    if (QFileInfo(added).isDir()) {
      addedFolders.append(added);
    }
  }
  QStringList others;
  for (const QString &operationPath : operation.paths) {
    const bool inAddedFolder =
        std::any_of(addedFolders.cbegin(), addedFolders.cend(),
                    [&](const QString &folder) {
                      return operationPath == folder ||
                             operationPath.startsWith(folder + '/');
                    });
    if (!inAddedFolder) {
      others.append(operationPath);
    }
  }

  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        const auto revert = [&](const QStringList &paths, svn_depth_t depth) {
          QStringList versioned;
          for (const QString &operationPath : paths) {
            NodeStatusBaton node;
            if (!reportError(SvnError::RevertFailed,
                             readNodeStatus(&node, operationPath, ctx,
                                            pool))) {
              return false;
            }
            if (node.status != svn_wc_status_none &&
                node.status != svn_wc_status_unversioned) {
              versioned.append(operationPath);
            }
          }
          if (versioned.isEmpty()) {
            return true;
          }
          const PathArray array = makePathArray(pool, versioned);
          return reportError(SvnError::RevertFailed,
                             svn_client_revert4(array.array, depth, nullptr,
                                                TRUE, FALSE, FALSE, ctx,
                                                pool));
        };
        return revert(addedFolders, svn_depth_infinity) &&
               revert(others, svn_depth_empty);
      },
      wcPath);

  if (success) {
    QSet<QString> stagedDirectories = readStagedDirectories(wcPath);
    for (const QString &operationPath : operation.paths) {
      removeAtOrBelow(&stagedDirectories, operationPath);
    }
    writeStagedDirectories(wcPath, stagedDirectories);
    removeOperationState(wcPath);
  }

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

        // The local side goes (an edit, or the scheduled delete of a file
        // the repository changed), then the incoming side comes in.
        const svn_opt_revision_t revision = revisionHead();
        apr_array_header_t *resultRevs = nullptr;
        return reportError(SvnError::RevertFailed,
                           svn_client_revert4(paths.array, svn_depth_empty,
                                              nullptr, TRUE, FALSE, FALSE, ctx,
                                              pool)) &&
               reportError(SvnError::UpdateFailed,
                           svn_client_update4(&resultRevs, paths.array,
                                              &revision, svn_depth_empty, FALSE,
                                              FALSE, TRUE, TRUE, FALSE, ctx,
                                              pool));
      },
      wcPath);

  if (!success) {
    restoreBackup();
  } else if (!backupPath.isEmpty()) {
    // The local copy was the side the user chose to drop.
    QFile::remove(backupPath);
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
  if (refuseWhileOperationInProgress(wcPath)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        const PathArray paths = makePathArray(pool, {wcPath});
        const svn_opt_revision_t revision = revisionHead();
        apr_array_header_t *resultRevs = nullptr;
        const ConflictWatch conflicts(ctx);
        return reportError(SvnError::UpdateFailed,
                           svn_client_update4(&resultRevs, paths.array,
                                              &revision, svn_depth_infinity,
                                              FALSE, FALSE, TRUE, TRUE, FALSE,
                                              ctx, pool)) &&
               conflicts.reportConflicts(SvnError::UpdateFailed, wcPath,
                                         QStringLiteral("The update"));
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
