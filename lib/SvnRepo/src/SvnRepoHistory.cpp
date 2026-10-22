#include "SvnRepo.h"
#include "SvnRepoPrivate.h"

#include <svn_hash.h>
#include <svn_props.h>
#include <svn_time.h>

#include <ConfigManager.h>
#include <Log.h>

#include <QSet>

#include <algorithm>

#define THREAD_ID "SvnRepo"

namespace SvnRepo {

using namespace Private;

namespace {

struct LogEntry {
  svn_revnum_t revision = SVN_INVALID_REVNUM;
  QString author;
  QString message;
  qint64 timeStamp = -1;
  // Changed path to the revision it was copied from, -1 if not copied.
  QVector<QPair<QString, svn_revnum_t>> changedPaths;
  // Action of each changed path: 'A' added, 'D' deleted, 'M' modified or
  // 'R' replaced.
  QHash<QString, char> changeActions;
  // Node kind of each changed path, unknown in old repositories.
  QHash<QString, svn_node_kind_t> changeKinds;
  // The path a changed path was copied from.
  QHash<QString, QString> copyFromPaths;
};

struct LogBaton {
  QVector<LogEntry> *entries = nullptr;
};

static QString revisionProperty(apr_hash_t *properties, const char *name) {
  if (!properties) {
    return QString();
  }

  const auto *value =
      static_cast<const svn_string_t *>(svn_hash_gets(properties, name));
  return value ? QString::fromUtf8(value->data, static_cast<int>(value->len))
               : QString();
}

static svn_error_t *logCallback(void *baton, svn_log_entry_t *logEntry,
                                apr_pool_t *pool) {
  auto *payload = static_cast<LogBaton *>(baton);
  if (!payload || !logEntry || !SVN_IS_VALID_REVNUM(logEntry->revision)) {
    return SVN_NO_ERROR;
  }

  LogEntry entry;
  entry.revision = logEntry->revision;
  entry.author = revisionProperty(logEntry->revprops, SVN_PROP_REVISION_AUTHOR);
  // A log stored with CRLF (by another client) reads with LF.
  entry.message = revisionProperty(logEntry->revprops, SVN_PROP_REVISION_LOG);
  entry.message.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
  entry.message.replace(QLatin1Char('\r'), QLatin1Char('\n'));

  const QString date =
      revisionProperty(logEntry->revprops, SVN_PROP_REVISION_DATE);
  if (!date.isEmpty()) {
    apr_time_t when = 0;
    const QByteArray dateBytes = date.toUtf8();
    svn_error_t *error =
        svn_time_from_cstring(&when, dateBytes.constData(), pool);
    if (error) {
      svn_error_clear(error);
    } else {
      // Milliseconds, like GitCommitInfo::timeStamp.
      entry.timeStamp = static_cast<qint64>(when / 1000);
    }
  }

  if (logEntry->changed_paths2) {
    for (apr_hash_index_t *index =
             apr_hash_first(pool, logEntry->changed_paths2);
         index; index = apr_hash_next(index)) {
      const void *key = nullptr;
      void *value = nullptr;
      apr_hash_this(index, &key, nullptr, &value);
      const auto *changedPath = static_cast<svn_log_changed_path2_t *>(value);
      const QString path = QString::fromUtf8(static_cast<const char *>(key));
      entry.changedPaths.append({path, changedPath && changedPath->copyfrom_path
                                           ? changedPath->copyfrom_rev
                                           : SVN_INVALID_REVNUM});
      if (changedPath) {
        entry.changeActions.insert(path, changedPath->action);
        entry.changeKinds.insert(path, changedPath->node_kind);
        if (changedPath->copyfrom_path) {
          entry.copyFromPaths.insert(
              path, QString::fromUtf8(changedPath->copyfrom_path));
        }
      }
    }
  }

  payload->entries->append(entry);
  return SVN_NO_ERROR;
}

// Log of url from HEAD (or from youngest when it is valid) down to revision
// 0, or of the single revision when revision is valid. limit <= 0 reads every
// revision.
static svn_error_t *readLog(QVector<LogEntry> *entries, const QString &url,
                            svn_revnum_t revision, int limit, bool changedPaths,
                            svn_client_ctx_t *ctx, apr_pool_t *pool,
                            svn_revnum_t youngest = SVN_INVALID_REVNUM) {
  const PathArray targets = makePathArray(pool, {url});

  const svn_opt_revision_t peg =
      SVN_IS_VALID_REVNUM(revision)   ? revisionNumber(revision)
      : SVN_IS_VALID_REVNUM(youngest) ? revisionNumber(youngest)
                                      : revisionHead();

  auto *range = static_cast<svn_opt_revision_range_t *>(
      apr_pcalloc(pool, sizeof(svn_opt_revision_range_t)));
  if (SVN_IS_VALID_REVNUM(revision)) {
    range->start = revisionNumber(revision);
    range->end = revisionNumber(revision);
  } else {
    range->start = peg;
    range->end = revisionNumber(0);
  }

  apr_array_header_t *ranges =
      apr_array_make(pool, 1, sizeof(svn_opt_revision_range_t *));
  APR_ARRAY_PUSH(ranges, svn_opt_revision_range_t *) = range;

  apr_array_header_t *revisionProperties =
      apr_array_make(pool, 3, sizeof(const char *));
  APR_ARRAY_PUSH(revisionProperties, const char *) = SVN_PROP_REVISION_AUTHOR;
  APR_ARRAY_PUSH(revisionProperties, const char *) = SVN_PROP_REVISION_DATE;
  APR_ARRAY_PUSH(revisionProperties, const char *) = SVN_PROP_REVISION_LOG;

  LogBaton baton;
  baton.entries = entries;

  return svn_client_log5(targets.array, &peg, ranges, (std::max)(limit, 0),
                         changedPaths, FALSE, FALSE, revisionProperties,
                         logCallback, &baton, ctx, pool);
}

// reposRootUrl turns the repository paths of the changed files into URLs,
// layoutRootUrl is the root the branch names are taken from.
static SvnCommitInfo commitInfoFromLogEntry(const LogEntry &entry,
                                            const QString &reposRootUrl,
                                            const QString &layoutRootUrl) {
  SvnCommitInfo info;
  info.revision = entry.revision;
  info.authorName = entry.author;
  info.message = entry.message;
  info.summary = entry.message.section('\n', 0, 0).trimmed();
  info.timeStamp = entry.timeStamp;

  QStringList branches;
  for (const auto &[changedPath, copyFromRevision] : entry.changedPaths) {
    Q_UNUSED(copyFromRevision);
    const QString branch =
        branchNameFromUrl(trimmedUrl(reposRootUrl) + changedPath, layoutRootUrl,
                          BranchType::Local);
    if (!branch.isEmpty() && !branches.contains(branch)) {
      branches.append(branch);
    }
  }
  info.branches = QVector<QString>(branches.begin(), branches.end());

  return info;
}

// URL whose history is shown for branchName. An empty name or BranchType::All
// shows the history of the checked out branch.
static QString historyUrl(const WorkingCopyInfo &info,
                          const QString &branchName, BranchType type) {
  if (branchName.isEmpty() || branchName == QStringLiteral("*") ||
      type == BranchType::All || type == BranchType::Unknown) {
    return info.url;
  }

  return branchUrl(info.layoutRootUrl, branchName, type);
}

} // namespace

SvnHistory getBranchHistory(const QString &path, const QString &branchName,
                            BranchType branchType, int page,
                            const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  if (quitFlag) {
    setLastError(SvnError::Cancelled, SVN_ERR_CANCELLED,
                 QStringLiteral("The history was cancelled."));
    LOG_PERF(0, "Exit");
    return {};
  }

  const int pageSize = static_cast<int>((std::max)(
      qint64(1),
      static_cast<qint64>(ConfigManager::ConfigManager::getInstance().getLong(
          ConfigManager::ConfigKey::GitRepoHistoryPageSize))));

  // One revision more than needed tells whether there are more pages and is
  // the parent of the oldest revision of the page.
  const int firstIndex = page > 0 ? page * pageSize : 0;
  const int limit = page < 0 ? 0 : firstIndex + pageSize + 1;

  const QString wcPath = normalizedPath(path);
  const QuitScope quitScope(&quitFlag);

  WorkingCopyInfo info;
  QVector<LogEntry> entries;
  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        if (!reportError(
                SvnError::InfoFailed,
                readInfo(&info, wcPath, revisionUnspecified(), ctx, pool))) {
          return false;
        }

        const QString url = historyUrl(info, branchName, branchType);
        svn_error_t *error = readLog(&entries, url, SVN_INVALID_REVNUM, limit,
                                     true, ctx, pool);
        if (isMissingPathError(error) &&
            trimmedUrl(url) == trimmedUrl(info.url)) {
          // The checked out branch is gone or renamed at HEAD: its history up
          // to the working copy's newest revision.
          svn_error_clear(error);
          entries.clear();
          svn_revnum_t newest = SVN_INVALID_REVNUM;
          error = readMaxRevision(&newest, wcPath, false, ctx, pool);
          if (!error) {
            error = readLog(&entries, url, SVN_INVALID_REVNUM, limit, true, ctx,
                            pool, newest);
          }
        }
        return reportError(SvnError::HistoryFailed, error);
      },
      wcPath);

  if (!success || quitFlag) {
    LOG_PERF(0, "Exit");
    return {};
  }

  SvnHistory history;
  history.valid = true;

  const int lastIndex = page < 0 ? static_cast<int>(entries.size())
                                 : (std::min)(static_cast<int>(entries.size()),
                                              firstIndex + pageSize);
  history.hasMore = page >= 0 && entries.size() > lastIndex;

  for (int i = firstIndex; i < lastIndex; ++i) {
    SvnCommitInfo commitInfo = commitInfoFromLogEntry(
        entries[i], info.reposRootUrl, info.layoutRootUrl);

    // A Subversion branch has a linear history.
    if (i + 1 < entries.size()) {
      commitInfo.parents.append(QString::number(entries[i + 1].revision));
    }
    if (i > 0) {
      commitInfo.children.append(QString::number(entries[i - 1].revision));
    }

    const QString revision = QString::number(entries[i].revision);
    history.commits.append(revision);
    history.commitInfos.insert(revision, commitInfo);
  }

  LOG_PERF(0, "Exit");
  return history;
}

