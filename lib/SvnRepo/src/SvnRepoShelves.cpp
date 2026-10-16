#include "SvnRepo.h"
#include "SvnRepoPrivate.h"

#include <private/svn_client_shelf2.h>

#include <svn_hash.h>

#include <Log.h>

#include <QDateTime>
#include <QDir>

#include <algorithm>

#define THREAD_ID "SvnRepo"

namespace SvnRepo {

using namespace Private;

namespace {

struct ShelfEntry {
  QString name;
  apr_time_t mtime = 0;
};

// Shelves of the working copy, newest first, so index 0 is the newest
// stash like in Git.
static svn_error_t *readShelves(QVector<ShelfEntry> *shelves,
                                const QString &wcPath, svn_client_ctx_t *ctx,
                                apr_pool_t *pool) {
  const QByteArray wcBytes = wcPath.toUtf8();
  apr_hash_t *infos = nullptr;
  SVN_ERR(
      svn_client__shelf2_list(&infos, wcBytes.constData(), ctx, pool, pool));

  for (apr_hash_index_t *index = apr_hash_first(pool, infos); index;
       index = apr_hash_next(index)) {
    const void *key = nullptr;
    void *value = nullptr;
    apr_hash_this(index, &key, nullptr, &value);
    const auto *info = static_cast<svn_client__shelf2_info_t *>(value);
    shelves->append({QString::fromUtf8(static_cast<const char *>(key)),
                     info ? info->mtime : 0});
  }

  std::sort(shelves->begin(), shelves->end(),
            [](const ShelfEntry &left, const ShelfEntry &right) {
              return left.mtime != right.mtime ? left.mtime > right.mtime
                                               : left.name > right.name;
            });
  return SVN_NO_ERROR;
}

static svn_error_t *shelfAt(ShelfEntry *shelf, int index, const QString &wcPath,
                            svn_client_ctx_t *ctx, apr_pool_t *pool) {
  QVector<ShelfEntry> shelves;
  SVN_ERR(readShelves(&shelves, wcPath, ctx, pool));
  if (index < 0 || index >= shelves.size()) {
    return svn_error_createf(SVN_ERR_ILLEGAL_TARGET, nullptr,
                             "There is no stash with index %d", index);
  }

  *shelf = shelves[index];
  return SVN_NO_ERROR;
}

static svn_error_t *readLogMessage(QString *message, const QString &name,
                                   const QString &wcPath, svn_client_ctx_t *ctx,
                                   apr_pool_t *pool) {
  const QByteArray nameBytes = name.toUtf8();
  const QByteArray wcBytes = wcPath.toUtf8();
  svn_client__shelf2_t *shelf = nullptr;
  SVN_ERR(svn_client__shelf2_open_existing(&shelf, nameBytes.constData(),
                                           wcBytes.constData(), ctx, pool));

  char *logMessage = nullptr;
  SVN_ERR(svn_client__shelf2_get_log_message(&logMessage, shelf, pool));
  *message = logMessage ? QString::fromUtf8(logMessage) : name;

  return svn_client__shelf2_close(shelf, pool);
}

struct StashFilesBaton {
  CommitStatus *status = nullptr;
  QString wcPath;
};

static svn_error_t *collectStashFile(void *baton, const char *abspath,
                                     const svn_wc_status3_t *status,
                                     apr_pool_t *scratch_pool) {
  auto *files = static_cast<StashFilesBaton *>(baton);
  const QString file =
      QDir(files->wcPath).relativeFilePath(QString::fromUtf8(abspath));

  switch (status->node_status) {
  case svn_wc_status_added:
  case svn_wc_status_replaced:
    files->status->newFiles.append(file);
    break;
  case svn_wc_status_deleted:
    files->status->deletedFiles.append(file);
    break;
  case svn_wc_status_modified:
    files->status->modifiedFiles.append(file);
    break;
  default:
    break;
  }
  return SVN_NO_ERROR;
}

// Fills the changed files of a shelf, relative to the working copy root.
static svn_error_t *readStashFiles(CommitStatus *status, const QString &name,
                                   const QString &wcPath, svn_client_ctx_t *ctx,
                                   apr_pool_t *pool) {
  const QByteArray nameBytes = name.toUtf8();
  const QByteArray wcBytes = wcPath.toUtf8();
  svn_client__shelf2_t *shelf = nullptr;
  SVN_ERR(svn_client__shelf2_open_existing(&shelf, nameBytes.constData(),
                                           wcBytes.constData(), ctx, pool));

  svn_client__shelf2_version_t *version = nullptr;
  SVN_ERR(svn_client__shelf2_get_newest_version(&version, shelf, pool, pool));
  if (version) {
    StashFilesBaton baton{status, wcPath};
    SVN_ERR(svn_client__shelf2_version_status_walk(
        version, "", collectStashFile, &baton, pool));
  }

  return svn_client__shelf2_close(shelf, pool);
}

// Applies the newest version of a shelf to the working copy.
static svn_error_t *applyShelf(const QString &name, const QString &wcPath,
                               svn_client_ctx_t *ctx, apr_pool_t *pool) {
  const QByteArray nameBytes = name.toUtf8();
  const QByteArray wcBytes = wcPath.toUtf8();
  svn_client__shelf2_t *shelf = nullptr;
  SVN_ERR(svn_client__shelf2_open_existing(&shelf, nameBytes.constData(),
                                           wcBytes.constData(), ctx, pool));

  svn_client__shelf2_version_t *version = nullptr;
  SVN_ERR(svn_client__shelf2_get_newest_version(&version, shelf, pool, pool));
  if (!version) {
    return svn_error_createf(SVN_ERR_ILLEGAL_TARGET, nullptr,
                             "Stash '%s' has no changes",
                             nameBytes.constData());
  }

  SVN_ERR(svn_client__shelf2_apply(version, FALSE, pool));
  return svn_client__shelf2_close(shelf, pool);
}

} // namespace

bool stash(const QString &path, const QString &note) {
  LOG_PERF(0, "Enter");

  const QString wcPath = normalizedPath(path);
  const QByteArray name = QStringLiteral("sourcerer-%1")
                              .arg(QDateTime::currentDateTimeUtc().toString(
                                  QStringLiteral("yyyyMMddHHmmsszzz")))
                              .toUtf8();
  const QString branch = getActiveBranch(wcPath, BranchType::Local, false);
  const QByteArray message =
      (note.trimmed().isEmpty()
           ? QStringLiteral("WIP on %1").arg(branch)
           : QStringLiteral("On %1: %2").arg(branch, note.trimmed()))
          .toUtf8();

  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        const QByteArray wcBytes = wcPath.toUtf8();
        svn_client__shelf2_t *shelf = nullptr;
        if (!reportError(SvnError::ShelveFailed,
                         svn_client__shelf2_open_or_create(
                             &shelf, name.constData(), wcBytes.constData(), ctx,
                             pool))) {
          return false;
        }

        // Saving a version does not revert the changes, unapplying does.
        const PathArray paths = makePathArray(pool, {wcPath});
        svn_client__shelf2_version_t *version = nullptr;
        svn_error_t *error = svn_client__shelf2_save_new_version3(
            &version, shelf, paths.array, svn_depth_infinity, nullptr, nullptr,
            nullptr, nullptr, nullptr, pool);
        if (!error && !version) {
          error = svn_error_create(SVN_ERR_ILLEGAL_TARGET, nullptr,
                                   "There are no local changes to stash");
        }
        if (!error) {
          error = svn_client__shelf2_set_log_message(shelf, message.constData(),
                                                     pool);
        }
        if (!error) {
          error = svn_client__shelf2_unapply(version, FALSE, pool);
        }

        if (error) {
          svn_error_clear(svn_client__shelf2_close(shelf, pool));
          if (!version) {
            svn_error_clear(svn_client__shelf2_delete(
                name.constData(), wcBytes.constData(), FALSE, ctx, pool));
          }
          return reportError(SvnError::ShelveFailed, error);
        }

        return reportError(SvnError::ShelveFailed,
                           svn_client__shelf2_close(shelf, pool));
      },
      wcPath);

  LOG_PERF(0, "Exit");
  return success;
}

