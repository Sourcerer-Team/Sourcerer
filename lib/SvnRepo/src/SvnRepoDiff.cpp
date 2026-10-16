#include "SvnRepo.h"
#include "SvnRepoPrivate.h"

#include <private/svn_client_shelf2.h>
#include <svn_diff.h>
#include <svn_hash.h>
#include <svn_io.h>
#include <svn_props.h>
#include <svn_string.h>
#include <svn_wc.h>

#include <ConfigManager.h>
#include <Log.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>

#define THREAD_ID "SvnRepo"

namespace SvnRepo {

using namespace Private;

namespace {

static qint64 maxDiffFileSize() {
  return ConfigManager::ConfigManager::getInstance().getLong(
      ConfigManager::ConfigKey::GitRepoMaxDiffFileSize);
}

// Diff between two revisions of path_or_url as unified diff text.
static svn_error_t *
runDiff(QString *diff, const QString &pathOrUrl, const svn_opt_revision_t &peg,
        const svn_opt_revision_t &start, const svn_opt_revision_t &end,
        const QString &relativeToDir, svn_depth_t depth, bool gitFormat,
        svn_client_ctx_t *ctx, apr_pool_t *pool) {
  svn_stringbuf_t *outBuffer = svn_stringbuf_create_empty(pool);
  svn_stream_t *outStream = svn_stream_from_stringbuf(outBuffer, pool);
  svn_stream_t *errStream = svn_stream_empty(pool);

  const QByteArray target = pathOrUrl.toUtf8();
  const QByteArray relativeTo = relativeToDir.toUtf8();
  SVN_ERR(svn_client_diff_peg7(
      nullptr, target.constData(), &peg, &start, &end,
      relativeToDir.isEmpty() ? nullptr : relativeTo.constData(), depth, FALSE,
      FALSE, FALSE, gitFormat, FALSE, !gitFormat, FALSE, gitFormat, FALSE,
      "UTF-8", outStream, errStream, nullptr, ctx, pool));
  SVN_ERR(svn_stream_close(outStream));

  *diff = QString::fromUtf8(outBuffer->data, static_cast<int>(outBuffer->len));
  return SVN_NO_ERROR;
}

static QString diffText(const QVector<FilesDiffDelta> &deltas);
static QString contentWithinLimit(const QByteArray &content);

// Stashes are shelves, which are not revisions. Each shelved file keeps its
// base copy as "<relpath>.base" and its work copy as "<relpath>.work".
static svn_error_t *shelfFilesDir(QString *filesDir, const QString &name,
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
    *filesDir = QString::fromUtf8(version->files_dir_abspath);
  }
  return svn_client__shelf2_close(shelf, pool);
}

static QString shelfCopyPath(const QString &filesDir,
                             const QString &relativePath, const char *suffix) {
  return QDir(filesDir).filePath(relativePath + QString::fromLatin1(suffix));
}

// Content of a shelved copy, empty when the copy does not exist.
static QByteArray readShelfCopy(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  return file.readAll();
}

static QString unifiedDiffOfContents(const QByteArray &before,
                                     const QByteArray &after,
                                     const QString &file, apr_pool_t *pool) {
  const svn_string_t *original =
      svn_string_ncreate(before.constData(), before.size(), pool);
  const svn_string_t *modified =
      svn_string_ncreate(after.constData(), after.size(), pool);

  svn_diff_t *diff = nullptr;
  svn_diff_file_options_t *options = svn_diff_file_options_create(pool);
  if (svn_diff_mem_string_diff(&diff, original, modified, options, pool)) {
    return QString();
  }

  svn_stringbuf_t *outBuffer = svn_stringbuf_create_empty(pool);
  svn_stream_t *outStream = svn_stream_from_stringbuf(outBuffer, pool);
  const QByteArray header = file.toUtf8();
  if (svn_diff_mem_string_output_unified3(
          outStream, diff, TRUE, "@@", header.constData(), header.constData(),
          "UTF-8", original, modified, 3, nullptr, nullptr, pool)) {
    return QString();
  }
  if (svn_stream_close(outStream)) {
    return QString();
  }

  // parseUnifiedDiff starts a new file only on an "Index:" line.
  return QStringLiteral("Index: %1\n").arg(file) +
         QString::fromUtf8(outBuffer->data, static_cast<int>(outBuffer->len));
}

static QString shelfRelativePath(const QString &wcPath, const QString &file) {
  return QFileInfo(file).isAbsolute() ? relativeFilePath(wcPath, file)
                                      : QDir::fromNativeSeparators(file);
}

static FilesDiff diffFilesFromShelf(const QString &path,
                                    const QStringList &files,
                                    const QString &shelfName) {
  const QString wcPath = normalizedPath(path);
  FilesDiff result;

  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        QString filesDir;
        if (!reportError(
                SvnError::DiffFailed,
                shelfFilesDir(&filesDir, shelfName, wcPath, ctx, pool))) {
          return false;
        }

        for (const QString &file : files) {
          const QString relativePath = shelfRelativePath(wcPath, file);
          const QByteArray oldContent =
              readShelfCopy(shelfCopyPath(filesDir, relativePath, ".base"));
          const QByteArray newContent =
              readShelfCopy(shelfCopyPath(filesDir, relativePath, ".work"));

          result.repoContent.insert(file, contentWithinLimit(oldContent));
          result.filesContent.insert(file, contentWithinLimit(newContent));

          const QVector<FilesDiffDelta> deltas = parseUnifiedDiff(
              unifiedDiffOfContents(oldContent, newContent, file, pool));
          for (FilesDiffDelta delta : deltas) {
            delta.oldFilePath = file;
            delta.newFilePath = file;
            result.deltas.append(delta);
          }
        }

        return true;
      },
      wcPath);

  if (!success) {
    return {};
  }

  result.diff = diffText(result.deltas);
  result.valid = true;
  return result;
}

