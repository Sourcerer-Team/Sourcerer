#pragma once

#include <GitRepoManagerState.h>
#include <GitRepoManagerStructs.h>

#include <EventManager.h>
#include <LockGuard.h>

#include <QMap>
#include <QString>

namespace GitRepoManager {

class ProcessingState final : public GitRepoManagerState {
  Q_OBJECT
public:
  explicit ProcessingState(GitRepoManagerFsm &fsm);
  ~ProcessingState() override;

  // FsmState interface
public:
  void enter() override;
  bool process(Fsm::FsmEvent event) override;
  void exit() override;

private:
  void checkoutBranchRepoWorker(
      const Structs::GitRepoCheckoutBranchCommand &command);
  void cloneRepoWorker(const Structs::GitRepoCloneCommand &command);
  void closeRepoWorker(const Structs::GitRepoCloseCommand &command);
  void commitStagedRepoWorker(const Structs::GitRepoCommitStagedCommand &command);
  Structs::GitRepoContext *context(const QString &uuid);
  void destroyContext(const QString &uuid);
  void destroyContextsMap();
  void eventCallback(const EventManager::Event event);
  void
  getActiveBranchWorker(const Structs::GitRepoGetActiveBranchCommand &command);
  void getCommitBranchesWorker(
      const Structs::GitRepoGetCommitBranchesCommand &command);
  void
  getCommitHeadsWorker(const Structs::GitRepoGetCommitHeadsCommand &command);
  void
  getCommitIsHeadWorker(const Structs::GitRepoGetCommitIsHeadCommand &command);
  void
  getCommitStatusWorker(const Structs::GitRepoGetCommitStatusCommand &command);
  void getBranchHistoryWorker(
      const Structs::GitRepoGetBranchHistoryCommand &command);
  void initRepoWorker(const Structs::GitRepoInitRepoCommand &command);
  void listLocalBranchesWorker(
      const Structs::GitRepoListLocalBranchesCommand &command);
  void listRemoteBranchesWorker(
      const Structs::GitRepoListRemoteBranchesCommand &command);
  void
  listSubmodulesWorker(const Structs::GitRepoListSubmodulesCommand &command);
  void listTagsWorker(const Structs::GitRepoListTagsCommand &command);
  void openRepoWorker(const Structs::GitRepoOpenCommand &command);
  void stageFilesWorker(const Structs::GitRepoStageFilesCommand &command);
  void statusWorker(const Structs::GitRepoStatusCommand &command);
  void unstageFilesWorker(const Structs::GitRepoUnstageFilesCommand &command);
  void quitRepoWorker(const Structs::GitRepoQuitCommand &command);
  void reportError(EventManager::EventType eventType, const QString &message,
                   const Structs::GitRepoCommand &command) const;
  void scheduleWorkerForCheckoutBranch(
      const Structs::GitRepoCheckoutBranchCommand &command);
  void scheduleWorkerForCloneRepo(const Structs::GitRepoCloneCommand &command);
  void scheduleWorkerForCloseRepo(const Structs::GitRepoCloseCommand &command);
  void scheduleWorkerForCommitStagedRepo(const Structs::GitRepoCommitStagedCommand &command);
  void scheduleWorkerForGetActiveBranch(
      const Structs::GitRepoGetActiveBranchCommand &command);
  void scheduleWorkerForGetCommitBranches(
      const Structs::GitRepoGetCommitBranchesCommand &command);
  void scheduleWorkerForGetCommitHeads(
      const Structs::GitRepoGetCommitHeadsCommand &command);
  void scheduleWorkerForGetCommitIsHead(
      const Structs::GitRepoGetCommitIsHeadCommand &command);
  void scheduleWorkerForGetCommitStatus(
      const Structs::GitRepoGetCommitStatusCommand &command);
  void scheduleWorkerForGetBranchHistory(
      const Structs::GitRepoGetBranchHistoryCommand &command);
  void
  scheduleWorkerForInitRepo(const Structs::GitRepoInitRepoCommand &command);
  void scheduleWorkerForListLocalBranches(
      const Structs::GitRepoListLocalBranchesCommand &command);
  void scheduleWorkerForListRemoteBranches(
      const Structs::GitRepoListRemoteBranchesCommand &command);
  void scheduleWorkerForListSubmodules(
      const Structs::GitRepoListSubmodulesCommand &command);
  void
  scheduleWorkerForListTags(const Structs::GitRepoListTagsCommand &command);
  void scheduleWorkerForOpenRepo(const Structs::GitRepoOpenCommand &command);
  void scheduleWorkerForStageFiles(const Structs::GitRepoStageFilesCommand &command);
  void scheduleWorkerForStatus(const Structs::GitRepoStatusCommand &command);
  void scheduleWorkerForUnstageFiles(const Structs::GitRepoUnstageFilesCommand &command);
  void scheduleWorkerForQuit(const Structs::GitRepoQuitCommand &command);
  void workersThreadFunc(const QString &uuid);

private:
  std::atomic<bool> _quitAll;
  Util::Mutex _contextsMutex;
  QMap<QString, Structs::GitRepoContext *> _contexts;
  EventManager::EventManager &_eventManager;
};

} // namespace GitRepoManager
