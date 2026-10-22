#pragma once

#include <GitRepo.h>

#include <QHash>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

#include <tuple>

// Subversion counterpart of the GitRepo free functions. Every function takes
// the absolute path of a working copy where GitRepo takes a git_repository.
//
// Repositories are expected to use the standard layout: trunk, branches/<name>
// and tags/<name> directly below the repository root. "Commits" are revision
// numbers, given as "123" or "r123".
//
// Operations that only exist in Git (rebase, bisect, worktrees, branching
// workflows, LFS, ...) are declared for API parity. They fail, and
// getLastError() reports SvnError::UnsupportedOperation.
namespace SvnRepo {

enum class BranchType : int {
  Unknown = 0,

  All,
  Local,
  Remote,
  Tag,

  last
};

enum class ResetType : int {
  Unknown = 0,

  Soft,
  Mixed,
  Hard,

  last
};

enum class SvnError : int {
  Unknown = 0,
  InitializationFailed,
  CreateClientContextFailed,
  CheckoutFailed,
  UpdateFailed,
  StatusFailed,
  AddFailed,
  DeleteFailed,
  RevertFailed,
  CommitFailed,
  HistoryFailed,
  ErrorWhileOpening,
  UnsupportedOperation,
  SwitchFailed,
  CopyFailed,
  MoveFailed,
  MergeFailed,
  DiffFailed,
  BlameFailed,
  ShelveFailed,
  PropertyFailed,
  PatchFailed,
  RelocateFailed,
  InfoFailed,
  InvalidRevision,
  InvalidArgument,
  Cancelled,
  ResolveFailed,
  // Merge, cherry-pick and revert (docs/SVN_TEST_PLAN.md 3.11.1).
  MergeFailedWithConflicts,
  CherryPickFailedWithConflicts,
  RevertFailedWithConflicts,
  MergeIsEmpty,
  CherryPickIsEmpty,
  RevertIsEmpty,
  LocalChangesInTheWay,
  OperationInProgress,
  AbortMergeNothingToAbort,
  Last
};

using GitError = SvnError;

enum class BranchStatusType : int {
  Unknown = 0,
  UpToDate,
  Ahead,
  Behind,
  Diverged,
  NoUpstream,
  last
};

struct BranchStatus {
  BranchStatusType status = BranchStatusType::Unknown;
  size_t aheadCount = 0;
  size_t behindCount = 0;
};

struct BranchInfo {
  QString name;
  BranchStatus statusInfo;
};

enum class SubmoduleStatusType : int {
  Unknown = 0,
  UpToDate,
  Outdated,
  Error,
  last
};

struct SubmoduleStatus {
  SubmoduleStatusType status = SubmoduleStatusType::Unknown;
};

struct SubmoduleInfo {
  QString name;
  QString commit;
  SubmoduleStatus statusInfo;
};

struct RemoteTarget {
  QString name;
  QString fetchUrl;
  QString pushUrl;
};

struct RemoteInfo {
  QString name;
  QString fetchUrl;
  QString pushUrl;
  bool isDefault = false;
  QVector<RemoteTarget> targets;
};

struct FilesDiffLine {
  QChar origin;
  QString content;
};

struct FilesDiffHunk {
  int oldStart = -1;
  int newStart = -1;
  int oldLines = -1;
  int newLines = -1;
  QVector<FilesDiffLine> lines;
};

struct FilesDiffDelta {
  QString oldFilePath;
  QString newFilePath;
  QVector<FilesDiffHunk> hunks;
};

struct FilesDiff {
  bool valid = false;
  QString diff;
  QMap<QString, QString> filesContent;
  QMap<QString, QString> repoContent;
  QVector<FilesDiffDelta> deltas;
};

struct CommitStatus {
  QString commit;
  QString author;
  QString message;
  qint64 timeStamp = -1;
  // Files of the working copy changed by the revision, relative to its root.
  QStringList newFiles;
  QStringList modifiedFiles;
  QStringList deletedFiles;
};

struct SvnErrorContext {
  SvnError error = SvnError::Unknown;
  int errorCode = -1;
  QString errorMessage;
  // Counts the errors recorded on this thread, so a caller can tell whether
  // a call recorded one: a call that succeeds leaves the last error as it
  // was.
  quint64 serial = 0;
};

// Subversion has no index. The staging area is the changelist
// kStagingChangelist: stageFiles adds the selected files to it and only those
// files are committed by commitStaged. Changes outside it are unstaged.
struct SvnStatus {
  QString path;
  QStringList newFiles;
  QStringList modifiedFiles;
  QStringList deletedFiles;
  QStringList typeChangedFiles;
  QStringList renamedFiles;
  QStringList unreadableFiles;
  QStringList newStagedFiles;
  QStringList modifiedStagedFiles;
  QStringList deletedStagedFiles;
  QStringList typeChangedStagedFiles;
  QStringList renamedStagedFiles;
  QStringList ignoredFiles;
  QStringList conflictedFiles;
  QStringList untrackedFiles;

