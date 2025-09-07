#pragma once

#include <GitRepoManagerState.h>
#include <GitRepoManagerStructs.h>

#include <EventManager.h>
#include <LockGuard.h>

#include <QMap>
#include <QString>

namespace GitRepoManager {

class ProcessingState : public GitRepoManagerState {
  Q_OBJECT
public:
  ProcessingState(GitRepoManagerFsm &fsm);
  virtual ~ProcessingState();

  // FsmState interface
public:
  void enter() override;
  bool process(Fsm::FsmEvent event) override;
  void exit() override;

private:
  void cloneRepoWorker(const Structs::GitRepoCloneCommand &command);
  void closeRepoWorker(const Structs::GitRepoCloseCommand &command);
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
  void quitRepoWorker(const Structs::GitRepoQuitCommand &command);
  void reportError(EventManager::EventType eventType,
                   Structs::GitRepoErrorType errorType, const QString &message,
                   const Structs::GitRepoCommand &command) const;
  void scheduleWorkerForCloneRepo(const Structs::GitRepoCloneCommand &command);
  void scheduleWorkerForCloseRepo(const Structs::GitRepoCloseCommand &command);
  void scheduleWorkerForGetActiveBranch(
      const Structs::GitRepoGetActiveBranchCommand &command);
  void scheduleWorkerForGetCommitBranches(
      const Structs::GitRepoGetCommitBranchesCommand &command);
  void scheduleWorkerForGetCommitHeads(
      const Structs::GitRepoGetCommitHeadsCommand &command);
  void scheduleWorkerForGetCommitIsHead(
      const Structs::GitRepoGetCommitIsHeadCommand &command);
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
  void scheduleWorkerForQuit(const Structs::GitRepoQuitCommand &command);
  void workersThreadFunc(const QString &uuid);

private:
  std::atomic<bool> _quitAll;
  Util::Mutex _contextsMutex;
  QMap<QString, Structs::GitRepoContext *> _contexts;
  EventManager::EventManager &_eventManager;
};

} // namespace GitRepoManager