QStringList listStashes(const QString &path, const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  if (quitFlag) {
    LOG_PERF(0, "Exit");
    return {};
  }

  const QString wcPath = normalizedPath(path);
  QStringList stashes;
  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        QVector<ShelfEntry> shelves;
        if (!reportError(SvnError::ShelveFailed,
                         readShelves(&shelves, wcPath, ctx, pool))) {
          return false;
        }

        for (const ShelfEntry &shelf : shelves) {
          QString message;
          svn_error_t *error =
              readLogMessage(&message, shelf.name, wcPath, ctx, pool);
          if (error) {
            svn_error_clear(error);
            message = shelf.name;
          }
          stashes.append(message);
        }

        return true;
      },
      wcPath);

  LOG_PERF(0, "Exit");
  return success ? stashes : QStringList();
}

bool applyStash(const QString &path, int index) {
  LOG_PERF(0, "Enter");

  const QString wcPath = normalizedPath(path);
  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        ShelfEntry shelf;
        return reportError(SvnError::ShelveFailed,
                           shelfAt(&shelf, index, wcPath, ctx, pool)) &&
               reportError(SvnError::ShelveFailed,
                           applyShelf(shelf.name, wcPath, ctx, pool));
      },
      wcPath);

  LOG_PERF(0, "Exit");
  return success;
}

