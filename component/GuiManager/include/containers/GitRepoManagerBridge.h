#pragma once

#include <GitRepo.h>

#include <QModelIndex>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QWeakPointer>

#include <functional>

namespace GuiManager::Containers {

class GitRepoContainer;

class GitRepoManagerBridge {
public:
  virtual ~GitRepoManagerBridge() = default;

  virtual void blame(const QWeakPointer<GitRepoContainer> &guard,
                     const QStringList &files) = 0;

  virtual void
  setUpstream(const QWeakPointer<GitRepoContainer> &guard,
              const QString &branchName, const QString &remoteName,
              const QString &remoteBranchName,
              const std::function<void(const QWeakPointer<GitRepoContainer> &)>
                  &onSuccess = nullptr) = 0;
  virtual void stash(const QWeakPointer<GitRepoContainer> &guard,
                     const QString &note) = 0;
  virtual void abortMerge(const QWeakPointer<GitRepoContainer> &guard) = 0;
  virtual void abortRebase(const QWeakPointer<GitRepoContainer> &guard) = 0;
  virtual void addSubmodule(const QWeakPointer<GitRepoContainer> &guard,
                            const QString &url, const QString &name,
                            const QString &branch) = 0;
  virtual void addWorktree(const QWeakPointer<GitRepoContainer> &guard,
                           const QString &path, const QString &branch,
                           bool createBranch, const QString &baseBranch) = 0;
  virtual void removeWorktree(const QWeakPointer<GitRepoContainer> &guard,
                              const QString &name, bool force) = 0;
  virtual void lockWorktree(const QWeakPointer<GitRepoContainer> &guard,
                            const QString &name, const QString &reason) = 0;
  virtual void unlockWorktree(const QWeakPointer<GitRepoContainer> &guard,
                              const QString &name) = 0;
  virtual bool getWorktrees(const QWeakPointer<GitRepoContainer> &guard) = 0;
  virtual void amend(const QWeakPointer<GitRepoContainer> &guard,
                     const QString &summary, const QString &description,
                     const QString &author, const QString &email, bool signoff,
                     bool signCommit) = 0;
  virtual void applyPatch(const QWeakPointer<GitRepoContainer> &guard,
                          const QString &filePath) = 0;
  virtual void applyStash(
      const QWeakPointer<GitRepoContainer> &guard, const int stashIndex,
      const std::function<void(const QWeakPointer<GitRepoContainer> &, int)>
          &onSuccess = nullptr) = 0;
  virtual void checkoutBranch(
      const QWeakPointer<GitRepoContainer> &guard, const QString &branchName,
      const GitRepo::BranchType branchType,
      const std::function<void(const QWeakPointer<GitRepoContainer> &)>
          &onSuccess = nullptr) = 0;
  virtual void checkoutCommit(const QWeakPointer<GitRepoContainer> &guard,
                              const QString &commit) = 0;
  virtual void closeRepo() const = 0;
  virtual void commit(const QWeakPointer<GitRepoContainer> &guard,
                      const QString &summary, const QString &description,
                      const QString &author, const QString &email, bool cleanup,
                      bool signoff, bool signCommit) = 0;
  virtual void commitRebase(const QWeakPointer<GitRepoContainer> &guard,
                            const QString &summary, const QString &description,
                            const QString &author, const QString &email,
                            bool cleanup, bool signoff, bool signCommit) = 0;
  virtual void createBranchFromCommit(
      const QWeakPointer<GitRepoContainer> &guard, const QString &commit,
      const QString &branchName,
      const std::function<void(const QWeakPointer<GitRepoContainer> &,
                               const QString &)> &onSuccess = nullptr) = 0;
  virtual void
  createPatchFromCommit(const QWeakPointer<GitRepoContainer> &guard,
                        const QString &commit) = 0;
  virtual void createTagFromCommit(const QWeakPointer<GitRepoContainer> &guard,
                                   const QString &commit,
                                   const QString &tagName,
                                   const QString &message,
                                   bool isAnnotated) = 0;
  virtual void deleteBranch(const QWeakPointer<GitRepoContainer> &guard,
                            const QString &branchName) = 0;
  virtual void deleteRemoteBranch(const QWeakPointer<GitRepoContainer> &guard,
                                  const QString &branchName) = 0;
  virtual void deleteStash(
      const QWeakPointer<GitRepoContainer> &guard, int stashIndex,
      const std::function<void(const QWeakPointer<GitRepoContainer> &, int)>
          &onSuccess = nullptr) = 0;
  virtual void deleteAllStashes(
      const QWeakPointer<GitRepoContainer> &guard,
      const std::function<void(const QWeakPointer<GitRepoContainer> &)>
          &onSuccess = nullptr) = 0;
  virtual void deleteTag(const QWeakPointer<GitRepoContainer> &guard,
                         const QString &tagName) = 0;
  virtual void pushTag(const QWeakPointer<GitRepoContainer> &guard,
                       const QString &tagName) = 0;
  virtual void pushAllTags(const QWeakPointer<GitRepoContainer> &guard) = 0;
  virtual void deleteRemoteTag(const QWeakPointer<GitRepoContainer> &guard,
                               const QString &tagName) = 0;
  virtual void openSubmodule(const QWeakPointer<GitRepoContainer> &guard,
                             const QString &submoduleName) = 0;
  virtual void showStashContent(const QWeakPointer<GitRepoContainer> &guard,
                                int stashIndex) = 0;
  virtual void onStashFileSelected(const QModelIndex &index) = 0;
  virtual void deleteSubmodule(const QWeakPointer<GitRepoContainer> &guard,
                               const QString &submoduleName) = 0;
  virtual void diff(const QWeakPointer<GitRepoContainer> &guard,
                    const QString &filePath) = 0;
  virtual void diffThreeWay(const QWeakPointer<GitRepoContainer> &guard,
                            const QString &filePath) = 0;
  virtual void diffFromCommit(const QWeakPointer<GitRepoContainer> &guard,
                              const QString &filePath,
                              const QString &commit) = 0;
  virtual void diffFiles(const QWeakPointer<GitRepoContainer> &guard,
                         const QStringList &files) = 0;
  virtual void discardFiles(const QWeakPointer<GitRepoContainer> &guard,
                            const QStringList &files) = 0;
  virtual bool getActiveBranchOrTag(const QWeakPointer<GitRepoContainer> &guard,
                                    const GitRepo::BranchType branchType) = 0;
  virtual bool
  getBranchHistory(const QWeakPointer<GitRepoContainer> &guard) = 0;
  // Loads page `page` of the history in the background and hands it to the
  // history model on the GUI thread (GitHistoryModel::appendPage or
  // GitHistoryModel::pageFailed). A `lowPriority` page is loaded only after
  // all other requests of the container have been executed.
  virtual void getBranchHistoryPage(const QWeakPointer<GitRepoContainer> &guard,
                                    const QString &branchName,
                                    GitRepo::BranchType branchType, int page,
                                    quint64 generation, bool lowPriority) = 0;
  virtual QStringList
  getCommitBranches(const QWeakPointer<GitRepoContainer> &guard,
                    const QString &commit) const = 0;
  virtual QStringList getCommitTags(const QWeakPointer<GitRepoContainer> &guard,
                                    const QString &commit) const = 0;
  virtual GitRepo::GitCommitStatus
  getCommitStatus(const QWeakPointer<GitRepoContainer> &guard,
                  const QString &commit) const = 0;
  virtual bool getLocalBranches() = 0;
  virtual bool getRemoteBranches() = 0;
  virtual bool getStashes(const QWeakPointer<GitRepoContainer> &guard) = 0;
  virtual bool getRemotes(const QWeakPointer<GitRepoContainer> &guard) = 0;
  virtual bool getSubmodules() = 0;
  virtual bool getTags() = 0;
  virtual QPair<QString, QString>
  getUpstream(const QWeakPointer<GitRepoContainer> &guard) = 0;
  virtual QPair<QString, QString>
  getUpstreamForBranch(const QWeakPointer<GitRepoContainer> &guard,
                       const QString &branchName) = 0;
  virtual void merge(const QWeakPointer<GitRepoContainer> &guard,
                     const QString &sourceBranch,
                     const QString &destBranch) = 0;
  virtual void rebase(const QWeakPointer<GitRepoContainer> &guard,
                      const QString &sourceBranch, const QString &destBranch,
                      bool signCommit) = 0;
  virtual void squash(const QWeakPointer<GitRepoContainer> &guard,
                      const QString &sourceBranch, const QString &destBranch,
                      bool signCommit) = 0;
  virtual void bisect(const QWeakPointer<GitRepoContainer> &guard,
                      GitRepo::BisectAction action, const QString &commit) = 0;
  virtual void startGitFlowBranch(const QWeakPointer<GitRepoContainer> &guard,
                                  const GitRepo::GitFlowBranchType branchType,
                                  const QString &branchName) = 0;
  virtual void finishGitFlowBranch(const QWeakPointer<GitRepoContainer> &guard,
                                   const GitRepo::GitFlowBranchType branchType,
                                   const QString &branchName) = 0;
  // branchName is the feature or release branch, or the environment branch
  // to promote, depending on the action.
  virtual void runGitLabFlowAction(const QWeakPointer<GitRepoContainer> &guard,
                                   const GitRepo::GitLabFlowAction action,
                                   const QString &branchName,
                                   const QString &commit = QString()) = 0;
  virtual void runGitHubFlowAction(const QWeakPointer<GitRepoContainer> &guard,
                                   const GitRepo::GitHubFlowAction action,
                                   const QString &branchName) = 0;
  // name is the main branch for Init, the topic branch type for Start and the
  // branch for Finish and Update; argument is the develop branch for Init and
  // the new branch for Start.
  virtual void runGitFlowNextAction(const QWeakPointer<GitRepoContainer> &guard,
                                    const GitRepo::GitFlowNextAction action,
                                    const QString &name,
                                    const QString &argument = QString()) = 0;
  virtual void
  applyGitFlowNextWorkflow(const QWeakPointer<GitRepoContainer> &guard,
                           const GitRepo::GitFlowNextWorkflow &workflow) = 0;
  virtual void getWorkflowType(const QWeakPointer<GitRepoContainer> &guard) = 0;
  virtual void setWorkflowType(const QWeakPointer<GitRepoContainer> &guard,
                               const GitRepo::WorkflowType workflow) = 0;
  virtual bool openRepo(const QWeakPointer<GitRepoContainer> &guard) = 0;
  virtual void pull(const QWeakPointer<GitRepoContainer> &guard,
                    bool rebase = false) = 0;
  virtual void fetchAll(const QWeakPointer<GitRepoContainer> &guard) = 0;
  virtual void fetchRemote(const QWeakPointer<GitRepoContainer> &guard,
                           const QString &remoteName) = 0;
  virtual void push(const QWeakPointer<GitRepoContainer> &guard,
                    bool force = false) = 0;
  virtual void addRemote(const QWeakPointer<GitRepoContainer> &guard,
                         const QString &remoteName, const QString &fetchUrl,
                         const QString &pushUrl) = 0;
  virtual void editRemote(const QWeakPointer<GitRepoContainer> &guard,
                          int remoteIndex, const QString &remoteName,
                          const QString &fetchUrl, const QString &pushUrl) = 0;
  virtual void deleteRemote(const QWeakPointer<GitRepoContainer> &guard,
                            int remoteIndex) = 0;
  virtual void setDefaultRemote(const QWeakPointer<GitRepoContainer> &guard,
                                int remoteIndex) = 0;
  virtual void addTarget(const QWeakPointer<GitRepoContainer> &guard,
                         int remoteIndex, const QString &targetName,
                         const QString &fetchUrl, const QString &pushUrl) = 0;
  virtual void editTarget(const QWeakPointer<GitRepoContainer> &guard,
                          int remoteIndex, int targetIndex,
                          const QString &newFetchUrl,
                          const QString &newPushUrl) = 0;
  virtual void deleteTarget(const QWeakPointer<GitRepoContainer> &guard,
                            int remoteIndex, int targetIndex) = 0;
  virtual void setDefaultTarget(const QWeakPointer<GitRepoContainer> &guard,
                                int remoteIndex, int targetIndex) = 0;
  virtual void renameBranch(const QWeakPointer<GitRepoContainer> &guard,
                            const QString &oldName, const QString &newName) = 0;
  virtual void renameTag(const QWeakPointer<GitRepoContainer> &guard,
                         const QString &oldName, const QString &newName) = 0;
  virtual void reset(const QWeakPointer<GitRepoContainer> &guard,
                     const QString &commit, GitRepo::ResetType type) = 0;
  virtual void revert(const QWeakPointer<GitRepoContainer> &guard,
                      const QString &commit) = 0;
  virtual void cherryPick(const QWeakPointer<GitRepoContainer> &guard,
                          const QString &commit) = 0;
  virtual void refreshStatus(const QWeakPointer<GitRepoContainer> &guard) = 0;
  virtual void showCommitAndMerge(QWeakPointer<GitRepoContainer> guard) = 0;
  virtual void stageFiles(const QWeakPointer<GitRepoContainer> &guard,
                          const QStringList &files) = 0;
  virtual void unstageFiles(const QWeakPointer<GitRepoContainer> &guard,
                            const QStringList &files) = 0;
  virtual void updateSubmodule(const QWeakPointer<GitRepoContainer> &guard,
                               const QString &submoduleName) = 0;
  virtual qint64 getFileSizeFromCommit(const QString &filePath,
                                       const QString &commit) = 0;
};

} // namespace GuiManager::Containers
