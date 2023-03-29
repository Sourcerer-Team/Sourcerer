#pragma once

#include <GitRepoManagerState.h>

namespace GitRepoManager {

class ProcessingState : public GitRepoManagerState {
public:
  ProcessingState(GitRepoManagerFsm& fsm);

  // FsmState interface
public:
  void enter() override;
  bool process(const Fsm::FsmEvent &event) override;
  void exit() override;
};

} // namespace GitRepoManager