SvnHistory getHistory(const QString &path, int limit) {
  LOG_PERF(0, "Enter");

  const bool quit = false;
  SvnHistory history =
      getBranchHistory(path, QString(), BranchType::All, -1, quit);
  if (limit > 0 && history.commits.size() > limit) {
    for (int i = limit; i < history.commits.size(); ++i) {
      history.commitInfos.remove(history.commits[i]);
    }
    history.commits.resize(limit);
    history.hasMore = true;
  }

  LOG_PERF(0, "Exit");
  return history;
}

CommitStatus getCommitStatus(const QString &path, const QString &commitOid) {
  LOG_PERF(0, "Enter");

  svn_revnum_t revision = SVN_INVALID_REVNUM;
  if (!parseRevision(commitOid, &revision)) {
    LOG_PERF(0, "Exit");
    return {};
  }

  const QString wcPath = normalizedPath(path);
  WorkingCopyInfo info;
  QVector<LogEntry> entries;
  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        if (!reportError(
                SvnError::InfoFailed,
                readInfo(&info, wcPath, revisionUnspecified(), ctx, pool))) {
          return false;
        }

        svn_error_t *error =
            readLog(&entries, info.reposRootUrl, revision, 1, true, ctx, pool);
        if (error &&
            svn_error_find_cause(error, SVN_ERR_FS_NO_SUCH_REVISION)) {
          svn_error_clear(error);
          setLastError(SvnError::InvalidRevision, -1,
                       QStringLiteral("r%1 doesn't exist.").arg(revision));
          return false;
        }
        return reportError(SvnError::HistoryFailed, error);
      },
      wcPath);

  if (!success || entries.isEmpty()) {
    LOG_PERF(0, "Exit");
    return {};
  }

  const LogEntry &entry = entries.first();
  CommitStatus status;
  status.commit = QString::number(entry.revision);
  status.author = entry.author;
  status.message = entry.message;
  status.timeStamp = entry.timeStamp;

  // Changed paths are repository paths such as "/trunk/src/main.cpp". The
  // ones inside the working copy are reported relative to its root. Others
  // (e.g. from a parent branch) keep the repository path with the leading
  // slash, which marks them as repository-root relative.
  const QString wcRepoPath =
      info.url.mid(trimmedUrl(info.reposRootUrl).length());
  for (const auto &[changedPath, copyFromRevision] : entry.changedPaths) {
    Q_UNUSED(copyFromRevision);
    const QString file =
        changedPath.startsWith(wcRepoPath + QLatin1Char('/'))
            ? changedPath.mid(wcRepoPath.length() + 1)
            : changedPath;
    if (entry.changeKinds.value(changedPath, svn_node_unknown) ==
        svn_node_dir) {
      continue;
    }
    switch (entry.changeActions.value(changedPath)) {
    case 'A':
      status.newFiles.append(file);
      break;
    case 'D':
      status.deletedFiles.append(file);
      break;
    default:
      status.modifiedFiles.append(file);
      break;
    }
  }
  status.newFiles.sort();
  status.modifiedFiles.sort();
  status.deletedFiles.sort();

  LOG_PERF(0, "Exit");
  return status;
}

