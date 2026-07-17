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

class GitRepoManagerBridgeImpl final {
public:
  explicit GitRepoManagerBridgeImpl(GitRepoContainer &container);

  void
  setUpstream(const QWeakPointer<GitRepoContainer> &guard,
              const QString &branchName, const QString &remoteName,
              const QString &remoteBranchName,
              const std::function<void(const QWeakPointer<GitRepoContainer> &)>
                  &onSuccess = nullptr);
  void stash(const QWeakPointer<GitRepoContainer> &guard);
  void abortMerge(const QWeakPointer<GitRepoContainer> &guard);
  void addSubmodule(const QWeakPointer<GitRepoContainer> &guard,
                    const QString &url, const QString &name,
                    const QString &branch);
  void amend(const QWeakPointer<GitRepoContainer> &guard,
             const QString &summary, const QString &description,
             const QString &author, const QString &email, bool signoff,
             bool signCommit);
  void applyPatch(const QWeakPointer<GitRepoContainer> &guard,
                  const QString &filePath);
  void applyStash(
      const QWeakPointer<GitRepoContainer> &guard, const int stashIndex,
      const std::function<void(const QWeakPointer<GitRepoContainer> &, int)>
          &onSuccess = nullptr);
  void checkoutBranch(
      const QWeakPointer<GitRepoContainer> &guard, const QString &branchName,
      const GitRepo::BranchType branchType,
      const std::function<void(const QWeakPointer<GitRepoContainer> &)>
          &onSuccess = nullptr);
  void checkoutCommit(const QWeakPointer<GitRepoContainer> &guard,
                      const QString &commit);
  void closeRepo() const;
  void commit(const QWeakPointer<GitRepoContainer> &guard,
              const QString &summary, const QString &description,
              const QString &author, const QString &email, bool cleanup,
              bool signoff, bool signCommit);
  void createBranchFromCommit(
      const QWeakPointer<GitRepoContainer> &guard, const QString &commit,
      const QString &branchName,
      const std::function<void(const QWeakPointer<GitRepoContainer> &,
                               const QString &)> &onSuccess = nullptr);
  void createPatchFromCommit(const QWeakPointer<GitRepoContainer> &guard,
                             const QString &commit);
  void createTagFromCommit(const QWeakPointer<GitRepoContainer> &guard,
                           const QString &commit, const QString &tagName,
                           const QString &message, bool isAnnotated);
  void deleteBranch(const QWeakPointer<GitRepoContainer> &guard,
                    const QString &branchName);
  void deleteRemoteBranch(const QWeakPointer<GitRepoContainer> &guard,
                          const QString &branchName);
  void deleteStash(
      const QWeakPointer<GitRepoContainer> &guard, int stashIndex,
      const std::function<void(const QWeakPointer<GitRepoContainer> &, int)>
          &onSuccess = nullptr);
  void deleteAllStashes(
      const QWeakPointer<GitRepoContainer> &guard,
      const std::function<void(const QWeakPointer<GitRepoContainer> &)>
          &onSuccess = nullptr);
  void deleteTag(const QWeakPointer<GitRepoContainer> &guard,
                 const QString &tagName);
  void pushTag(const QWeakPointer<GitRepoContainer> &guard,
               const QString &tagName);
  void pushAllTags(const QWeakPointer<GitRepoContainer> &guard);
  void deleteRemoteTag(const QWeakPointer<GitRepoContainer> &guard,
                       const QString &tagName);
  void openSubmodule(const QWeakPointer<GitRepoContainer> &guard,
                     const QString &submoduleName);
  void showStashContent(const QWeakPointer<GitRepoContainer> &guard,
                        int stashIndex);
  void onStashFileSelected(const QModelIndex &index);
  void deleteSubmodule(const QWeakPointer<GitRepoContainer> &guard,
                       const QString &submoduleName);
  void diff(const QWeakPointer<GitRepoContainer> &guard,
            const QString &filePath);
  void diffThreeWay(const QWeakPointer<GitRepoContainer> &guard,
                    const QString &filePath);
  void diffFromCommit(const QWeakPointer<GitRepoContainer> &guard,
                      const QString &filePath, const QString &commit);
  void diffFiles(const QWeakPointer<GitRepoContainer> &guard,
                 const QStringList &files);
  void discardFiles(const QWeakPointer<GitRepoContainer> &guard,
                    const QStringList &files);
  bool getActiveBranchOrTag(const QWeakPointer<GitRepoContainer> &guard,
                            const GitRepo::BranchType branchType);
  bool getBranchHistory(const QWeakPointer<GitRepoContainer> &guard);
  QStringList getCommitBranches(const QWeakPointer<GitRepoContainer> &guard,
                                const QString &commit) const;
  QStringList getCommitTags(const QWeakPointer<GitRepoContainer> &guard,
                            const QString &commit) const;
  GitRepo::GitCommitStatus
  getCommitStatus(const QWeakPointer<GitRepoContainer> &guard,
                  const QString &commit) const;
  bool getLocalBranches();
  bool getRemoteBranches();
  bool getStashes(const QWeakPointer<GitRepoContainer> &guard);
  bool getRemotes(const QWeakPointer<GitRepoContainer> &guard);
  bool getSubmodules();
  bool getTags();
  QPair<QString, QString>
  getUpstream(const QWeakPointer<GitRepoContainer> &guard);
  QPair<QString, QString>
  getUpstreamForBranch(const QWeakPointer<GitRepoContainer> &guard,
                       const QString &branchName);
  void merge(const QWeakPointer<GitRepoContainer> &guard,
             const QString &sourceBranch, const QString &destBranch);
  void rebase(const QWeakPointer<GitRepoContainer> &guard,
              const QString &sourceBranch, const QString &destBranch);
  bool openRepo(const QWeakPointer<GitRepoContainer> &guard);
  void pull(const QWeakPointer<GitRepoContainer> &guard, bool rebase = false);
  void fetchAll(const QWeakPointer<GitRepoContainer> &guard);
  void fetchRemote(const QWeakPointer<GitRepoContainer> &guard,
                   const QString &remoteName);
  void push(const QWeakPointer<GitRepoContainer> &guard, bool force = false);
  void addRemote(const QWeakPointer<GitRepoContainer> &guard,
                 const QString &remoteName, const QString &fetchUrl,
                 const QString &pushUrl);
  void editRemote(const QWeakPointer<GitRepoContainer> &guard, int remoteIndex,
                  const QString &remoteName, const QString &fetchUrl,
                  const QString &pushUrl);
  void deleteRemote(const QWeakPointer<GitRepoContainer> &guard,
                    int remoteIndex);
  void setDefaultRemote(const QWeakPointer<GitRepoContainer> &guard,
                        int remoteIndex);
  void addTarget(const QWeakPointer<GitRepoContainer> &guard, int remoteIndex,
                 const QString &targetName, const QString &fetchUrl,
                 const QString &pushUrl);
  void editTarget(const QWeakPointer<GitRepoContainer> &guard, int remoteIndex,
                  int targetIndex, const QString &newFetchUrl,
                  const QString &newPushUrl);
  void deleteTarget(const QWeakPointer<GitRepoContainer> &guard,
                    int remoteIndex, int targetIndex);
  void setDefaultTarget(const QWeakPointer<GitRepoContainer> &guard,
                        int remoteIndex, int targetIndex);
  void renameBranch(const QWeakPointer<GitRepoContainer> &guard,
                    const QString &oldName, const QString &newName);
  void renameTag(const QWeakPointer<GitRepoContainer> &guard,
                 const QString &oldName, const QString &newName);
  void reset(const QWeakPointer<GitRepoContainer> &guard, const QString &commit,
             GitRepo::ResetType type);
  void revert(const QWeakPointer<GitRepoContainer> &guard,
              const QString &commit);
  void cherryPick(const QWeakPointer<GitRepoContainer> &guard,
                  const QString &commit);
  void refreshStatus(const QWeakPointer<GitRepoContainer> &guard);
  void showCommitAndMerge(QWeakPointer<GitRepoContainer> guard);
  void stageFiles(const QWeakPointer<GitRepoContainer> &guard,
                  const QStringList &files);
  void unstageFiles(const QWeakPointer<GitRepoContainer> &guard,
                    const QStringList &files);
  void updateSubmodule(const QWeakPointer<GitRepoContainer> &guard,
                       const QString &submoduleName);
  qint64 getFileSizeFromCommit(const QString &filePath, const QString &commit);

private:
  GitRepoContainer &_container;
};

} // namespace GuiManager::Containers
