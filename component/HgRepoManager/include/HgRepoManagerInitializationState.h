// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "HgRepoManagerFsm.h"
#include "HgRepoManagerState.h"

namespace HgRepoManager {

class HgRepoManagerInitializationState : public HgRepoManagerState {
public:
  HgRepoManagerInitializationState(HgRepoManagerFsm &fsm);

  // FsmState interface
public:
  void enter() override;
  bool process(Fsm::FsmEvent event) override;
  void exit() override;
};

} // namespace HgRepoManager