static qint64 fileSizeFromShelf(const QString &path, const QString &filePath,
                                const QString &shelfName) {
  const QString wcPath = normalizedPath(path);
  qint64 size = -1;

  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        QString filesDir;
        if (!reportError(
                SvnError::InfoFailed,
                shelfFilesDir(&filesDir, shelfName, wcPath, ctx, pool))) {
          return false;
        }

        const QFileInfo info(shelfCopyPath(
            filesDir, shelfRelativePath(wcPath, filePath), ".work"));
        if (info.exists()) {
          size = info.size();
        }
        return true;
      },
      wcPath);

  return success ? size : -1;
}

// Delta adding every line of content, for unversioned files.
static FilesDiffDelta addedFileDelta(const QString &file,
                                     const QString &content) {
  FilesDiffDelta delta;
  delta.oldFilePath = file;
  delta.newFilePath = file;

  QStringList lines = QString(content).remove('\r').split('\n');
  if (!lines.isEmpty() && lines.last().isEmpty()) {
    lines.removeLast();
  }

  if (lines.isEmpty()) {
    return delta;
  }

  FilesDiffHunk hunk;
  hunk.oldStart = 0;
  hunk.oldLines = 0;
  hunk.newStart = 1;
  hunk.newLines = static_cast<int>(lines.size());
  for (const QString &line : lines) {
    hunk.lines.append(FilesDiffLine{QChar('+'), line});
  }
  delta.hunks.append(hunk);
  return delta;
}

// Diff text in the format GitRepo uses for FilesDiff::diff.
static QString diffText(const QVector<FilesDiffDelta> &deltas) {
  QString text;
  for (const FilesDiffDelta &delta : deltas) {
    text += QStringLiteral("diff --git a/%1 b/%2\n")
                .arg(delta.oldFilePath, delta.newFilePath);
    for (const FilesDiffHunk &hunk : delta.hunks) {
      text += QStringLiteral("@@ -%1,%2 +%3,%4 @@\n")
                  .arg(hunk.oldStart)
                  .arg(hunk.oldLines)
                  .arg(hunk.newStart)
                  .arg(hunk.newLines);
      for (const FilesDiffLine &line : hunk.lines) {
        text += line.origin + line.content + QStringLiteral("\n");
      }
    }
  }
  return text;
}

struct FileStatusBaton {
  svn_wc_status_kind status = svn_wc_status_none;
};

static svn_error_t *fileStatusCallback(void *baton, const char *,
                                       const svn_client_status_t *status,
                                       apr_pool_t *) {
  if (baton && status) {
    static_cast<FileStatusBaton *>(baton)->status = status->node_status;
  }
  return SVN_NO_ERROR;
}

static QString contentWithinLimit(const QByteArray &content) {
  const qint64 limit = maxDiffFileSize();
  if (limit > 0 && content.size() > limit) {
    return QString();
  }
  return QString::fromUtf8(content);
}

struct FileSizeBaton {
  qint64 size = -1;
};

