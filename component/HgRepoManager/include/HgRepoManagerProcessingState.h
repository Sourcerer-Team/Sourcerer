#pragma once

#include "HgRepoManagerState.h"
#include "HgRepoManagerStructs.h"

#include <BlockingQueue.h>
#include <EventManager.h>

#include <QMap>
#include <QString>
#include <QThread>

namespace HgRepoManager {

// Runs the commands of every open repository. Each repository has its own
// worker thread, which owns the repository's hg command server and runs its
// commands one at a time. Clones and inits run on threads of their own. The
// contexts are only touched on the FSM thread; a worker thread asks the FSM
// to retire its context when it ends.
class HgRepoManagerProcessingState final : public HgRepoManagerState {
  Q_OBJECT

public:
  explicit HgRepoManagerProcessingState(HgRepoManagerFsm &fsm);
  ~HgRepoManagerProcessingState() override;

  // FsmState interface
public:
  void enter() override;
  bool process(Fsm::FsmEvent event) override;
  void exit() override;

private:
  struct Context {
    QString uuid;
    BlockingQueue<Structs::HgRepoCommandPtr> commandsQueue;
    QThread *workersThread = nullptr;
    // Cancels the running command and the ones queued after it.
    bool quit = false;
    // A close is queued, commands for the repository are refused.
    bool closing = false;
  };

  struct Task {
    QThread *thread = nullptr;
  };

  void schedule(const Structs::HgRepoCommandPtr &command);
  void openContext(const Structs::HgRepoCommandPtr &command);
  void closeContext(Context *context, const Structs::HgRepoCommandPtr &command);
  void startTask(const Structs::HgRepoCommandPtr &command);
  void retireContext(const QString &uuid);
  void retireTask(const QString &taskId);
  void retireAll();

  // Answers every command still queued for context with an error.
  void refuseQueued(Context *context, const QString &message) const;
  void reportError(const Structs::HgRepoCommand &command,
                   const QString &message) const;

  void workersThreadFunc(Context *context);
  void taskThreadFunc(const Structs::HgRepoCommandPtr &command,
                      const QString &taskId);

  QMap<QString, Context *> _contexts;
  QMap<QString, Task> _tasks;
  // Cancels running clones and inits when the component stops.
  bool _quitTasks = false;
  EventManager::EventManager &_eventManager;
};

} // namespace HgRepoManager