QStringList getCommitBranches(const QString &path, const QString &commitOid,
                              const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  svn_revnum_t revision = SVN_INVALID_REVNUM;
  if (quitFlag || !parseRevision(commitOid, &revision)) {
    LOG_PERF(0, "Exit");
    return {};
  }

  const QString wcPath = normalizedPath(path);
  const QuitScope quitScope(&quitFlag);

  // The branches a revision changed.
  WorkingCopyInfo info;
  QVector<LogEntry> entries;
  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        if (!reportError(
                SvnError::InfoFailed,
                readInfo(&info, wcPath, revisionUnspecified(), ctx, pool))) {
          return false;
        }

        return reportError(
            SvnError::HistoryFailed,
            readLog(&entries, info.reposRootUrl, revision, 1, true, ctx, pool));
      },
      wcPath);

  if (!success || entries.isEmpty()) {
    LOG_PERF(0, "Exit");
    return {};
  }

  const SvnCommitInfo commitInfo = commitInfoFromLogEntry(
      entries.first(), info.reposRootUrl, info.layoutRootUrl);

  LOG_PERF(0, "Exit");
  return QStringList(commitInfo.branches.begin(), commitInfo.branches.end());
}

QStringList getCommitTags(const QString &path, const QString &commitOid,
                          const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  svn_revnum_t revision = SVN_INVALID_REVNUM;
  if (quitFlag || !parseRevision(commitOid, &revision)) {
    LOG_PERF(0, "Exit");
    return {};
  }

  const QString wcPath = normalizedPath(path);
  const QuitScope quitScope(&quitFlag);
  const QStringList existingTags = listTags(wcPath);
  if (existingTags.isEmpty()) {
    LOG_PERF(0, "Exit");
    return {};
  }

  // Tags are copies: a tag points at the last change of what it was copied
  // from, which a tag copied from another tag (a rename) takes over.
  QStringList tags;
  runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        WorkingCopyInfo info;
        if (!reportError(
                SvnError::InfoFailed,
                readInfo(&info, wcPath, revisionUnspecified(), ctx, pool))) {
          return false;
        }
        const QString reposRoot = trimmedUrl(info.reposRootUrl);
        const QString tagsPath =
            trimmedUrl(info.layoutRootUrl).mid(reposRoot.size()) +
            QStringLiteral("/tags/");

        // Canonical URLs: no trailing slash, the path encoded.
        QVector<LogEntry> entries;
        svn_error_t *error =
            readLog(&entries,
                    urlOfRelativePath(reposRoot, tagsPath.mid(1).chopped(1)),
                    SVN_INVALID_REVNUM, 0, true, ctx, pool);
        if (error && (isMissingPathError(error) ||
                      error->apr_err == SVN_ERR_CLIENT_UNRELATED_RESOURCES)) {
          svn_error_clear(error);
          return true;
        }
        if (!reportError(SvnError::HistoryFailed, error)) {
          return false;
        }

        // The newest copy that made each tag path (the log is newest first).
        QHash<QString, QPair<QString, svn_revnum_t>> sources;
        for (const LogEntry &entry : entries) {
          for (const auto &[changedPath, copyFromRevision] :
               entry.changedPaths) {
            const QString name = changedPath.mid(tagsPath.size());
            if (!changedPath.startsWith(tagsPath) || name.contains('/') ||
                !SVN_IS_VALID_REVNUM(copyFromRevision) ||
                sources.contains(name)) {
              continue;
            }
            sources.insert(name, {entry.copyFromPaths.value(changedPath),
                                  copyFromRevision});
          }
        }

        for (const QString &tag : existingTags) {
          QString name = tag;
          for (int depth = 0; depth < 16 && sources.contains(name); ++depth) {
            const auto &[fromPath, fromRevision] = sources.value(name);
            if (fromPath.startsWith(tagsPath) &&
                !fromPath.mid(tagsPath.size()).contains('/')) {
              name = fromPath.mid(tagsPath.size());
              continue;
            }
            // The last change can't be newer than the copy's revision.
            if (fromRevision < revision) {
              break;
            }
            WorkingCopyInfo source;
            const QByteArray sourceUrl =
                urlOfRelativePath(reposRoot, fromPath.mid(1)).toUtf8();
            const svn_opt_revision_t at = revisionNumber(fromRevision);
            svn_error_t *infoError = svn_client_info4(
                sourceUrl.constData(), &at, &at, svn_depth_empty, FALSE, TRUE,
                FALSE, nullptr,
                [](void *baton, const char *, const svn_client_info2_t *item,
                   apr_pool_t *) -> svn_error_t * {
                  static_cast<WorkingCopyInfo *>(baton)->lastChangedRevision =
                      item->last_changed_rev;
                  return SVN_NO_ERROR;
                },
                &source, ctx, pool);
            if (infoError) {
              svn_error_clear(infoError);
            } else if (source.lastChangedRevision == revision) {
              tags.append(tag);
            }
            break;
          }
        }
        return true;
      },
      wcPath);

  tags.sort(Qt::CaseInsensitive);

  LOG_PERF(0, "Exit");
  return tags;
}