static svn_error_t *fileSizeCallback(void *baton, const char *,
                                     const svn_dirent_t *dirent,
                                     const svn_lock_t *, const char *,
                                     const char *, const char *, apr_pool_t *) {
  if (baton && dirent && dirent->kind == svn_node_file) {
    static_cast<FileSizeBaton *>(baton)->size =
        static_cast<qint64>(dirent->size);
  }
  return SVN_NO_ERROR;
}

struct BlameBaton {
  BlameFileInfo *file = nullptr;
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

static svn_error_t *blameCallback(void *baton, apr_int64_t lineNumber,
                                  svn_revnum_t revision,
                                  apr_hash_t *revisionProperties, svn_revnum_t,
                                  apr_hash_t *, const char *,
                                  const svn_string_t *line,
                                  svn_boolean_t localChange, apr_pool_t *) {
  auto *payload = static_cast<BlameBaton *>(baton);
  if (!payload || !payload->file) {
    return SVN_NO_ERROR;
  }

  BlameLineInfo info;
  info.filePath = payload->file->filePath;
  info.lineNumber = static_cast<qint64>(lineNumber) + 1;
  info.content =
      line ? QString::fromUtf8(line->data, static_cast<int>(line->len))
                 .remove('\r')
           : QString();
  if (!localChange && SVN_IS_VALID_REVNUM(revision)) {
    info.commit = QString::number(revision);
    info.author =
        revisionProperty(revisionProperties, SVN_PROP_REVISION_AUTHOR);
    info.summary = revisionProperty(revisionProperties, SVN_PROP_REVISION_LOG)
                       .section('\n', 0, 0)
                       .trimmed();
  }

  payload->file->lineInfos.append(info);
  return SVN_NO_ERROR;
}

} // namespace

FilesDiff diffFiles(const QString &path, const QStringList &files) {
  LOG_PERF(0, "Enter");

  const QString wcPath = normalizedPath(path);
  FilesDiff result;

  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        for (const QString &file : files) {
          const QString absolutePath = absoluteFilePath(wcPath, file);
          const QByteArray pathBytes = absolutePath.toUtf8();

          FileStatusBaton status;
          const svn_opt_revision_t working = revisionWorking();
          svn_revnum_t youngest = SVN_INVALID_REVNUM;
          svn_error_t *statusError = svn_client_status6(
              &youngest, ctx, pathBytes.constData(), &working, svn_depth_empty,
              TRUE, FALSE, TRUE, TRUE, TRUE, FALSE, nullptr, fileStatusCallback,
              &status, pool);
          // Files below an unversioned directory are unknown to Subversion.
          if (statusError && statusError->apr_err == SVN_ERR_WC_PATH_NOT_FOUND) {
            svn_error_clear(statusError);
            statusError = SVN_NO_ERROR;
          }
          if (!reportError(SvnError::StatusFailed, statusError)) {
            return false;
          }

          const QFileInfo fileInfo(absolutePath);
          const bool onDisk = fileInfo.isFile();
          const QString workingContent =
              onDisk && (maxDiffFileSize() <= 0 ||
                         fileInfo.size() <= maxDiffFileSize())
                  ? readTextFile(absolutePath)
                  : QString();
          result.filesContent.insert(file, workingContent);

          if (status.status == svn_wc_status_unversioned ||
              status.status == svn_wc_status_ignored ||
              status.status == svn_wc_status_none) {
            result.repoContent.insert(file, QString());
            result.deltas.append(addedFileDelta(file, workingContent));
            continue;
          }

          QByteArray baseContent;
          if (status.status != svn_wc_status_added) {
            svn_error_t *error = readFileAtRevision(&baseContent, absolutePath,
                                                    revisionUnspecified(),
                                                    revisionBase(), ctx, pool);
            if (error) {
              svn_error_clear(error);
              baseContent.clear();
            }
          }
          result.repoContent.insert(file, contentWithinLimit(baseContent));

          QString diff;
          if (!reportError(SvnError::DiffFailed,
                           runDiff(&diff, absolutePath, revisionUnspecified(),
                                   revisionBase(), revisionWorking(), wcPath,
                                   svn_depth_empty, false, ctx, pool))) {
            return false;
          }

          const QVector<FilesDiffDelta> deltas = parseUnifiedDiff(diff);
          for (FilesDiffDelta delta : deltas) {
            delta.oldFilePath = file;
            delta.newFilePath = file;
            result.deltas.append(delta);
          }
        }

        return true;
      },
      wcPath);

  if (!success) {
    LOG_PERF(0, "Exit");
    return {};
  }

  result.diff = diffText(result.deltas);
  result.valid = true;

  LOG_PERF(0, "Exit");
  return result;
}

