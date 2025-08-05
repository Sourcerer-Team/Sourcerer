#pragma once

#include <GitRepoManagerState.h>

namespace GitRepoManager {

class InitializationState : public GitRepoManagerState {
public:
  InitializationState(GitRepoManagerFsm &fsm);

  // FsmState interface
public:
  void enter() override;
  bool process(Fsm::FsmEvent event) override;
  void exit() override;
};

} // namespace GitRepoManager
