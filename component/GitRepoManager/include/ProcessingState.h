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

  // FsmState interface
public:
  void enter() override;
  bool process(Fsm::FsmEvent event) override;
  void exit() override;

private:
  Structs::GitRepoContext *context(const QString &uuid);
  void openRepoWorker(const Structs::GitRepoOpenCommand &command);
  void reportError(Structs::GitRepoErrorType errorType, const QString& message, const QString& uuid) const;
  void scheduleWorkerForOpenRepo(const Structs::GitRepoOpenCommand &command);
  void workersThreadFunc(const QString &uuid);

private:
  Util::Mutex _contextsMutex;
  QMap<QString, Structs::GitRepoContext *> _contexts;
  EventManager::EventManager &_eventManager;
};

} // namespace GitRepoManager