FilesDiff diffFilesFromCommit(const QString &path, const QStringList &files,
                              const QString &commit) {
  LOG_PERF(0, "Enter");

  svn_revnum_t revision = SVN_INVALID_REVNUM;
  if (!parseRevision(commit, &revision)) {
    LOG_PERF(0, "Exit");
    return diffFilesFromShelf(path, files, commit);
  }

  const QString wcPath = normalizedPath(path);
  FilesDiff result;

  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        WorkingCopyInfo info;
        if (!reportError(
                SvnError::InfoFailed,
                readInfo(&info, wcPath, revisionUnspecified(), ctx, pool))) {
          return false;
        }

        const svn_opt_revision_t peg = revisionNumber(revision);
        const svn_opt_revision_t previous =
            revisionNumber(revision > 0 ? revision - 1 : 0);

        for (const QString &file : files) {
          const QString relativePath = QFileInfo(file).isAbsolute()
                                           ? relativeFilePath(wcPath, file)
                                           : QDir::fromNativeSeparators(file);
          const QString fileUrl = urlOfRelativePath(info.url, relativePath);

          QByteArray oldContent;
          svn_error_t *error = readFileAtRevision(
              &oldContent, fileUrl, previous, previous, ctx, pool);
          if (error) {
            svn_error_clear(error);
            oldContent.clear();
          }
          result.repoContent.insert(file, contentWithinLimit(oldContent));

          QByteArray newContent;
          error = readFileAtRevision(&newContent, fileUrl, peg, peg, ctx, pool);
          if (error) {
            svn_error_clear(error);
            newContent.clear();
          }
          result.filesContent.insert(file, contentWithinLimit(newContent));

          QString diff;
          error = runDiff(&diff, fileUrl, peg, previous, peg, QString(),
                          svn_depth_empty, false, ctx, pool);
          if (error) {
            // The file did not exist before the revision.
            svn_error_clear(error);
            result.deltas.append(
                addedFileDelta(file, QString::fromUtf8(newContent)));
            continue;
          }

          const QVector<FilesDiffDelta> deltas = parseUnifiedDiff(diff);
          for (FilesDiffDelta delta : deltas) {
            delta.oldFilePath = file;
            delta.newFilePath = file;
            result.deltas.append(delta);
          }
        }

        return true;
      },
      wcPath);

  if (!success) {
    LOG_PERF(0, "Exit");
    return {};
  }

  result.diff = diffText(result.deltas);
  result.valid = true;

  LOG_PERF(0, "Exit");
  return result;
}

QString createPatchFromCommit(const QString &path, const QString &commit) {
  LOG_PERF(0, "Enter");

  svn_revnum_t revision = SVN_INVALID_REVNUM;
  if (!parseRevision(commit, &revision)) {
    LOG_PERF(0, "Exit");
    return QString();
  }

  const QString wcPath = normalizedPath(path);
  QString patch;
  QString checkoutPrefix;

  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        WorkingCopyInfo info;
        if (!reportError(
                SvnError::InfoFailed,
                readInfo(&info, wcPath, revisionUnspecified(), ctx, pool))) {
          return false;
        }

        // Subversion writes headers relative to the parent of the checkout
        // folder (e.g. "branches/file.txt" for a checkout of
        // "branches/test1"), so remember that parent to strip it below.
        const QString reposRoot = Private::trimmedUrl(info.reposRootUrl);
        QString checkoutRepoPath = info.url.mid(reposRoot.length());
        if (checkoutRepoPath.startsWith(QLatin1Char('/')))
          checkoutRepoPath.remove(0, 1);
        const int slash = checkoutRepoPath.lastIndexOf(QLatin1Char('/'));
        if (slash > 0)
          checkoutPrefix = checkoutRepoPath.left(slash + 1);

        // Git format, so the patch can be applied to Git repositories too.
        const svn_opt_revision_t peg = revisionNumber(revision);
        const svn_opt_revision_t previous =
            revisionNumber(revision > 0 ? revision - 1 : 0);
        return reportError(SvnError::DiffFailed,
                           runDiff(&patch, info.url, peg, previous, peg,
                                   QString(), svn_depth_infinity, true, ctx,
                                   pool));
      },
      wcPath);

  if (!success)
    return QString();

  // Make header paths relative to the checkout root, so the patch applies
  // to any checkout of the same branch. Only "diff --git", "---" and "+++"
  // lines carry paths; the content is untouched.
  if (!checkoutPrefix.isEmpty()) {
    const QString aPrefix = QLatin1String("a/") + checkoutPrefix;
    const QString bPrefix = QLatin1String("b/") + checkoutPrefix;
    const QStringList lines = patch.split(QLatin1Char('\n'));
    QStringList result;
    result.reserve(lines.size());
    for (QString line : lines) {
      if (line.startsWith(QLatin1String("diff --git ") + aPrefix)) {
        line.replace(QLatin1String("diff --git ") + aPrefix,
                     QLatin1String("diff --git a/"));
        line.replace(QLatin1Char(' ') + bPrefix, QLatin1String(" b/"));
      } else if (line.startsWith(QLatin1String("--- ") + aPrefix)) {
        line.replace(QLatin1String("--- ") + aPrefix, QLatin1String("--- a/"));
      } else if (line.startsWith(QLatin1String("+++ ") + bPrefix)) {
        line.replace(QLatin1String("+++ ") + bPrefix, QLatin1String("+++ b/"));
      }
      result.append(line);
    }
    patch = result.join(QLatin1Char('\n'));
  }

  LOG_PERF(0, "Exit");
  return patch;
}

