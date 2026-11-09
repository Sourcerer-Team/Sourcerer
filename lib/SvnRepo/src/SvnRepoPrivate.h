#pragma once

// Helpers shared by the SvnRepo translation units. Not part of the public API.

#include "SvnRepo.h"

#include <svn_client.h>
#include <svn_error.h>
#include <svn_opt.h>
#include <svn_types.h>
#include <svn_wc.h>

#include <QByteArray>
#include <QSet>
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

// Whether error says that a repository path doesn't exist, as opposed to a
// network, authentication or other failure.
bool isMissingPathError(svn_error_t *error);

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

// The canonical form of a repository URL, or of a local path to a
// repository, as libsvn requires it (Debug builds assert on any other); empty
// when it is neither.
QString canonicalUrl(const QString &urlOrPath);
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

// Name of the branch, tag or trunk containing url. Empty outside trunk,
// branches and tags (the layout root, a repository without trunk).
QString branchNameFromUrl(const QString &url, const QString &reposRootUrl,
                          BranchType type);

// The part of a working copy's URL below its trunk, branch or tag, with a
// leading slash ("/src" for a working copy of trunk/src), or empty.
QString branchSubpath(const WorkingCopyInfo &info);

// Highest revision in the working copy at wcPath: of its nodes' BASE, or of
// their last changes with committed. Own commits leave the root at an older
// revision, so the root's revision isn't it.
svn_error_t *readMaxRevision(svn_revnum_t *revision, const QString &wcPath,
                             bool committed, svn_client_ctx_t *ctx,
                             apr_pool_t *pool);

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

// Changelist that holds the staged files. Subversion has no index, so the
// selection made by the user is kept as a changelist in the working copy.
inline constexpr const char *kStagingChangelist = "sourcerer-staged";

// Changelists only hold files, so staged folders (a property change, an
// empty or a copied folder) are kept in a file in the administrative area of
// the working copy at path. Paths are absolute, as normalizedPath returns
// them.
QSet<QString> readStagedDirectories(const QString &path);
void writeStagedDirectories(const QString &path,
                            const QSet<QString> &directories);

// A merge, cherry-pick or revert that stopped with conflicts, or whose commit
// failed (docs/SVN_TEST_PLAN.md 3.11.1). It is kept in a file in the
// administrative area, so it survives closing the repository. Paths are
// absolute; the working copy root stands for its svn:mergeinfo.
struct OperationState {
  QString kind; // "merge", "cherry-pick" or "revert"
  QString source;
  QString sourceUrl;
  qint64 start = -1;
  qint64 end = -1;
  QString destination;
  QString message;
  QStringList paths;
  QStringList added;
  QStringList conflicted;
};

// Reads the operation of the working copy at path; false without one.
bool readOperationState(const QString &path, OperationState *state);
bool writeOperationState(const QString &path, const OperationState &state);
void removeOperationState(const QString &path);

// The operation in progress, if any. A state whose paths have neither local
// changes nor conflicts any more (the user committed or reverted with another
// client) is removed, and nothing is in progress.
bool activeOperation(const QString &wcPath, OperationState *state = nullptr);

// Records OperationInProgress and returns true while an operation is in
// progress.
bool refuseWhileOperationInProgress(const QString &wcPath);

// Collects the paths an update, switch or merge leaves conflicted, while it
// is alive.
class ConflictWatch {
public:
  explicit ConflictWatch(svn_client_ctx_t *ctx);
  ~ConflictWatch();

  ConflictWatch(const ConflictWatch &) = delete;
  ConflictWatch &operator=(const ConflictWatch &) = delete;

  // Records kind with a message naming the conflicted paths, relative to
  // wcPath, and returns false; true when nothing conflicted.
  bool reportConflicts(SvnError kind, const QString &wcPath,
                       const QString &operation) const;

private:
  static void notify(void *baton, const svn_wc_notify_t *notify,
                     apr_pool_t *pool);

  svn_client_ctx_t *_ctx;
  svn_wc_notify_func2_t _previousFunc;
  void *_previousBaton;
  QStringList _conflicted;
};

// Commits the local modifications below the working copy root.
svn_error_t *commitWorkingCopy(const QString &wcPath, const QString &message,
                               svn_client_ctx_t *ctx, apr_pool_t *pool);

// Commits the local modifications of paths, without their children.
svn_error_t *commitPaths(const QStringList &paths, const QString &message,
                         svn_client_ctx_t *ctx, apr_pool_t *pool);

// Status of the working copy, paths relative to its root.
SvnStatus readStatus(const QString &wcPath, const bool *quitFlag);

// Reads a UTF-8 file, at most maxSize bytes when maxSize > 0.
QString readTextFile(const QString &filePath);

// Content of a file at a revision; with expandKeywords as the working copy
// shows it (svn:keywords expanded).
svn_error_t *readFileAtRevision(QByteArray *content, const QString &pathOrUrl,
                                const svn_opt_revision_t &peg,
                                const svn_opt_revision_t &revision,
                                svn_client_ctx_t *ctx, apr_pool_t *pool,
                                bool expandKeywords = false);

// Parses unified diff text, as written by svn_client_diff7.
QVector<FilesDiffDelta> parseUnifiedDiff(const QString &diff);

} // namespace SvnRepo::Private