  bool hasChanges() const;

  bool operator==(const SvnStatus &other) const;
};

struct SvnCommitInfo {
  qint64 revision = -1;
  QString message;
  QString summary;
  QString authorName;
  QString authorEmail;
  qint64 timeStamp = -1;
  bool signedCommit = false;
  QString signatureType;
  QString signature;
  QVector<QString> branches;
  QVector<QString> parents;
  QVector<QString> children;

  bool operator==(const SvnCommitInfo &other) const {
    return revision == other.revision && message == other.message &&
           summary == other.summary && authorName == other.authorName &&
           authorEmail == other.authorEmail && timeStamp == other.timeStamp &&
           signedCommit == other.signedCommit &&
           signatureType == other.signatureType &&
           signature == other.signature && branches == other.branches &&
           parents == other.parents && children == other.children;
  }
};

struct SvnHistory {
  bool valid = false;
  // Revisions, newest first.
  QVector<QString> commits;
  QMap<QString, SvnCommitInfo> commitInfos;
  // True if there are older revisions after this page.
  bool hasMore = false;
};

struct BlameLineInfo {
  QString filePath;
  qint64 lineNumber = -1;
  QString content;
  // Revision, empty for locally modified lines.
  QString commit;
  QString author;
  QString email;
  QString summary;
};

struct BlameFileInfo {
  QString filePath;
  QVector<BlameLineInfo> lineInfos;
  QString errorMessage;
};

struct BlameReport {
  QVector<BlameFileInfo> fileInfos;
  bool valid = false;
};

SvnErrorContext getLastError();

// Subversion's text for one of its error codes (apr_status_t), empty for an
// unknown code.
QString errorCodeText(int errorCode);

void initForCurrentThread();
void shutdownForCurrentThread();

bool init();

// Credentials.
void finishPendingCredentialPrompts();
void clearCachedCredentials(const QString &repoPath);
QString getBasicAuthHeader(const QString &repoPath, const QString &url);
GitRepo::GitSshAuthInfo
getSshAuthInfo(const QString &repoPath, const QString &url,
               const QString &usernameFromUrl = QString());

// Repository.
QString openRepository(const QString &path);
void closeRepository(const QString &repo);
bool cloneRepository(const QString &url, const QString &destinationDir,
                     const QString &uuid = QString());
bool initRepository(const QString &repoName, const QString &workingDir,
                    const QString &defaultBranch, const QString &author,
                    const QString &email);

// Working copy.
SvnStatus getStatus(const QString &path, const bool &quitFlag);
bool hasConflicts(const QString &path);
bool stageFiles(const QString &path, const QStringList &files);
bool unstageFiles(const QString &path, const QStringList &files);
// Ignores files with svn:ignore on their parent directory. With recursive,
// svn:global-ignores on the parent directory ignores everything below it.
bool ignoreFiles(const QString &path, const QStringList &files, bool recursive);
bool discardFiles(const QString &path, const QStringList &files);
// Commits the files in the staging changelist and nothing else. While a
// merge, cherry-pick or revert is in progress, it fails as long as one of its
// conflicts is unresolved; otherwise it also commits the operation's
// svn:mergeinfo and ends the operation.
bool commitStaged(const QString &path, const QString &summary,
                  const QString &message, const QString &author,
                  const QString &email, bool cleanup, bool signoff,
                  bool signCommit);
bool applyPatch(const QString &path, const QString &patchContent);
bool checkoutCommit(const QString &path, const QString &commit);
QString listConflictsForAFile(const QString &path, const QString &filePath);
// True while a merge, cherry-pick or revert stopped with conflicts or
// couldn't commit: from then until commitStaged or abortMerge ends it, also
// across closing and reopening the repository. Conflicts from an update don't
// count.
bool getIsMergeInProgress(const QString &path);
// Ends the merge, cherry-pick or revert in progress: its paths go back to
// BASE and leave the staging changelist, and the files it added are removed.
// Other local changes and unversioned files are kept. Without an operation in
// progress it fails with AbortMergeNothingToAbort and changes nothing.
bool abortMerge(const QString &path);
bool isTreeConflict(const QString &path, const QString &filePath);
bool resolveTreeConflict(const QString &path, const QString &filePath,
                         bool keepLocal);

// Diffs and blame.
FilesDiff diffFiles(const QString &path, const QStringList &files);
FilesDiff diffFilesFromCommit(const QString &path, const QStringList &files,
                              const QString &commit);
QString createPatchFromCommit(const QString &path, const QString &commit);
qint64 getFileSizeFromCommit(const QString &path, const QString &filePath,
                             const QString &commit);
BlameReport blame(const QString &path, const QStringList &files,
                  const QString &commit);

// History.
SvnHistory getBranchHistory(const QString &path, const QString &branchName,
                            BranchType branchType, int page,
                            const bool &quitFlag);
SvnHistory getHistory(const QString &path, int limit = 100);
CommitStatus getCommitStatus(const QString &path, const QString &commitOid);
QStringList getCommitBranches(const QString &path, const QString &commitOid,
                              const bool &quitFlag);
QStringList getCommitTags(const QString &path, const QString &commitOid,
                          const bool &quitFlag);
QStringList getCommitHeads(const QString &path, const QString &commitOid,
                           const bool &quitFlag);
bool getCommitIsHead(const QString &path, const QString &commitOid,
                     const bool &quitFlag);

// Branches and tags.
QString getActiveBranch(const QString &path, BranchType type,
                        const bool &quitFlag);
QStringList listLocalBranches(const QString &path, const bool &quitFlag);
QStringList listRemoteBranches(const QString &path, const bool &quitFlag);
QVector<BranchInfo> listRemoteBranchesWithStatus(const QString &path,
                                                 const bool &quitFlag);
bool checkoutBranch(const QString &path, const QString &branchOrTagName,
                    BranchType type);
bool createBranchFromCommit(const QString &path, const QString &commit,
                            const QString &branchName);
bool deleteBranch(const QString &path, const QString &branchName);
bool deleteRemoteBranch(const QString &path, const QString &branchName);
bool renameBranch(const QString &path, const QString &oldBranchName,
                  const QString &newBranchName);
QStringList listTags(const QString &path);
QStringList listRemoteTags(const QString &path);
bool createTagFromCommit(const QString &path, const QString &commit,
                         const QString &tagName, const QString &author,
                         const QString &email, const QString &message);
bool deleteTag(const QString &path, const QString &tagName);
bool deleteRemoteTag(const QString &path, const QString &tagName);
bool renameTag(const QString &path, const QString &oldTagName,
               const QString &newTagName);
bool pushTag(const QString &path, const QString &tagName);
bool pushAllTags(const QString &path);

// Merging. These work like the Git backend (docs/SVN_TEST_PLAN.md 3.11.1):
// - a clean result is committed as one revision holding only the
//   operation's changes; the user's own local changes are neither committed
//   nor touched;
// - a local change in a path the operation changes refuses it before
//   anything is touched (LocalChangesInTheWay), as does another operation in
//   progress (OperationInProgress);
// - conflicts return false with ...FailedWithConflicts and leave the
//   operation in progress, its other changes staged, for commitStaged or
//   abortMerge;
// - nothing to do returns false with ...IsEmpty and makes no revision.
// A merge into another branch than the checked out one switches to it first,
// taking the local changes along like git checkout.
bool merge(const QString &path, const QString &sourceBranch,
           const QString &destinationBranch, const QString &author,
           const QString &email);
bool cherryPickCommit(const QString &path, const QString &commit,
                      const QString &author, const QString &email);
bool revertCommit(const QString &path, const QString &commit,
                  const QString &author, const QString &email,
                  const QString &message);

// Synchronization.
bool pull(const QString &path, const QString &activeBranchName, bool rebase);
bool push(const QString &path, const QString &activeBranchName, bool force);
bool fetchAll(const QString &path);
bool fetchRemote(const QString &path, const QString &remoteName);

// Remotes. A working copy has a single remote, "origin", the repository root.
QVector<RemoteInfo> listRemotes(const QString &path);
QString getDefaultRemote(const QString &path);
bool addRemote(const QString &path, const QString &remoteName,
               const QString &fetchUrl, const QString &pushUrl);
bool updateRemote(const QString &path, const QString &remoteName,
                  const QString &newRemoteName, const QString &fetchUrl,
                  const QString &pushUrl);
bool removeRemote(const QString &path, const QString &remoteName);
bool setDefaultRemote(const QString &path, const QString &remoteName);
bool addTarget(const QString &path, const QString &remoteName,
               const QString &targetName, const QString &fetchUrl,
               const QString &pushUrl);
bool editTarget(const QString &path, const QString &remoteName,
                const QString &targetName, const QString &newFetchUrl,
                const QString &newPushUrl);
bool deleteTarget(const QString &path, const QString &remoteName,
                  const QString &targetName);
bool setDefaultTarget(const QString &path, const QString &remoteName,
                      const QString &targetName);
std::tuple<bool, QString, QString> getUpstream(const QString &path,
                                               const QString &localBranchName);
bool setUpstream(const QString &path, const QString &remoteName,
                 const QString &remoteBranchName,
                 const QString &localBranchName);

// Submodules, backed by svn:externals definitions.
QStringList listSubmodules(const QString &path);
QVector<SubmoduleInfo> listSubmodulesWithStatus(const QString &path,
                                                const bool &quitFlag);
QString getSubmodulePath(const QString &path, const QString &submoduleName);
bool addSubmodule(const QString &path, const QString &url, const QString &name,
                  const QString &branch, const QString &uuid);
bool deleteSubmodule(const QString &path, const QString &submoduleName,
                     const QString &submodulePath);
bool updateSubmodule(const QString &path, const QString &submoduleName);

// Stashes, backed by Subversion shelves. Index 0 is the newest stash.
bool stash(const QString &path, const QString &note);
QStringList listStashes(const QString &path, const bool &quitFlag);
bool applyStash(const QString &path, int index);
bool deleteStash(const QString &path, int index);
bool deleteAllStashes(const QString &path);
CommitStatus getStashStatus(const QString &path, int stashIndex);
QString listConflictsForStashFile(const QString &path, int index,
                                  const QString &filePath);

// Git only operations.
bool amend(const QString &path, const QString &summary, const QString &message,
           const QString &author, const QString &email, bool signoff,
           bool signCommit);
bool resetToCommit(const QString &path, const QString &commit, ResetType type);
GitRepo::Lfs::StorageStatus getLfsStorageStatus(const QString &path);
bool rebaseBranches(const QString &path, const QString &sourceBranch,
                    const QString &destinationBranch, const QString &author,
                    const QString &email, const bool signCommit);
bool squashBranches(const QString &path, const QString &sourceBranch,
                    const QString &destinationBranch, const QString &author,
                    const QString &email, const bool signCommit);
bool getIsRebaseInProgress(const QString &path);
bool abortRebase(const QString &path);
bool commitRebase(const QString &path, const QString &summary,
                  const QString &message, const QString &author,
                  const QString &email, bool cleanup, bool signoff,
                  bool signCommit);
bool bisect(const QString &path, GitRepo::BisectAction action,
            const QString &commit, GitRepo::BisectStatus &status);
QHash<QString, int> getBisectMarks(const QString &path);
QVector<GitRepo::WorktreeInfo> listWorktrees(const QString &path);
bool addWorktree(const QString &path, const QString &name,
                 const QString &worktreePath, const QString &branch,
                 bool createBranch, const QString &baseBranch = QString());
bool removeWorktree(const QString &path, const QString &name, bool force);
bool lockWorktree(const QString &path, const QString &name,
                  const QString &reason);
bool unlockWorktree(const QString &path, const QString &name);
bool startGitLabFlowFeature(const QString &path, const QString &branchName,
                            const QString &mainBranchName);
bool finishGitLabFlowFeature(const QString &path, const QString &branchName,
                             const QString &mainBranchName,
                             const QString &author, const QString &email);
bool promoteGitLabFlowEnvironment(const QString &path,
                                  const QStringList &environmentChain,
                                  const QString &sourceBranchName,
                                  const QString &author, const QString &email);
bool startGitLabFlowRelease(const QString &path,
                            const QString &releaseBranchName,
                            const QString &mainBranchName);
bool cherryPickToGitLabFlowRelease(const QString &path, const QString &commit,
                                   const QString &releaseBranchName,
                                   const QString &author, const QString &email,
                                   bool *conflicted = nullptr);
bool startGitHubFlowBranch(const QString &path, const QString &branchName,
                           const QString &mainBranchName);
bool updateGitHubFlowBranch(const QString &path, const QString &branchName,
                            const QString &mainBranchName,
                            const QString &author, const QString &email);
bool finishGitHubFlowBranch(const QString &path, const QString &branchName,
                            const QString &mainBranchName,
                            const QString &author, const QString &email);
bool readGitFlowNextConfig(const QString &path,
                           GitRepo::GitFlowNextConfig *config);
bool initGitFlowNext(const QString &path, const QString &mainBranchName,
                     const QString &developBranchName);
bool startGitFlowNextBranch(const QString &path, const QString &branchType,
                            const QString &branchName);
bool finishGitFlowNextBranch(const QString &path, const QString &branchName,
                             const QString &author, const QString &email);
bool updateGitFlowNextBranch(const QString &path, const QString &branchName,
                             const QString &author, const QString &email);
GitRepo::WorkflowType getWorkflowType(const QString &path);
bool setWorkflowType(const QString &path, GitRepo::WorkflowType type);
bool applyGitFlowNextWorkflow(const QString &path,
                              const GitRepo::GitFlowNextWorkflow &workflow);
bool getGitFlowNextWorkflowReference(const QString &path, QString *id,
                                     int *revision);

#ifdef SOURCERER_SVNREPO_TESTING
// Hooks for the tests in tests/svnrepo. Only built when SvnRepo is compiled
// with SOURCERER_SVNREPO_TESTING.
namespace Testing {

// Makes every client context read its configuration (config, servers) and
// cached credentials from path instead of the user's Subversion configuration
// area. An empty path restores the default.
void setConfigDirectory(const QString &path);
QString configDirectory();

// The server certificate a TLS connection presents, as Subversion reports it
// to the trust prompt.
struct ServerCertificateInfo {
  QString hostname;
  QString fingerprint;
  QString validFrom;
  QString validUntil;
  QString issuerDistinguishedName;
  QString asciiCertificate;
};

// Runs the server trust prompt for a certificate with failures (a mask of
// SVN_AUTH_SSL_* bits). Returns true when the certificate is accepted;
// acceptedFailures receives the failures the answer accepts.
bool sslServerTrustCheck(quint32 failures, const ServerCertificateInfo &info,
                         const QString &realm = QString(),
                         quint32 *acceptedFailures = nullptr);

// Makes libsvn malfunctions (SVN_ERR_MALFUNCTION, SVN_ERR_ASSERT) return an
// error and record the failed assertion, instead of aborting the process.
// Malfunctions that can't return still abort, after they are recorded.
void installMalfunctionHandler();

// The malfunctions recorded since the last call, as "file:line: expression".
QStringList takeMalfunctions();

// Clears this thread's last error. Successful calls leave it as it was.
void resetLastError();

// Takes the working copy's write lock and leaves it behind, as an interrupted
// operation does. Only `svn cleanup` removes it.
bool leaveStaleWorkingCopyLock(const QString &path);

} // namespace Testing
#endif

} // namespace SvnRepo

bool svn_init();
