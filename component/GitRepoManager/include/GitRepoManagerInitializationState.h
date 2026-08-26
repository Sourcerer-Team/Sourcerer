#pragma once

#include <GitRepoManagerState.h>

namespace GitRepoManager {

class GitRepoManagerInitializationState : public GitRepoManagerState {
public:
  GitRepoManagerInitializationState(GitRepoManagerFsm &fsm);

  // FsmState interface
public:
  void enter() override;
  bool process(Fsm::FsmEvent event) override;
  void exit() override;
};

} // namespace GitRepoManager
