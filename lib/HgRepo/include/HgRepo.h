// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include <GitRepo.h>

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>
#include <tuple>

// Mercurial counterpart of the GitRepo free functions. Mercurial is not linked
// in: every Repository owns an "hg serve --cmdserver pipe" process of the hg
// installed by the user, and the functions below run hg commands through it.
//
// The functions take a Repository where GitRepo takes a git_repository and
// return the GitRepo types. Commits are full 40 character node ids.
//
// A Repository must only be used on the thread that opened it.
//
// Mercurial has no index. The staging area is a list of paths kept by
// Sourcerer in .hg/sourcerer/staged: stageFiles adds files to it and
// commitStaged commits only those files.
namespace HgRepo {

// Oldest supported Mercurial release.
inline constexpr int kMinimumMajorVersion = 5;
inline constexpr int kMinimumMinorVersion = 0;

enum class HgError : int {
  Unknown = 0,
  MercurialNotFound,
  MercurialTooOld,
  ServerStartFailed,
  ServerHandshakeFailed,
  ServerDied,
  CommandFailed,
  Cancelled,
  InvalidArgument,
  NotARepository,
  InitFailed,
  StatusFailed,
  StagingFailed,
  CommitFailed,
  NothingToCommit,
  UnsupportedOperation,
  DiscardFailed,
  ResolveFailed,
  HistoryFailed,
  CommitStatusFailed,
  DiffFailed,
  BlameFailed,
  FileSizeFailed,
  BranchFailed,
  CheckoutFailed,
  TagFailed,
  MergeFailed,
  MergeConflicts,
  RebaseFailed,
  GraftFailed,
  BackoutFailed,
  StripFailed,
  ShelveFailed,
  BisectFailed,
  BisectNotInProgress,
  RemoteFailed,
  PullFailed,
  PushFailed,
  UncommittedChanges,
  SubrepoFailed,
  WorktreeFailed,
  LfsFailed,
  LfsNotEnabled,
  WorkflowFailed,
  WorkflowNotInitialized,
  CloneFailed,
  PatchFailed,
  // The remote rejected the credentials, or the user cancelled the prompt.
  AuthenticationFailed,
  Last
};

enum class HgBranchKind : int { NamedBranch = 0, Bookmark };

enum class HgSubrepoKind : int { Mercurial = 0, Git, Subversion };

struct HgSubrepo {
  // Relative to the repository root, forward slashes.
  QString path;
  // As written in .hgsub, without the "[git]" or "[svn]" prefix.
  QString source;
  HgSubrepoKind kind = HgSubrepoKind::Mercurial;
  // Node, Git commit or Subversion revision recorded in .hgsubstate, empty
  // before the first commit with the subrepo.
  QString pinnedState;
  // What is checked out in the subrepo, empty when unknown. Mercurial and Git
  // subrepos are read from their files; Subversion ones are left empty for
  // the caller to fill with SvnRepo.
  QString currentState;
};

struct HgBranchInfo {
  // Bare name, without the "@" bookmarks get in branch lists.
  QString name;
  HgBranchKind kind = HgBranchKind::NamedBranch;
  // The working directory is on this branch, or this bookmark is active.
  bool active = false;
  // Named branch closed with "commit --close-branch".
  bool closed = false;
  // Set with "hg branch" but without commits yet.
  bool isNew = false;
  QString node;
};

struct HgErrorContext {
  HgError error = HgError::Unknown;
  // Exit code of the hg command, -1 if no command ran.
  int errorCode = -1;
  QString errorMessage;
};

class Repository;

// Error of the last failed call on the current thread.
HgErrorContext getLastError();

void initForCurrentThread();
void shutdownForCurrentThread();

// Commands of the current thread stop while *flag is true, also in functions
// without a quit flag of their own. nullptr clears it.
void setCancelFlag(const bool *flag);

// HTTP authentication. hg runs without prompts; when a remote asks for
// authorization the command is run again with credentials from the provider
// of the thread, passed to hg's [auth] section for that command only.
struct HgAuthRequest {
  // Remote that asked for authorization, without user info.
  QString url;
  // Repository root, or the destination of a clone.
  QString repoPath;
  // 0 for the first request of a command, counts the rejected credentials.
  int attempt = 0;
};

struct HgCredentials {
  QString username;
  QString password;
};

enum class HgAuthResult {
  // The remote accepted the credentials.
  Accepted,
  // The remote rejected them, credentials() is asked again.
  Rejected,
  // The remote rejected them and the command gives up.
  GaveUp,
};

// SSH key chosen for a host, see HgCredentialsProvider::sshKeys.
struct HgSshKey {
  QString host;
  QString privateKeyPath;
};

class HgCredentialsProvider {
public:
  virtual ~HgCredentialsProvider() = default;
  // Credentials for request.url. false cancels the command.
  virtual bool credentials(const HgAuthRequest &request,
                           HgCredentials *credentials) = 0;
  // What the remote said about the credentials of the last credentials() call.
  virtual void finished(const HgAuthRequest &request, HgAuthResult result) = 0;
  // Keys ssh uses for these hosts, through an ssh config file that includes
  // the user's own. Keys with a passphrase must be loaded in ssh-agent or
  // Pageant: ssh runs without prompts. Not used when the user's ui.ssh is
  // PuTTY's plink, which reads other key files.
  virtual QVector<HgSshKey> sshKeys() { return {}; }
};

// Provider asked by the commands of the current thread, nullptr when the
// commands fail without asking. The provider must outlive its use.
void setCredentialsProvider(HgCredentialsProvider *provider);

// SSH_ASKPASS program ssh runs when it needs a key passphrase, and the local
// socket and token it reaches Sourcerer with (src/askpass.cpp). Applies to
// hg processes started afterwards; an empty program turns it off, and ssh
// then runs in batch mode.
void setAskPass(const QString &program, const QString &socketName,
                const QString &token);

// hg executable used for new repositories. Empty selects the one found by
// findExecutable().
void setExecutablePath(const QString &path);

// Executable set with setExecutablePath, else hg from PATH, else the default
// Mercurial and TortoiseHg install locations. Empty if none is found.
QString findExecutable();

// Version of the hg executable, e.g. "7.1.2". Empty and sets the error when
// hg can't be run.
QString version();

// True if hg is found and is at least kMinimumMajorVersion.
// kMinimumMinorVersion. Sets MercurialNotFound or MercurialTooOld otherwise.
bool checkExecutable();

// Repository.
// Starts the command server for the repository at path. author and email are
// used for commits when no other author is given. Returns nullptr and sets the
// error on failure.
Repository *openRepository(const QString &path, const QString &author,
                           const QString &email);
// Stops the command server and deletes repo.
void closeRepository(Repository *repo);
// Absolute root of the working directory, forward slashes.
QString repositoryPath(const Repository *repo);
// Creates workingDir/repoName. Mercurial's first branch is always "default",
// so defaultBranch is ignored. When author is given, it is written to the
// repository's ui.username.
bool initRepository(const QString &repoName, const QString &workingDir,
                    const QString &defaultBranch, const QString &author,
                    const QString &email);
// Clones url into destinationDir. progress, when set, is called on this
// thread with the progress hg reports; uuid is copied into it.
bool cloneRepository(
    const QString &url, const QString &destinationDir, const QString &uuid,
    const std::function<void(const GitRepo::CloneProgress &)> &progress = {});

// Working directory.
GitRepo::GitStatus getStatus(Repository *repo, const bool &quitFlag);
bool hasConflicts(Repository *repo);
// Unknown files are added and missing files are marked removed before they
// are staged. A directory stages every changed file below it.
bool stageFiles(Repository *repo, const QStringList &files);
// Removes files from the staging area. The working directory is unchanged.
bool unstageFiles(Repository *repo, const QStringList &files);
// Reverts files to the working directory parent. Added files are forgotten
// and, like unknown files, stay on disk, as in GitRepo.
bool discardFiles(Repository *repo, const QStringList &files);
// Commits the staged files, or every change while a merge is in progress.
// signCommit is not supported.
bool commitStaged(Repository *repo, const QString &summary,
                  const QString &message, const QString &author,
                  const QString &email, bool cleanup, bool signoff,
                  bool signCommit);
// Replaces the working directory parent with a commit holding its changes,
// the staged files and the new message.
bool amend(Repository *repo, const QString &summary, const QString &message,
           const QString &author, const QString &email, bool signoff,
           bool signCommit);
// Marks conflicted files as resolved.
bool markResolved(Repository *repo, const QStringList &files);
bool getIsMergeInProgress(Repository *repo);

// History.
// branchName is ALL_BRANCHES for every commit, a named branch, "@name" for a
// bookmark or a tag name with BranchType::Tag. An empty name or "HEAD" selects
// the ancestors of the working directory parent.
GitRepo::GitHistory getBranchHistory(Repository *repo,
                                     const QString &branchName,
                                     GitRepo::BranchType type, int page,
                                     const bool &quitFlag);
GitRepo::GitCommitStatus getCommitStatus(Repository *repo,
                                         const QString &commitOid);

// Diffs and blame.
// Diff of the working directory against its parent.
GitRepo::FilesDiff diffFiles(Repository *repo, const QStringList &files);
// Diff of commit against its first parent.
GitRepo::FilesDiff diffFilesFromCommit(Repository *repo,
                                       const QStringList &files,
                                       const QString &commit);
// Blame of the working directory files. Uncommitted lines have an all zeros
// commit, as in GitRepo.
GitRepo::BlameReport blame(Repository *repo, const QStringList &files);
// Size of filePath at commit, the working directory parent if commit is
// empty. -1 on failure.
qint64 getFileSizeFromCommit(Repository *repo, const QString &filePath,
                             const QString &commit);
// "hg export" of commit.
QString createPatchFromCommit(Repository *repo, const QString &commit);
// Applies a patch to the working directory without committing it.
bool applyPatch(Repository *repo, const QString &patchContent);

// Branches and bookmarks.
//
// Branch lists hold named branches by name and bookmarks as "@name", so a
// branch and a bookmark of the same name stay apart. Remote branches are
// "path/name" and remote bookmarks "path/@name", as recorded by the
// remotenames extension at every pull and push.

// Local: the active bookmark as "@name", else the named branch of the working
// directory. Tag: a tag of the working directory parent. Remote: empty.
QString getActiveBranch(Repository *repo, GitRepo::BranchType type,
                        const bool &quitFlag);
// Open named branches (the new branch of the working directory included),
// then bookmarks.
QVector<HgBranchInfo> listLocalBranchesWithKind(Repository *repo,
                                                bool includeClosed,
                                                const bool &quitFlag);
// listLocalBranchesWithKind as names: "name" and "@name".
QStringList listLocalBranches(Repository *repo, const bool &quitFlag);
// Updates the working directory to the branch tip, the bookmark (activating
// it), the tag, or the commit of a remote branch or bookmark. A remote
// bookmark creates the local bookmark when it is missing and makes the remote
// its upstream. Uncommitted changes are kept unless they conflict.
bool checkoutBranch(Repository *repo, const QString &branchOrTagName,
                    GitRepo::BranchType type);
bool checkoutCommit(Repository *repo, const QString &commit);
// Creates the bookmark branchName at commit without updating to it.
bool createBranchFromCommit(Repository *repo, const QString &commit,
                            const QString &branchName);
// Updates to commit and sets the named branch, which is recorded by the next
// commit.
bool createNamedBranchFromCommit(Repository *repo, const QString &commit,
                                 const QString &branchName);
// Commits "--close-branch" on the branch tip and returns to the previous
// working directory parent. The working directory must be clean.
bool closeNamedBranch(Repository *repo, const QString &branchName,
                      const QString &author, const QString &email);
// Bookmarks are renamed. A named branch can only be renamed while it is new.
bool renameBranch(Repository *repo, const QString &oldName,
                  const QString &newName);
// Deletes a bookmark, or closes a named branch.
bool deleteBranch(Repository *repo, const QString &branchName);
// Branches and bookmarks whose tip is the commit or a descendant of it.
QStringList getCommitBranches(Repository *repo, const QString &commitOid,
                              const bool &quitFlag);
QStringList getCommitTags(Repository *repo, const QString &commitOid,
                          const bool &quitFlag);
// Branches and bookmarks whose tip is the commit.
QStringList getCommitHeads(Repository *repo, const QString &commitOid,
                           const bool &quitFlag);
bool getCommitIsHead(Repository *repo, const QString &commitOid,
                     const bool &quitFlag);

// Tags. Global tags are commits to .hgtags; local tags are kept in
// .hg/localtags and never pushed.
QStringList listTags(Repository *repo);
bool isLocalTag(Repository *repo, const QString &tagName);
// The local tags of listTags.
QStringList listLocalTags(Repository *repo);
bool createTagFromCommit(Repository *repo, const QString &commit,
                         const QString &tagName, const QString &author,
                         const QString &email, const QString &message,
                         bool local = false);
bool deleteTag(Repository *repo, const QString &tagName);
bool renameTag(Repository *repo, const QString &oldName,
               const QString &newName);
// Pushes the commit recording the tag to the default remote.
bool pushTag(Repository *repo, const QString &tagName);
bool pushAllTags(Repository *repo);
// Removes the tag with a commit and pushes it.
bool deleteRemoteTag(Repository *repo, const QString &tagName);

// Merging and history editing.
// Updates to destinationBranch and merges sourceBranch into it. A merge
// without conflicts is committed, a fast-forward of a bookmark or within a
// named branch moves the working directory. On conflicts the merge is left in
// progress, it returns false and the error is MergeConflicts.
bool merge(Repository *repo, const QString &sourceBranch,
           const QString &destinationBranch, const QString &author,
           const QString &email);
// Aborts a merge, graft, backout or unshelve in progress.
bool abortMerge(Repository *repo);
// Rebases sourceBranch onto destinationBranch and updates to it. On conflicts
// the rebase is left in progress and it returns false.
bool rebaseBranches(Repository *repo, const QString &sourceBranch,
                    const QString &destinationBranch, const QString &author,
                    const QString &email, bool signCommit);
bool getIsRebaseInProgress(Repository *repo);
bool abortRebase(Repository *repo);
// Marks the staged conflicts resolved and continues the rebase. Mercurial
// keeps the original messages, so summary and message are not used.
bool commitRebase(Repository *repo, const QString &summary,
                  const QString &message, const QString &author,
                  const QString &email, bool cleanup, bool signoff,
                  bool signCommit);
// Adds a single commit to destinationBranch with the changes of sourceBranch
// that are not in it, and updates to destinationBranch. sourceBranch is kept.
bool squashBranches(Repository *repo, const QString &sourceBranch,
                    const QString &destinationBranch, const QString &author,
                    const QString &email, bool signCommit);
// Grafts commit onto the working directory parent. On conflicts it returns
// true without a commit; commitStaged finishes the graft.
bool cherryPickCommit(Repository *repo, const QString &commit,
                      const QString &author, const QString &email);
// Commits the backout of commit on the working directory parent. On
// conflicts it returns true without a commit.
bool revertCommit(Repository *repo, const QString &commit,
                  const QString &author, const QString &email,
                  const QString &message);
// Removes commit and its descendants. With keepWorkingCopy the working
// directory keeps their changes. With backup a bundle is saved in
// .hg/strip-backup.
bool strip(Repository *repo, const QString &commit, bool keepWorkingCopy,
           bool backup);
// Strips the descendants of commit on the line of the working directory, so
// the working directory parent becomes commit. Soft and Mixed keep the
// changes in the working directory.
bool resetToCommit(Repository *repo, const QString &commit,
                   GitRepo::ResetType type);

// Shelves, Mercurial's stashes. Index 0 is the newest shelve.
// Shelves every change, unknown files included.
bool stash(Repository *repo, const QString &note);
// Shelve messages, newest first.
QStringList listStashes(Repository *repo, const bool &quitFlag);
// Shelve names, in the order of listStashes.
QStringList listStashNames(Repository *repo);
// Restores the shelve and keeps it. On conflicts it returns false and the
// unshelve is left in progress; staging the resolved files finishes it.
bool applyStash(Repository *repo, int index);
bool deleteStash(Repository *repo, int index);
bool deleteAllStashes(Repository *repo);
GitRepo::GitCommitStatus getStashStatus(Repository *repo, int index);
// Content of the conflicted working directory file, with its conflict
// markers. Empty when the file is not conflicted.
QString listConflictsForAFile(Repository *repo, const QString &filePath);
QString listConflictsForStashFile(Repository *repo, int index,
                                  const QString &filePath);

// Bisect, with Mercurial's own bisect state (.hg/bisect.state). The first
// Good, Bad or Skip starts a session; Reset returns to where it started.
// An empty commit marks the working directory parent.
bool bisect(Repository *repo, GitRepo::BisectAction action,
            const QString &commit, GitRepo::BisectStatus &status);
// Commit node to GitRepo::BisectMark, empty without a session.
QHash<QString, int> getBisectMarks(Repository *repo);

// Subrepos (.hgsub and .hgsubstate) of the three kinds Mercurial supports.
// Mercurial subrepos are cloned and updated here. Git and Subversion subrepos
// are cloned and updated by the caller with GitRepo and SvnRepo, and then
// registered with registerSubrepo; HgRepo doesn't link those libraries.
//
// A subrepo whose checkout differs from .hgsubstate is reported by getStatus
// as a modified (or, before its first commit, new) entry named by its path.
// Staging and committing that entry records its state in .hgsubstate.
QVector<HgSubrepo> listSubrepos(Repository *repo);
// Subrepo paths.
QStringList listSubmodules(Repository *repo);
// name is the path, commit the pinned state. Subversion subrepos, whose
// checkout HgRepo can't read, are Unknown.
QVector<GitRepo::SubmoduleInfo> listSubmodulesWithStatus(Repository *repo,
                                                         const bool &quitFlag);
QString getSubmodulePath(Repository *repo, const QString &submoduleName);
// Clones the Mercurial repository url into name (at branch when given) and
// registers it.
bool addSubmodule(Repository *repo, const QString &url, const QString &name,
                  const QString &branch, const QString &uuid);
// Adds "path = [kind]source" to .hgsub and stages .hgsub and the subrepo. The
// subrepo must already be checked out at path.
bool registerSubrepo(Repository *repo, const QString &path,
                     const QString &source, HgSubrepoKind kind);
// Removes the subrepo from .hgsub and deletes its directory.
bool deleteSubmodule(Repository *repo, const QString &submoduleName,
                     const QString &submodulePath);
// Pulls a Mercurial subrepo and updates it to its pinned node. Git and
// Subversion subrepos fail with UnsupportedOperation.
bool updateSubmodule(Repository *repo, const QString &submoduleName);

// Worktrees, built on the share extension: a share is a working directory
// using the store of the main repository. Mercurial doesn't list shares, so
// the main repository keeps a registry in .hg/sourcerer/shares.json; shares
// made outside Sourcerer are not listed. Works from the main repository and
// from any of its shares.
QVector<GitRepo::WorktreeInfo> listWorktrees(Repository *repo);
// Shares the repository at path, sharing bookmarks, and updates it to branch
// ("name" or "@name"). With createBranch the bookmark or named branch (kind)
// branch is created first, at baseBranch or the working directory parent.
bool addWorktree(Repository *repo, const QString &name, const QString &path,
                 const QString &branch, bool createBranch,
                 const QString &baseBranch = QString(),
                 HgBranchKind kind = HgBranchKind::Bookmark);
// Deletes the share's directory. Without force a share with changes is kept.
bool removeWorktree(Repository *repo, const QString &name, bool force);
// A locked share can't be removed.
bool lockWorktree(Repository *repo, const QString &name, const QString &reason);
bool unlockWorktree(Repository *repo, const QString &name);

// LFS with the lfs extension. Tracked patterns are in the versioned .hglfs.
bool isLfsEnabled(Repository *repo);
// Enables the lfs extension in the repository. Its next commit of a tracked
// file adds the "lfs" requirement, so clients without the extension can't
// read the repository any more.
bool enableLfs(Repository *repo);
// Tracks or untracks files with "path:<file> = all()" entries in .hglfs and
// stages .hglfs. Adding fails with LfsNotEnabled until enableLfs.
bool addFilesToLfs(Repository *repo, const QStringList &files);
bool removeFilesFromLfs(Repository *repo, const QStringList &files);
QStringList listLfsFiles(Repository *repo);
GitRepo::Lfs::StorageStatus getLfsStorageStatus(Repository *repo);

// Branching workflows. Branch names are given as in GitRepo, without "@";
// HgRepo finds the bookmark or named branch of that name. Branches the
// workflows create are named branches (the hgflow convention) or bookmarks,
// as set per repository with setFlowBranchKind; finishing closes a named
// branch and deletes a bookmark. Configuration is in the repository's
// .hg/hgrc: [sourcerer] workflow and flow-branch-kind, [gitflow] with the
// git-flow-next keys without their "gitflow." prefix, and
// [sourcerer-gitflownext] for the applied custom workflow.
HgBranchKind getFlowBranchKind(Repository *repo);
bool setFlowBranchKind(Repository *repo, HgBranchKind kind);

// Classic Git Flow. The main and development branches are the GitFlow
// settings of ConfigManager (developmentBranchName overrides the latter);
// Mercurial's "default" stands in for a missing main branch.
bool startGitFlowBranch(Repository *repo, GitRepo::GitFlowBranchType branchType,
                        const QString &branchName,
                        const QString &developmentBranchName = {});
bool finishGitFlowBranch(Repository *repo,
                         GitRepo::GitFlowBranchType branchType,
                         const QString &branchName, const QString &author,
                         const QString &email,
                         const QString &developmentBranchName = {});

bool startGitLabFlowFeature(Repository *repo, const QString &branchName,
                            const QString &mainBranchName);
bool finishGitLabFlowFeature(Repository *repo, const QString &branchName,
                             const QString &mainBranchName,
                             const QString &author, const QString &email);
bool promoteGitLabFlowEnvironment(Repository *repo,
                                  const QStringList &environmentChain,
                                  const QString &sourceBranchName,
                                  const QString &author, const QString &email);
bool startGitLabFlowRelease(Repository *repo, const QString &releaseBranchName,
                            const QString &mainBranchName);
bool cherryPickToGitLabFlowRelease(Repository *repo, const QString &commit,
                                   const QString &releaseBranchName,
                                   const QString &author, const QString &email,
                                   bool *conflicted = nullptr);

bool startGitHubFlowBranch(Repository *repo, const QString &branchName,
                           const QString &mainBranchName);
bool updateGitHubFlowBranch(Repository *repo, const QString &branchName,
                            const QString &mainBranchName,
                            const QString &author, const QString &email);
bool finishGitHubFlowBranch(Repository *repo, const QString &branchName,
                            const QString &mainBranchName,
                            const QString &author, const QString &email);

bool readGitFlowNextConfig(Repository *repo,
                           GitRepo::GitFlowNextConfig *config);
bool initGitFlowNext(Repository *repo, const QString &mainBranchName,
                     const QString &developBranchName);
bool startGitFlowNextBranch(Repository *repo, const QString &branchType,
                            const QString &branchName);
bool finishGitFlowNextBranch(Repository *repo, const QString &branchName,
                             const QString &author, const QString &email);
bool updateGitFlowNextBranch(Repository *repo, const QString &branchName,
                             const QString &author, const QString &email);
GitRepo::WorkflowType getWorkflowType(Repository *repo);
bool setWorkflowType(Repository *repo, GitRepo::WorkflowType type);
bool applyGitFlowNextWorkflow(Repository *repo,
                              const GitRepo::GitFlowNextWorkflow &workflow);
bool getGitFlowNextWorkflowReference(Repository *repo, QString *id,
                                     int *revision);

// Remotes are the [paths] of the repository. "default" is used when no other
// default remote is set.
QVector<GitRepo::RemoteInfo> listRemotes(Repository *repo);
QString getDefaultRemote(Repository *repo);
bool setDefaultRemote(Repository *repo, const QString &remoteName);
bool addRemote(Repository *repo, const QString &remoteName,
               const QString &fetchUrl, const QString &pushUrl);
bool removeRemote(Repository *repo, const QString &remoteName);
bool updateRemote(Repository *repo, const QString &remoteName,
                  const QString &newRemoteName, const QString &fetchUrl,
                  const QString &pushUrl);
// Further URLs of a remote. The "Default" target is the remote's own URLs.
bool addTarget(Repository *repo, const QString &remoteName,
               const QString &targetName, const QString &fetchUrl,
               const QString &pushUrl);
bool editTarget(Repository *repo, const QString &remoteName,
                const QString &targetName, const QString &newFetchUrl,
                const QString &newPushUrl);
bool deleteTarget(Repository *repo, const QString &remoteName,
                  const QString &targetName);
// Makes the target's URLs the remote's URLs.
bool setDefaultTarget(Repository *repo, const QString &remoteName,
                      const QString &targetName);

// Pulls from the upstream remote of activeBranchName (the active branch when
// empty), or the default remote, and brings the working directory to the
// upstream: an update when it is a descendant, a rebase with rebase, else a
// merge that is left uncommitted.
bool pull(Repository *repo, const QString &activeBranchName = QString(),
          bool rebase = false);
// Pushes activeBranchName (the active branch when empty) to its upstream
// remote, creating the branch there when needed.
bool push(Repository *repo, const QString &activeBranchName = QString(),
          bool force = false);
// Pulls every remote without updating the working directory.
bool fetchAll(Repository *repo);
bool fetchRemote(Repository *repo, const QString &remoteName);
// "path/name" and "path/@name" of the last pull or push.
QStringList listRemoteBranches(Repository *repo, const bool &quitFlag);
// The remote branches with the commits their local counterpart (the same
// name, or the branch whose upstream they are) is ahead and behind, counted
// against the remote names of the last pull or push. For the active branch
// the counterpart is the working directory parent, because a pull already
// moves branch heads and bookmarks to the pulled commits.
QVector<GitRepo::BranchInfo> listRemoteBranchesWithStatus(Repository *repo,
                                                          const bool &quitFlag);
// Deletes the remote bookmark "path/@name", or closes and pushes the remote
// named branch "path/name".
bool deleteRemoteBranch(Repository *repo, const QString &branchName);
// Stored in the repository's [sourcerer-upstream] section.
bool setUpstream(Repository *repo, const QString &remoteName,
                 const QString &remoteBranchName,
                 const QString &localBranchName);
// (configured, remote, remote branch).
std::tuple<bool, QString, QString> getUpstream(Repository *repo,
                                               const QString &localBranchName);

} // namespace HgRepo
