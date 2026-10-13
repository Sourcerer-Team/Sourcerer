#pragma once

#include "RepoManagerBridge.h"

namespace GuiManager::Containers {

class RepoContainer;

class SvnRepoManagerBridgeImpl final : public RepoManagerBridge {
public:
  explicit SvnRepoManagerBridgeImpl(RepoContainer &container);

  RepoFeatures features() const override;

  void blame(const QWeakPointer<RepoContainer> &guard,
             const QStringList &files) override;
  void
  setUpstream(const QWeakPointer<RepoContainer> &guard,
              const QString &branchName, const QString &remoteName,
              const QString &remoteBranchName,
              const std::function<void(const QWeakPointer<RepoContainer> &)>
                  &onSuccess = nullptr) override;
  void stash(const QWeakPointer<RepoContainer> &guard,
             const QString &note) override;
  void abortMerge(const QWeakPointer<RepoContainer> &guard) override;
  void abortRebase(const QWeakPointer<RepoContainer> &guard) override;
  void addSubmodule(const QWeakPointer<RepoContainer> &guard,
                    const QString &url, const QString &name,
                    const QString &branch) override;
  void addWorktree(const QWeakPointer<RepoContainer> &guard,
                   const QString &path, const QString &branch,
                   bool createBranch, const QString &baseBranch) override;
  void removeWorktree(const QWeakPointer<RepoContainer> &guard,
                      const QString &name, bool force) override;
  void lockWorktree(const QWeakPointer<RepoContainer> &guard,
                    const QString &name, const QString &reason) override;
  void unlockWorktree(const QWeakPointer<RepoContainer> &guard,
                      const QString &name) override;
  bool getWorktrees(const QWeakPointer<RepoContainer> &guard) override;
  void amend(const QWeakPointer<RepoContainer> &guard, const QString &summary,
             const QString &description, const QString &author,
             const QString &email, bool signoff, bool signCommit) override;
  void applyPatch(const QWeakPointer<RepoContainer> &guard,
                  const QString &filePath) override;
  void
  applyStash(const QWeakPointer<RepoContainer> &guard, const int stashIndex,
             const std::function<void(const QWeakPointer<RepoContainer> &, int)>
                 &onSuccess = nullptr) override;
  void
  checkoutBranch(const QWeakPointer<RepoContainer> &guard,
                 const QString &branchName, const int branchType,
                 const std::function<void(const QWeakPointer<RepoContainer> &)>
                     &onSuccess = nullptr) override;
  void checkoutCommit(const QWeakPointer<RepoContainer> &guard,
                      const QString &commit) override;
  void closeRepo() const override;
  void commit(const QWeakPointer<RepoContainer> &guard, const QString &summary,
              const QString &description, const QString &author,
              const QString &email, bool cleanup, bool signoff,
              bool signCommit) override;
  void commitRebase(const QWeakPointer<RepoContainer> &guard,
                    const QString &summary, const QString &description,
                    const QString &author, const QString &email, bool cleanup,
                    bool signoff, bool signCommit) override;
  void createBranchFromCommit(
      const QWeakPointer<RepoContainer> &guard, const QString &commit,
      const QString &branchName,
      const std::function<void(const QWeakPointer<RepoContainer> &,
                               const QString &)> &onSuccess = nullptr) override;
  void createPatchFromCommit(const QWeakPointer<RepoContainer> &guard,
                             const QString &commit) override;
  void createTagFromCommit(const QWeakPointer<RepoContainer> &guard,
                           const QString &commit, const QString &tagName,
                           const QString &message, bool isAnnotated) override;
  void deleteBranch(const QWeakPointer<RepoContainer> &guard,
                    const QString &branchName) override;
  void deleteRemoteBranch(const QWeakPointer<RepoContainer> &guard,
                          const QString &branchName) override;
  void deleteStash(
      const QWeakPointer<RepoContainer> &guard, int stashIndex,
      const std::function<void(const QWeakPointer<RepoContainer> &, int)>
          &onSuccess = nullptr) override;
  void deleteAllStashes(
      const QWeakPointer<RepoContainer> &guard,
      const std::function<void(const QWeakPointer<RepoContainer> &)>
          &onSuccess = nullptr) override;
  void deleteTag(const QWeakPointer<RepoContainer> &guard,
                 const QString &tagName) override;
  void pushTag(const QWeakPointer<RepoContainer> &guard,
               const QString &tagName) override;
  void pushAllTags(const QWeakPointer<RepoContainer> &guard) override;
  void deleteRemoteTag(const QWeakPointer<RepoContainer> &guard,
                       const QString &tagName) override;
  void openSubmodule(const QWeakPointer<RepoContainer> &guard,
                     const QString &submoduleName) override;
  void showStashContent(const QWeakPointer<RepoContainer> &guard,
                        int stashIndex) override;
  void onStashFileSelected(const QModelIndex &index) override;
  void deleteSubmodule(const QWeakPointer<RepoContainer> &guard,
                       const QString &submoduleName) override;
  void diff(const QWeakPointer<RepoContainer> &guard,
            const QString &filePath) override;
  void diffThreeWay(const QWeakPointer<RepoContainer> &guard,
                    const QString &filePath) override;
  void diffFromCommit(const QWeakPointer<RepoContainer> &guard,
                      const QString &filePath, const QString &commit) override;
  void diffFiles(const QWeakPointer<RepoContainer> &guard,
                 const QStringList &files) override;
  void discardFiles(const QWeakPointer<RepoContainer> &guard,
                    const QStringList &files) override;
  bool getActiveBranchOrTag(const QWeakPointer<RepoContainer> &guard,
                            const int branchType) override;
  bool getBranchHistory(const QWeakPointer<RepoContainer> &guard) override;
  void getBranchHistoryPage(const QWeakPointer<RepoContainer> &guard,
                            const QString &branchName,
                            GitRepo::BranchType branchType, int page,
                            quint64 generation, bool lowPriority) override;
  QStringList getCommitBranches(const QWeakPointer<RepoContainer> &guard,
                                const QString &commit) const override;
  QStringList getCommitTags(const QWeakPointer<RepoContainer> &guard,
                            const QString &commit) const override;
  GitRepo::GitCommitStatus
  getCommitStatus(const QWeakPointer<RepoContainer> &guard,
                  const QString &commit) const override;
  bool getLocalBranches() override;
  bool getRemoteBranches() override;
  bool getStashes(const QWeakPointer<RepoContainer> &guard) override;
  bool getRemotes(const QWeakPointer<RepoContainer> &guard) override;
  bool getSubmodules() override;
  bool getTags() override;
  QPair<QString, QString>
  getUpstream(const QWeakPointer<RepoContainer> &guard) override;
  QPair<QString, QString>
  getUpstreamForBranch(const QWeakPointer<RepoContainer> &guard,
                       const QString &branchName) override;
  void merge(const QWeakPointer<RepoContainer> &guard,
             const QString &sourceBranch, const QString &destBranch) override;
  void rebase(const QWeakPointer<RepoContainer> &guard,
              const QString &sourceBranch, const QString &destBranch,
              bool signCommit) override;
  void squash(const QWeakPointer<RepoContainer> &guard,
              const QString &sourceBranch, const QString &destBranch,
              bool signCommit) override;
  void bisect(const QWeakPointer<RepoContainer> &guard,
              GitRepo::BisectAction action, const QString &commit) override;
  void startGitFlowBranch(const QWeakPointer<RepoContainer> &guard,
                          const GitRepo::GitFlowBranchType branchType,
                          const QString &branchName) override;
  void finishGitFlowBranch(const QWeakPointer<RepoContainer> &guard,
                           const GitRepo::GitFlowBranchType branchType,
                           const QString &branchName) override;
  void runGitLabFlowAction(const QWeakPointer<RepoContainer> &guard,
                           const GitRepo::GitLabFlowAction action,
                           const QString &branchName,
                           const QString &commit = QString()) override;
  void runGitHubFlowAction(const QWeakPointer<RepoContainer> &guard,
                           const GitRepo::GitHubFlowAction action,
                           const QString &branchName) override;
  void runGitFlowNextAction(const QWeakPointer<RepoContainer> &guard,
                            const GitRepo::GitFlowNextAction action,
                            const QString &name,
                            const QString &argument = QString()) override;
  void applyGitFlowNextWorkflow(
      const QWeakPointer<RepoContainer> &guard,
      const GitRepo::GitFlowNextWorkflow &workflow) override;
  void getWorkflowType(const QWeakPointer<RepoContainer> &guard) override;
  void setWorkflowType(const QWeakPointer<RepoContainer> &guard,
                       const GitRepo::WorkflowType workflow) override;
  bool openRepo(const QWeakPointer<RepoContainer> &guard) override;
  void pull(const QWeakPointer<RepoContainer> &guard,
            bool rebase = false) override;
  void fetchAll(const QWeakPointer<RepoContainer> &guard) override;
  void fetchRemote(const QWeakPointer<RepoContainer> &guard,
                   const QString &remoteName) override;
  void push(const QWeakPointer<RepoContainer> &guard,
            bool force = false) override;
  void addRemote(const QWeakPointer<RepoContainer> &guard,
                 const QString &remoteName, const QString &fetchUrl,
                 const QString &pushUrl) override;
  void editRemote(const QWeakPointer<RepoContainer> &guard, int remoteIndex,
                  const QString &remoteName, const QString &fetchUrl,
                  const QString &pushUrl) override;
  void deleteRemote(const QWeakPointer<RepoContainer> &guard,
                    int remoteIndex) override;
  void setDefaultRemote(const QWeakPointer<RepoContainer> &guard,
                        int remoteIndex) override;
  void addTarget(const QWeakPointer<RepoContainer> &guard, int remoteIndex,
                 const QString &targetName, const QString &fetchUrl,
                 const QString &pushUrl) override;
  void editTarget(const QWeakPointer<RepoContainer> &guard, int remoteIndex,
                  int targetIndex, const QString &newFetchUrl,
                  const QString &newPushUrl) override;
  void deleteTarget(const QWeakPointer<RepoContainer> &guard, int remoteIndex,
                    int targetIndex) override;
  void setDefaultTarget(const QWeakPointer<RepoContainer> &guard,
                        int remoteIndex, int targetIndex) override;
  void renameBranch(const QWeakPointer<RepoContainer> &guard,
                    const QString &oldName, const QString &newName) override;
  void renameTag(const QWeakPointer<RepoContainer> &guard,
                 const QString &oldName, const QString &newName) override;
  void reset(const QWeakPointer<RepoContainer> &guard, const QString &commit,
             const int type) override;
  void revert(const QWeakPointer<RepoContainer> &guard,
              const QString &commit) override;
  void cherryPick(const QWeakPointer<RepoContainer> &guard,
                  const QString &commit) override;
  void refreshStatus(const QWeakPointer<RepoContainer> &guard) override;
  void showCommitAndMerge(QWeakPointer<RepoContainer> guard) override;
  void stageFiles(const QWeakPointer<RepoContainer> &guard,
                  const QStringList &files) override;
  void unstageFiles(const QWeakPointer<RepoContainer> &guard,
                    const QStringList &files) override;
  void updateSubmodule(const QWeakPointer<RepoContainer> &guard,
                       const QString &submoduleName) override;
  qint64 getFileSizeFromCommit(const QString &filePath,
                               const QString &commit) override;

private:
  // Reports that a Git only operation is not available for Subversion
  // repositories.
  void reportUnsupportedOperation(const QString &operation) const;

  RepoContainer &_container;
};

} // namespace GuiManager::Containers