qint64 getFileSizeFromCommit(const QString &path, const QString &filePath,
                             const QString &commit) {
  LOG_PERF(0, "Enter");

  svn_revnum_t revision = SVN_INVALID_REVNUM;
  if (!parseRevision(commit, &revision)) {
    LOG_PERF(0, "Exit");
    return fileSizeFromShelf(path, filePath, commit);
  }

  const QString wcPath = normalizedPath(path);
  FileSizeBaton baton;

  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        WorkingCopyInfo info;
        if (!reportError(
                SvnError::InfoFailed,
                readInfo(&info, wcPath, revisionUnspecified(), ctx, pool))) {
          return false;
        }

        const QString relativePath = QFileInfo(filePath).isAbsolute()
                                         ? relativeFilePath(wcPath, filePath)
                                         : QDir::fromNativeSeparators(filePath);
        const QByteArray fileUrl =
            urlOfRelativePath(info.url, relativePath).toUtf8();
        const svn_opt_revision_t peg = revisionNumber(revision);
        return reportError(SvnError::InfoFailed,
                           svn_client_list4(fileUrl.constData(), &peg, &peg,
                                            nullptr, svn_depth_empty,
                                            SVN_DIRENT_KIND | SVN_DIRENT_SIZE,
                                            FALSE, FALSE, fileSizeCallback,
                                            &baton, ctx, pool));
      },
      wcPath);

  LOG_PERF(0, "Exit");
  return success ? baton.size : -1;
}

BlameReport blame(const QString &path, const QStringList &files) {
  LOG_PERF(0, "Enter");

  const QString wcPath = normalizedPath(path);
  BlameReport report;

  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        for (const QString &file : files) {
          BlameFileInfo fileInfo;
          fileInfo.filePath = file;

          BlameBaton baton;
          baton.file = &fileInfo;

          const QByteArray pathBytes = absoluteFilePath(wcPath, file).toUtf8();
          const svn_opt_revision_t peg = revisionWorking();
          const svn_opt_revision_t start = revisionNumber(0);
          const svn_opt_revision_t end = revisionWorking();
          svn_revnum_t startRevision = SVN_INVALID_REVNUM;
          svn_revnum_t endRevision = SVN_INVALID_REVNUM;
          const svn_diff_file_options_t *diffOptions =
              svn_diff_file_options_create(pool);
          svn_error_t *error = svn_client_blame6(
              &startRevision, &endRevision, pathBytes.constData(), &peg, &start,
              &end, diffOptions, FALSE, FALSE, blameCallback, &baton, ctx,
              pool);
          if (error) {
            if (error->apr_err == SVN_ERR_CANCELLED) {
              return reportError(SvnError::BlameFailed, error);
            }

            fileInfo.lineInfos.clear();
            fileInfo.errorMessage = svnErrorMessage(error);
            svn_error_clear(error);
          }

          report.fileInfos.append(fileInfo);
        }

        return true;
      },
      wcPath);

  report.valid = success;

  LOG_PERF(0, "Exit");
  return report;
}

} // namespace SvnRepo