namespace {

struct HeadsBaton {
  QVector<QPair<QString, svn_revnum_t>> *heads = nullptr;
};

static svn_error_t *headsCallback(void *baton, const char *path,
                                  const svn_dirent_t *dirent,
                                  const svn_lock_t *, const char *,
                                  const char *, const char *, apr_pool_t *) {
  auto *payload = static_cast<HeadsBaton *>(baton);
  if (payload && path && *path && dirent && dirent->kind == svn_node_dir) {
    payload->heads->append({QString::fromUtf8(path), dirent->created_rev});
  }
  return SVN_NO_ERROR;
}

} // namespace

QStringList getCommitHeads(const QString &path, const QString &commitOid,
                           const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  svn_revnum_t revision = SVN_INVALID_REVNUM;
  if (quitFlag || !parseRevision(commitOid, &revision)) {
    LOG_PERF(0, "Exit");
    return {};
  }

  const QString wcPath = normalizedPath(path);
  const QuitScope quitScope(&quitFlag);

  // The branches whose newest revision is the revision.
  QVector<QPair<QString, svn_revnum_t>> heads;
  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        WorkingCopyInfo info;
        if (!reportError(
                SvnError::InfoFailed,
                readInfo(&info, wcPath, revisionUnspecified(), ctx, pool))) {
          return false;
        }

        const svn_opt_revision_t head = revisionHead();
        HeadsBaton baton;
        baton.heads = &heads;

        const QString root = trimmedUrl(info.layoutRootUrl);
        WorkingCopyInfo trunk;
        svn_error_t *error =
            readInfo(&trunk, root + QStringLiteral("/trunk"), head, ctx, pool);
        if (isMissingPathError(error)) {
          svn_error_clear(error);
        } else if (error) {
          return reportError(SvnError::InfoFailed, error);
        } else {
          heads.append({QStringLiteral("trunk"), trunk.lastChangedRevision});
        }

        const QByteArray branchesUrl =
            (root + QStringLiteral("/branches")).toUtf8();
        error = svn_client_list4(
            branchesUrl.constData(), &head, &head, nullptr,
            svn_depth_immediates, SVN_DIRENT_KIND | SVN_DIRENT_CREATED_REV,
            FALSE, FALSE, headsCallback, &baton, ctx, pool);
        if (isMissingPathError(error)) {
          // No branches directory.
          svn_error_clear(error);
          return true;
        }

        return reportError(SvnError::InfoFailed, error);
      },
      wcPath);

  if (!success) {
    LOG_PERF(0, "Exit");
    return {};
  }

  QStringList result;
  for (const auto &[name, createdRevision] : heads) {
    if (createdRevision == revision) {
      result.append(name);
    }
  }

  LOG_PERF(0, "Exit");
  return result;
}

bool getCommitIsHead(const QString &path, const QString &commitOid,
                     const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  svn_revnum_t revision = SVN_INVALID_REVNUM;
  if (quitFlag || !parseRevision(commitOid, &revision)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  // The newest change in the working copy, its own commits included.
  const QString wcPath = normalizedPath(path);
  svn_revnum_t newest = SVN_INVALID_REVNUM;
  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        return reportError(SvnError::InfoFailed,
                           readMaxRevision(&newest, wcPath, true, ctx, pool));
      },
      wcPath);

  LOG_PERF(0, "Exit");
  return success && newest == revision;
}

} // namespace SvnRepo
