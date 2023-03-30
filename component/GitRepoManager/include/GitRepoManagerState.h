#pragma once

#include "GitRepoManagerFsm.h"

#include <FsmState.h>

#include <QObject>

namespace GitRepoManager {

enum class StateType : int { Unknown = 0, Initialization, Processing, last };

class GitRepoManagerState : public QObject, public Fsm::FsmState {
  Q_OBJECT
public:
  GitRepoManagerState(StateType type, GitRepoManagerFsm &fsm);

  GitRepoManagerFsm &fsm() { return _fsm; }

private:
  GitRepoManagerFsm &_fsm;
};

} // namespace GitRepoManager
