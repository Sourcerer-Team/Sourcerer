#pragma once

#include "GitFlowNextWorkflow.h"
#include "GitRepoTypes.h"
#include "WorkflowUtils.h"

#include <git2/repository.h>

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <tuple>

namespace GitRepo {

// Errors, thread setup and repositories.

GitErrorContext getLastError();

void initForCurrentThread();

void shutdownForCurrentThread();

Lfs::StorageStatus getLfsStorageStatus(git_repository *repo);

bool cloneRepository(const QString &url, const QString &destinationDir,
                     const QString &uuid);

void closeRepository(git_repository *repo);

bool initRepository(const QString &repoName, const QString &workingDir,
                    const QString &defaultBranch, const QString &author,
                    const QString &email);

git_repository *openRepository(const QString &path, const QString &author,
                               const QString &email);

// Credentials.

// Closes every credentials/passphrase overlay raised on the current thread.
void finishPendingCredentialPrompts();

QString getBasicAuthHeader(const QString &repoPath, const QString &url);

QString getBasicAuthHeader(git_repository *repo, const QString &url);

GitSshAuthInfo getSshAuthInfo(const QString &repoPath, const QString &url,
                              const QString &usernameFromUrl = QString());

// Clear cached credentials for a given repository path
// This should be called after updating credentials to ensure fresh credentials
// are used
void clearCachedCredentials(const QString &repoPath);

// Working copy: status, staging, commits, discarding and resetting.

bool amend(git_repository *repo, const QString &summary, const QString &message,
           const QString &author, const QString &email, bool signoff,
           bool signCommit);

bool hasConflicts(git_repository *repo);

bool checkoutCommit(git_repository *repo, const QString &commit);

QString listConflictsForAFile(git_repository *repo, const QString &filePath);

GitStatus getStatus(git_repository *repo, const bool &quitFlag);

bool stageFiles(git_repository *repo, const QStringList &files);

bool unstageFiles(git_repository *repo, const QStringList &files);

bool commitStaged(git_repository *repo, const QString &summary,
                  const QString &message, const QString &author,
                  const QString &email, bool cleanup, bool signoff,
                  bool signCommit);

bool discardFiles(git_repository *repo, const QStringList &files);

bool resetToCommit(git_repository *repo, const QString &commit, ResetType type);

bool applyPatch(git_repository *repo, const QString &patchContent);

bool getIsMergeInProgress(git_repository *repo);

bool abortMerge(git_repository *repo);

// Diffs, patches and blame.

FilesDiff diffFiles(git_repository *repo, const QStringList &files);

FilesDiff diffFilesFromCommit(git_repository *repo, const QStringList &files,
                              const QString &commit);

BlameReport blame(git_repository *repo, const QStringList &files);

qint64 getFileSizeFromCommit(git_repository *repo, const QString &filePath,
                             const QString &commit);

QString createPatchFromCommit(git_repository *repo, const QString &commit);

// History.

GitHistory getBranchHistory(git_repository *repo, const QString &branchName,
                            BranchType type, int page, const bool &quitFlag);

QStringList getCommitBranches(git_repository *repo, const QString &commitOid,
                              const bool &quitFlag);

QStringList getCommitTags(git_repository *repo, const QString &commitOid,
                          const bool &quitFlag);

QStringList getCommitHeads(git_repository *repo, const QString &commitOid,
                           const bool &quitFlag);

bool getCommitIsHead(git_repository *repo, const QString &commitOid,
                     const bool &quitFlag);

GitCommitStatus getCommitStatus(git_repository *repo, const QString &commitOid);

// Branches and tags.

bool checkoutBranch(git_repository *repo, const QString &branchOrTagName,
                    BranchType type);

bool createBranchFromCommit(git_repository *repo, const QString &commit,
                            const QString &branchName);

QString getActiveBranch(git_repository *repo, BranchType type,
                        const bool &quitFlag);

QStringList listLocalBranches(git_repository *repo, const bool &quitFlag);

QStringList listRemoteBranches(git_repository *repo, const bool &quitFlag);

QVector<BranchInfo> listRemoteBranchesWithStatus(git_repository *repo,
                                                 const bool &quitFlag);

QStringList listTags(git_repository *repo);

bool pushTag(git_repository *repo, const QString &tagName);

bool pushAllTags(git_repository *repo);

bool renameBranch(git_repository *repo, const QString &oldName,
                  const QString &newName);

bool deleteBranch(git_repository *repo, const QString &branchName);

bool deleteRemoteBranch(git_repository *repo, const QString &branchName);

bool createTagFromCommit(git_repository *repo, const QString &commit,
                         const QString &tagName, const QString &author,
                         const QString &email, const QString &message);

bool renameTag(git_repository *repo, const QString &oldName,
               const QString &newName);

bool deleteTag(git_repository *repo, const QString &tagName);

bool deleteRemoteTag(git_repository *repo, const QString &tagName);

// Merging, rebasing, cherry-picking and reverting.

bool merge(git_repository *repo, const QString &sourceBranch,
           const QString &destinationBranch, const QString &author,
           const QString &email);

bool rebaseBranches(git_repository *repo, const QString &sourceBranch,
                    const QString &destinationBranch, const QString &author,
                    const QString &email, const bool signCommit);

bool squashBranches(git_repository *repo, const QString &sourceBranch,
                    const QString &destinationBranch, const QString &author,
                    const QString &email, const bool signCommit);

bool commitRebase(git_repository *repo, const QString &summary,
                  const QString &message, const QString &author,
                  const QString &email, bool cleanup, bool signoff,
                  bool signCommit);

bool rebaseBranches(git_repository *repo, const QString &sourceBranch,
                    const QString &destinationBranch, const QString &author,
                    const QString &email);

bool revertCommit(git_repository *repo, const QString &commit,
                  const QString &author, const QString &email,
                  const QString &message);

bool cherryPickCommit(git_repository *repo, const QString &commit,
                      const QString &author, const QString &email);

bool getIsRebaseInProgress(git_repository *repo);

bool abortRebase(git_repository *repo);

// Remotes, upstreams and synchronization.

bool pull(git_repository *rep, const QString &activeBranchName = QString(),
          bool rebase = false);

bool fetchAll(git_repository *repo);

bool fetchRemote(git_repository *repo, const QString &remoteName);

bool push(git_repository *repo, const QString &activeBranchName = QString(),
          bool force = false);

bool push(git_repository *rep);

bool setUpstream(git_repository *repo, const QString &remoteName,
                 const QString &remoteBranchName,
                 const QString &localBranchName);

std::tuple<bool, QString, QString> getUpstream(git_repository *repo,
                                               const QString &localBranchName);

// Remote management functions
QVector<RemoteInfo> listRemotes(git_repository *repo);

QString getDefaultRemote(git_repository *repo);

bool setDefaultRemote(git_repository *repo, const QString &remoteName);

bool addRemote(git_repository *repo, const QString &remoteName,
               const QString &fetchUrl, const QString &pushUrl);

bool removeRemote(git_repository *repo, const QString &remoteName);

bool updateRemote(git_repository *repo, const QString &remoteName,
                  const QString &newRemoteName, const QString &fetchUrl,
                  const QString &pushUrl);

// Target management functions (multiple targets per remote)
bool addTarget(git_repository *repo, const QString &remoteName,
               const QString &targetName, const QString &fetchUrl,
               const QString &pushUrl);

bool editTarget(git_repository *repo, const QString &remoteName,
                const QString &targetName, const QString &newFetchUrl,
                const QString &newPushUrl);

bool deleteTarget(git_repository *repo, const QString &remoteName,
                  const QString &targetName);

bool setDefaultTarget(git_repository *repo, const QString &remoteName,
                      const QString &targetName);

// Submodules.

QStringList listSubmodules(git_repository *repo);

QVector<SubmoduleInfo> listSubmodulesWithStatus(git_repository *repo,
                                                const bool &quitFlag);

QString getSubmodulePath(git_repository *repo, const QString &submoduleName);

bool deleteSubmodule(git_repository *repo, const QString &submoduleName,
                     const QString &submodulePath);

bool updateSubmodule(git_repository *repo, const QString &submoduleName);

bool addSubmodule(git_repository *repo, const QString &url, const QString &name,
                  const QString &branch, const QString &uuid);

// Stashes.

bool applyStash(git_repository *repo, int index);

bool deleteStash(git_repository *repo, int stashIndex);

// Delete all stashes in the repository
bool deleteAllStashes(git_repository *repo);

QString listConflictsForStashFile(git_repository *repo, int stashIndex,
                                  const QString &filePath);

QStringList listStashes(git_repository *repo, const bool &quitFlag);

GitCommitStatus getStashStatus(git_repository *repo, int stashIndex);

bool stash(git_repository *repo, const QString &note);

// Worktrees.

// Main worktree first (if the repository is not bare), then linked worktrees
// sorted by name (case-insensitive). Works when `repo` is a linked worktree.
// Returns {} and sets error on failure.
QVector<WorktreeInfo> listWorktrees(git_repository *repo);

// Creates a linked worktree `name` at `path`. If `createBranch` is true, a new
// local branch `branch` is created at the tip of the local branch `baseBranch`
// (or at the commit HEAD resolves to when `baseBranch` is empty) and checked
// out in the new worktree; otherwise `branch` must be an existing local branch
// and `baseBranch` is ignored. An empty `branch` is not supported.
bool addWorktree(git_repository *repo, const QString &name, const QString &path,
                 const QString &branch, bool createBranch,
                 const QString &baseBranch = QString());

// Removes a linked worktree (its working directory and admin data).
bool removeWorktree(git_repository *repo, const QString &name, bool force);

// Locks a linked worktree so that it can't be pruned. `reason` is optional.
// Locking an already locked worktree is treated as success.
bool lockWorktree(git_repository *repo, const QString &name,
                  const QString &reason);

// Unlocks a linked worktree. Unlocking a worktree that is not locked is
// treated as success.
bool unlockWorktree(git_repository *repo, const QString &name);

// Bisect.

// Binary search for the commit that introduced a change. The state is kept
// in the git directory (refs/bisect/*, BISECT_START, BISECT_LOG) so the
// session is compatible with the git command line.
bool bisect(git_repository *repo, BisectAction action, const QString &commit,
            BisectStatus &status);

// Commit SHA to BisectMark for the session in progress, empty otherwise.
QHash<QString, int> getBisectMarks(git_repository *repo);

// Branching workflows.

bool startGitFlowBranch(git_repository *repo, GitFlowBranchType branchType,
                        const QString &branchName,
                        const QString &developmentBranchName = {});

bool finishGitFlowBranch(git_repository *repo, GitFlowBranchType branchType,
                         const QString &branchName, const QString &author,
                         const QString &email,
                         const QString &developmentBranchName = {});

bool startGitLabFlowFeature(git_repository *repo, const QString &branchName,
                            const QString &mainBranchName);

bool finishGitLabFlowFeature(git_repository *repo, const QString &branchName,
                             const QString &mainBranchName,
                             const QString &author, const QString &email);

// Merges sourceBranchName into the next environment branch of the chain,
// creating that branch from sourceBranchName if it does not exist yet.
bool promoteGitLabFlowEnvironment(git_repository *repo,
                                  const QStringList &environmentChain,
                                  const QString &sourceBranchName,
                                  const QString &author, const QString &email);

bool startGitLabFlowRelease(git_repository *repo,
                            const QString &releaseBranchName,
                            const QString &mainBranchName);

// Cherry-picks commit onto releaseBranchName and returns to the original
// branch. On conflicts the release branch stays checked out and conflicted
// is set.
bool cherryPickToGitLabFlowRelease(git_repository *repo, const QString &commit,
                                   const QString &releaseBranchName,
                                   const QString &author, const QString &email,
                                   bool *conflicted = nullptr);

bool startGitHubFlowBranch(git_repository *repo, const QString &branchName,
                           const QString &mainBranchName);

// Merges the main branch into branchName to bring it up to date.
bool updateGitHubFlowBranch(git_repository *repo, const QString &branchName,
                            const QString &mainBranchName,
                            const QString &author, const QString &email);

// Merges branchName into the main branch and deletes it.
bool finishGitHubFlowBranch(git_repository *repo, const QString &branchName,
                            const QString &mainBranchName,
                            const QString &author, const QString &email);

// git-flow-next keeps its branch model in the repository config, shared with
// the git flow command line tool. See WorkflowUtils.h.
bool readGitFlowNextConfig(git_repository *repo, GitFlowNextConfig *config);

// Writes the classic preset and creates the develop branch if it is missing.
bool initGitFlowNext(git_repository *repo, const QString &mainBranchName,
                     const QString &developBranchName);

bool startGitFlowNextBranch(git_repository *repo, const QString &branchType,
                            const QString &branchName);

// Integrates branchName into its parent with the upstream strategy, tags it
// if configured, updates the auto-update base branches of the parent and
// deletes branchName.
bool finishGitFlowNextBranch(git_repository *repo, const QString &branchName,
                             const QString &author, const QString &email);

// Brings branchName up to date with its parent using the downstream strategy.
bool updateGitFlowNextBranch(git_repository *repo, const QString &branchName,
                             const QString &author, const QString &email);

// Reads sourcerer.workflow from the repository config. Missing or unknown
// values mean classic Git Flow.
WorkflowType getWorkflowType(git_repository *repo);

// Writes a custom workflow into the repository: replaces the gitflow.branch.*
// configuration, creates missing base branches from their parents and
// records the workflow id and revision (sourcerer.gitflownext.*).
bool applyGitFlowNextWorkflow(git_repository *repo,
                              const GitFlowNextWorkflow &workflow);

// The custom workflow last applied to the repository. Returns false if the
// repository doesn't use one.
bool getGitFlowNextWorkflowReference(git_repository *repo, QString *id,
                                     int *revision);

bool setWorkflowType(git_repository *repo, WorkflowType type);

} // namespace GitRepo
