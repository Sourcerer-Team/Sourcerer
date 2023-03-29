#pragma once

#include "GitRepoManagerFsm.h"

#include <FsmState.h>

namespace GitRepoManager {

enum class StateType : int {
  Unknown = 0,
  Initialization,
  Processing,
  last
};

class GitRepoManagerState : public Fsm::FsmState {
public:
  GitRepoManagerState(StateType type, GitRepoManagerFsm& fsm);

  GitRepoManagerFsm& fsm() { return _fsm; }

private:
  GitRepoManagerFsm& _fsm;
};

} // namespace GitRepoManager
