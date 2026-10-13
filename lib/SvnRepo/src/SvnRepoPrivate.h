#pragma once

// Helpers shared by the SvnRepo translation units. Not part of the public API.

#include "SvnRepo.h"

#include <svn_client.h>
#include <svn_error.h>
#include <svn_opt.h>
#include <svn_types.h>

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

namespace SvnRepo::Private {

void setLastError(SvnError error, int errorCode, const QString &errorMessage);

// Records an unsupported operation and returns false.
bool setUnsupported(const char *operation);

QString svnErrorMessage(const svn_error_t *error);

// Records and clears error. Returns false so callers can write
// "return reportError(SvnError::X, error);".
bool reportError(SvnError kind, svn_error_t *error);

// Runs fn with a client context and pool that live for the duration of the
// call. path, when given, is the working copy whose stored credentials are
// used when the server asks for them; otherwise the repository opened on this
// thread is used. Credential prompts raised by fn are closed after it returns.
bool runWithClientContext(
    const std::function<bool(svn_client_ctx_t *, apr_pool_t *)> &fn,
    const QString &path = QString());

// Makes long running operations of this thread stop when *quitFlag becomes
// true while the scope is alive.
class QuitScope {
public:
  explicit QuitScope(const bool *quitFlag);
  ~QuitScope();

  QuitScope(const QuitScope &) = delete;
  QuitScope &operator=(const QuitScope &) = delete;

private:
  const bool *_previous;
};

// Absolute path with forward slashes and without a trailing slash.
QString normalizedPath(const QString &path);

// Absolute path of file, which is relative to the working copy at base or
// absolute.
QString absoluteFilePath(const QString &base, const QString &file);

// Path of absolutePath relative to base, with forward slashes.
QString relativeFilePath(const QString &base, const QString &absolutePath);

// Array of const char * whose strings live in storage.
struct PathArray {
  apr_array_header_t *array = nullptr;
  QVector<QByteArray> storage;
};

PathArray makePathArray(apr_pool_t *pool, const QStringList &paths);

svn_opt_revision_t revisionUnspecified();
svn_opt_revision_t revisionHead();
svn_opt_revision_t revisionBase();
svn_opt_revision_t revisionWorking();
svn_opt_revision_t revisionNumber(svn_revnum_t number);

// Parses "123" or "r123". Records SvnError::InvalidRevision on failure.
bool parseRevision(const QString &commit, svn_revnum_t *revision);

struct WorkingCopyInfo {
  QString url;
  QString reposRootUrl;
  // Directory holding the trunk/branches/tags layout. It is the repository
  // root for a standard repository, but a project directory inside a larger
  // repository (e.g. .../asf/subversion) is the layout root for its branches.
  QString layoutRootUrl;
  QString reposUuid;
  QString wcRoot;
  svn_revnum_t revision = SVN_INVALID_REVNUM;
  svn_revnum_t lastChangedRevision = SVN_INVALID_REVNUM;
  QString lastChangedAuthor;
};

// Reads the svn info of a working copy path or URL.
svn_error_t *readInfo(WorkingCopyInfo *info, const QString &pathOrUrl,
                      const svn_opt_revision_t &revision, svn_client_ctx_t *ctx,
                      apr_pool_t *pool);
bool readInfo(WorkingCopyInfo *info, const QString &path);

QString trimmedUrl(QString url);
// Directory of url that holds the trunk/branches/tags layout, see
// WorkingCopyInfo::layoutRootUrl.
QString layoutRootUrlFor(const QString &url, const QString &reposRootUrl);
// URL of relativePath below rootUrl. The path is percent-encoded, as
// Subversion requires, so names with spaces become e.g. "New%20Bitmap.bmp".
QString urlOfRelativePath(const QString &rootUrl, const QString &relativePath);

// URL of a branch, tag or trunk below the repository root. "trunk" names
// the trunk for every branch type but Tag.
QString branchUrl(const QString &reposRootUrl, const QString &name,
                  BranchType type);

// Name of the branch, tag or trunk containing url.
QString branchNameFromUrl(const QString &url, const QString &reposRootUrl,
                          BranchType type);

// Names of the directories directly below url at HEAD.
svn_error_t *listDirectories(QStringList *names, const QString &url,
                             svn_client_ctx_t *ctx, apr_pool_t *pool);

// Log message for a commit, built like the Git commit message.
QString makeLogMessage(const QString &summary, const QString &message,
                       bool cleanup, bool signoff, const QString &author,
                       const QString &email);

// Installs message as the log message of the commits made with ctx while
// the scope is alive.
class LogMessageScope {
public:
  LogMessageScope(svn_client_ctx_t *ctx, const QString &message);
  ~LogMessageScope();

  LogMessageScope(const LogMessageScope &) = delete;
  LogMessageScope &operator=(const LogMessageScope &) = delete;

private:
  svn_client_ctx_t *_ctx;
  QByteArray _message;
};

// Commits the local modifications below the working copy root.
svn_error_t *commitWorkingCopy(const QString &wcPath, const QString &message,
                               svn_client_ctx_t *ctx, apr_pool_t *pool);

// Status of the working copy, paths relative to its root.
SvnStatus readStatus(const QString &wcPath, const bool *quitFlag);

// Reads a UTF-8 file, at most maxSize bytes when maxSize > 0.
QString readTextFile(const QString &filePath);

// Content of a file at a revision.
svn_error_t *readFileAtRevision(QByteArray *content, const QString &pathOrUrl,
                                const svn_opt_revision_t &peg,
                                const svn_opt_revision_t &revision,
                                svn_client_ctx_t *ctx, apr_pool_t *pool);

// Parses unified diff text, as written by svn_client_diff7.
QVector<FilesDiffDelta> parseUnifiedDiff(const QString &diff);

} // namespace SvnRepo::Private