bool deleteStash(const QString &path, int index) {
  LOG_PERF(0, "Enter");

  const QString wcPath = normalizedPath(path);
  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        ShelfEntry shelf;
        if (!reportError(SvnError::ShelveFailed,
                         shelfAt(&shelf, index, wcPath, ctx, pool))) {
          return false;
        }

        const QByteArray nameBytes = shelf.name.toUtf8();
        const QByteArray wcBytes = wcPath.toUtf8();
        return reportError(SvnError::ShelveFailed,
                           svn_client__shelf2_delete(nameBytes.constData(),
                                                     wcBytes.constData(), FALSE,
                                                     ctx, pool));
      },
      wcPath);

  LOG_PERF(0, "Exit");
  return success;
}

bool deleteAllStashes(const QString &path) {
  LOG_PERF(0, "Enter");

  const QString wcPath = normalizedPath(path);
  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        QVector<ShelfEntry> shelves;
        if (!reportError(SvnError::ShelveFailed,
                         readShelves(&shelves, wcPath, ctx, pool))) {
          return false;
        }

        const QByteArray wcBytes = wcPath.toUtf8();
        for (const ShelfEntry &shelf : shelves) {
          const QByteArray nameBytes = shelf.name.toUtf8();
          if (!reportError(SvnError::ShelveFailed,
                           svn_client__shelf2_delete(nameBytes.constData(),
                                                     wcBytes.constData(), FALSE,
                                                     ctx, pool))) {
            return false;
          }
        }

        return true;
      },
      wcPath);

  LOG_PERF(0, "Exit");
  return success;
}

CommitStatus getStashStatus(const QString &path, int stashIndex) {
  LOG_PERF(0, "Enter");

  const QString wcPath = normalizedPath(path);
  CommitStatus status;
  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        ShelfEntry shelf;
        if (!reportError(SvnError::ShelveFailed,
                         shelfAt(&shelf, stashIndex, wcPath, ctx, pool))) {
          return false;
        }

        status.commit = shelf.name;
        status.timeStamp = static_cast<qint64>(shelf.mtime / 1000);
        if (!reportError(SvnError::ShelveFailed,
                         readLogMessage(&status.message, shelf.name, wcPath,
                                        ctx, pool))) {
          return false;
        }

        return reportError(
            SvnError::ShelveFailed,
            readStashFiles(&status, shelf.name, wcPath, ctx, pool));
      },
      wcPath);

  LOG_PERF(0, "Exit");
  return success ? status : CommitStatus();
}

QString listConflictsForStashFile(const QString &path, int index,
                                  const QString &filePath) {
  // Conflicts of an applied stash are written into the working file.
  Q_UNUSED(index);
  return readTextFile(absoluteFilePath(normalizedPath(path), filePath));
}

} // namespace SvnRepo
