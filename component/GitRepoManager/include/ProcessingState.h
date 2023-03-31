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
  void closeRepoWorker(const Structs::GitRepoCloseCommand &command);
  Structs::GitRepoContext *context(const QString &uuid);
  void destroyContext(const QString &uuid);
  void destroyContextsMap();
  void eventCallback(const EventManager::Event event);
  void listLocalBranchesWorker(
      const Structs::GitRepoListLocalBranchesCommand &command);
  void listRemoteBranchesWorker(
      const Structs::GitRepoListRemoteBranchesCommand &command);
  void openRepoWorker(const Structs::GitRepoOpenCommand &command);
  void quitRepoWorker(const Structs::GitRepoQuitCommand &command);
  void reportError(EventManager::EventType eventType,
                   Structs::GitRepoErrorType errorType, const QString &message,
                   const QString &uuid) const;
  void scheduleWorkerForCloseRepo(const Structs::GitRepoCloseCommand &command);
  void scheduleWorkerForListLocalBranches(
      const Structs::GitRepoListLocalBranchesCommand &command);
  void scheduleWorkerForListRemoteBranches(
      const Structs::GitRepoListRemoteBranchesCommand &command);
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
