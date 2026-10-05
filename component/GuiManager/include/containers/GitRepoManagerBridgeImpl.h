#pragma once

#include "GitRepoManagerBridge.h"

namespace GitRepoManager::Structs {
struct GitRepoGitFlowCommand;
} // namespace GitRepoManager::Structs

namespace GuiManager::Containers {

class GitRepoContainer;

class GitRepoManagerBridgeImpl final : public GitRepoManagerBridge {
public:
  explicit GitRepoManagerBridgeImpl(GitRepoContainer &container);

  void blame(const QWeakPointer<GitRepoContainer> &guard,
             const QStringList &files) override;
  void
  setUpstream(const QWeakPointer<GitRepoContainer> &guard,
              const QString &branchName, const QString &remoteName,
              const QString &remoteBranchName,
              const std::function<void(const QWeakPointer<GitRepoContainer> &)>
                  &onSuccess = nullptr) override;
  void stash(const QWeakPointer<GitRepoContainer> &guard,
             const QString &note) override;
  void abortMerge(const QWeakPointer<GitRepoContainer> &guard) override;
  void abortRebase(const QWeakPointer<GitRepoContainer> &guard) override;
  void addSubmodule(const QWeakPointer<GitRepoContainer> &guard,
                    const QString &url, const QString &name,
                    const QString &branch) override;
  void addWorktree(const QWeakPointer<GitRepoContainer> &guard,
                   const QString &path, const QString &branch,
                   bool createBranch, const QString &baseBranch) override;
  void removeWorktree(const QWeakPointer<GitRepoContainer> &guard,
                      const QString &name, bool force) override;
  void lockWorktree(const QWeakPointer<GitRepoContainer> &guard,
                    const QString &name, const QString &reason) override;
  void unlockWorktree(const QWeakPointer<GitRepoContainer> &guard,
                      const QString &name) override;
  bool getWorktrees(const QWeakPointer<GitRepoContainer> &guard) override;
  void amend(const QWeakPointer<GitRepoContainer> &guard,
             const QString &summary, const QString &description,
             const QString &author, const QString &email, bool signoff,
             bool signCommit) override;
  void applyPatch(const QWeakPointer<GitRepoContainer> &guard,
                  const QString &filePath) override;
  void applyStash(
      const QWeakPointer<GitRepoContainer> &guard, const int stashIndex,
      const std::function<void(const QWeakPointer<GitRepoContainer> &, int)>
          &onSuccess = nullptr) override;
  void checkoutBranch(
      const QWeakPointer<GitRepoContainer> &guard, const QString &branchName,
      const GitRepo::BranchType branchType,
      const std::function<void(const QWeakPointer<GitRepoContainer> &)>
          &onSuccess = nullptr) override;
  void checkoutCommit(const QWeakPointer<GitRepoContainer> &guard,
                      const QString &commit) override;
  void closeRepo() const override;
  void commit(const QWeakPointer<GitRepoContainer> &guard,
              const QString &summary, const QString &description,
              const QString &author, const QString &email, bool cleanup,
              bool signoff, bool signCommit) override;
  void commitRebase(const QWeakPointer<GitRepoContainer> &guard,
                    const QString &summary, const QString &description,
                    const QString &author, const QString &email, bool cleanup,
                    bool signoff, bool signCommit) override;
  void createBranchFromCommit(
      const QWeakPointer<GitRepoContainer> &guard, const QString &commit,
      const QString &branchName,
      const std::function<void(const QWeakPointer<GitRepoContainer> &,
                               const QString &)> &onSuccess = nullptr) override;
  void createPatchFromCommit(const QWeakPointer<GitRepoContainer> &guard,
                             const QString &commit) override;
  void createTagFromCommit(const QWeakPointer<GitRepoContainer> &guard,
                           const QString &commit, const QString &tagName,
                           const QString &message, bool isAnnotated) override;
  void deleteBranch(const QWeakPointer<GitRepoContainer> &guard,
                    const QString &branchName) override;
  void deleteRemoteBranch(const QWeakPointer<GitRepoContainer> &guard,
                          const QString &branchName) override;
  void deleteStash(
      const QWeakPointer<GitRepoContainer> &guard, int stashIndex,
      const std::function<void(const QWeakPointer<GitRepoContainer> &, int)>
          &onSuccess = nullptr) override;
  void deleteAllStashes(
      const QWeakPointer<GitRepoContainer> &guard,
      const std::function<void(const QWeakPointer<GitRepoContainer> &)>
          &onSuccess = nullptr) override;
  void deleteTag(const QWeakPointer<GitRepoContainer> &guard,
                 const QString &tagName) override;
  void pushTag(const QWeakPointer<GitRepoContainer> &guard,
               const QString &tagName) override;
  void pushAllTags(const QWeakPointer<GitRepoContainer> &guard) override;
  void deleteRemoteTag(const QWeakPointer<GitRepoContainer> &guard,
                       const QString &tagName) override;
  void openSubmodule(const QWeakPointer<GitRepoContainer> &guard,
                     const QString &submoduleName) override;
  void showStashContent(const QWeakPointer<GitRepoContainer> &guard,
                        int stashIndex) override;
  void onStashFileSelected(const QModelIndex &index) override;
  void deleteSubmodule(const QWeakPointer<GitRepoContainer> &guard,
                       const QString &submoduleName) override;
  void diff(const QWeakPointer<GitRepoContainer> &guard,
            const QString &filePath) override;
  void diffThreeWay(const QWeakPointer<GitRepoContainer> &guard,
                    const QString &filePath) override;
  void diffFromCommit(const QWeakPointer<GitRepoContainer> &guard,
                      const QString &filePath, const QString &commit) override;
  void diffFiles(const QWeakPointer<GitRepoContainer> &guard,
                 const QStringList &files) override;
  void discardFiles(const QWeakPointer<GitRepoContainer> &guard,
                    const QStringList &files) override;
  bool getActiveBranchOrTag(const QWeakPointer<GitRepoContainer> &guard,
                            const GitRepo::BranchType branchType) override;
  bool getBranchHistory(const QWeakPointer<GitRepoContainer> &guard) override;
  void getBranchHistoryPage(const QWeakPointer<GitRepoContainer> &guard,
                            const QString &branchName,
                            GitRepo::BranchType branchType, int page,
                            quint64 generation) override;
  QStringList getCommitBranches(const QWeakPointer<GitRepoContainer> &guard,
                                const QString &commit) const override;
  QStringList getCommitTags(const QWeakPointer<GitRepoContainer> &guard,
                            const QString &commit) const override;
  GitRepo::GitCommitStatus
  getCommitStatus(const QWeakPointer<GitRepoContainer> &guard,
                  const QString &commit) const override;
  bool getLocalBranches() override;
  bool getRemoteBranches() override;
  bool getStashes(const QWeakPointer<GitRepoContainer> &guard) override;
  bool getRemotes(const QWeakPointer<GitRepoContainer> &guard) override;
  bool getSubmodules() override;
  bool getTags() override;
  QPair<QString, QString>
  getUpstream(const QWeakPointer<GitRepoContainer> &guard) override;
  QPair<QString, QString>
  getUpstreamForBranch(const QWeakPointer<GitRepoContainer> &guard,
                       const QString &branchName) override;
  void merge(const QWeakPointer<GitRepoContainer> &guard,
             const QString &sourceBranch, const QString &destBranch) override;
  void rebase(const QWeakPointer<GitRepoContainer> &guard,
              const QString &sourceBranch, const QString &destBranch,
              bool signCommit) override;
  void squash(const QWeakPointer<GitRepoContainer> &guard,
              const QString &sourceBranch, const QString &destBranch,
              bool signCommit) override;
  void bisect(const QWeakPointer<GitRepoContainer> &guard,
              GitRepo::BisectAction action, const QString &commit) override;
  void startGitFlowBranch(const QWeakPointer<GitRepoContainer> &guard,
                          const GitRepo::GitFlowBranchType branchType,
                          const QString &branchName) override;
  void finishGitFlowBranch(const QWeakPointer<GitRepoContainer> &guard,
                           const GitRepo::GitFlowBranchType branchType,
                           const QString &branchName) override;
  void runGitLabFlowAction(const QWeakPointer<GitRepoContainer> &guard,
                           const GitRepo::GitLabFlowAction action,
                           const QString &branchName,
                           const QString &commit = QString()) override;
  void runGitHubFlowAction(const QWeakPointer<GitRepoContainer> &guard,
                           const GitRepo::GitHubFlowAction action,
                           const QString &branchName) override;
  void runGitFlowNextAction(const QWeakPointer<GitRepoContainer> &guard,
                            const GitRepo::GitFlowNextAction action,
                            const QString &name,
                            const QString &argument = QString()) override;
  void applyGitFlowNextWorkflow(
      const QWeakPointer<GitRepoContainer> &guard,
      const GitRepo::GitFlowNextWorkflow &workflow) override;
  void getWorkflowType(const QWeakPointer<GitRepoContainer> &guard) override;
  void setWorkflowType(const QWeakPointer<GitRepoContainer> &guard,
                       const GitRepo::WorkflowType workflow) override;
  bool openRepo(const QWeakPointer<GitRepoContainer> &guard) override;
  void pull(const QWeakPointer<GitRepoContainer> &guard,
            bool rebase = false) override;
  void fetchAll(const QWeakPointer<GitRepoContainer> &guard) override;
  void fetchRemote(const QWeakPointer<GitRepoContainer> &guard,
                   const QString &remoteName) override;
  void push(const QWeakPointer<GitRepoContainer> &guard,
            bool force = false) override;
  void addRemote(const QWeakPointer<GitRepoContainer> &guard,
                 const QString &remoteName, const QString &fetchUrl,
                 const QString &pushUrl) override;
  void editRemote(const QWeakPointer<GitRepoContainer> &guard, int remoteIndex,
                  const QString &remoteName, const QString &fetchUrl,
                  const QString &pushUrl) override;
  void deleteRemote(const QWeakPointer<GitRepoContainer> &guard,
                    int remoteIndex) override;
  void setDefaultRemote(const QWeakPointer<GitRepoContainer> &guard,
                        int remoteIndex) override;
  void addTarget(const QWeakPointer<GitRepoContainer> &guard, int remoteIndex,
                 const QString &targetName, const QString &fetchUrl,
                 const QString &pushUrl) override;
  void editTarget(const QWeakPointer<GitRepoContainer> &guard, int remoteIndex,
                  int targetIndex, const QString &newFetchUrl,
                  const QString &newPushUrl) override;
  void deleteTarget(const QWeakPointer<GitRepoContainer> &guard,
                    int remoteIndex, int targetIndex) override;
  void setDefaultTarget(const QWeakPointer<GitRepoContainer> &guard,
                        int remoteIndex, int targetIndex) override;
  void renameBranch(const QWeakPointer<GitRepoContainer> &guard,
                    const QString &oldName, const QString &newName) override;
  void renameTag(const QWeakPointer<GitRepoContainer> &guard,
                 const QString &oldName, const QString &newName) override;
  void reset(const QWeakPointer<GitRepoContainer> &guard, const QString &commit,
             GitRepo::ResetType type) override;
  void revert(const QWeakPointer<GitRepoContainer> &guard,
              const QString &commit) override;
  void cherryPick(const QWeakPointer<GitRepoContainer> &guard,
                  const QString &commit) override;
  void refreshStatus(const QWeakPointer<GitRepoContainer> &guard) override;
  void showCommitAndMerge(QWeakPointer<GitRepoContainer> guard) override;
  void stageFiles(const QWeakPointer<GitRepoContainer> &guard,
                  const QStringList &files) override;
  void unstageFiles(const QWeakPointer<GitRepoContainer> &guard,
                    const QStringList &files) override;
  void updateSubmodule(const QWeakPointer<GitRepoContainer> &guard,
                       const QString &submoduleName) override;
  qint64 getFileSizeFromCommit(const QString &filePath,
                               const QString &commit) override;

private:
  // Runs a Git Flow or GitLab Flow command and reports the outcome in toasts.
  // conflictMessage is shown instead of successMessage when a cherry-pick
  // left conflicts behind.
  void sendGitFlowCommand(
      const QWeakPointer<GitRepoContainer> &guard,
      const GitRepoManager::Structs::GitRepoGitFlowCommand &gitFlowCommand,
      const QString &failureMessage, const QString &successMessage,
      const QString &conflictMessage = QString());

  GitRepoContainer &_container;
};

} // namespace GuiManager::Containers
