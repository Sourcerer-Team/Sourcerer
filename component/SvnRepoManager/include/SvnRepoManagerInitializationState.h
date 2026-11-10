// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "SvnRepoManagerFsm.h"
#include "SvnRepoManagerState.h"

namespace SvnRepoManager {

class SvnRepoManagerInitializationState : public SvnRepoManagerState {
public:
  SvnRepoManagerInitializationState(SvnRepoManagerFsm &fsm);

  // FsmState interface
public:
  void enter() override;
  bool process(Fsm::FsmEvent event) override;
  void exit() override;
};

} // namespace SvnRepoManager
