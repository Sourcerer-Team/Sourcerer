// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include <Fsm/NewsTrackerState.h>

namespace NewsTracker {

namespace Fsm {

class WaitingState : public NewsTrackerState {
public:
  explicit WaitingState(NewsTrackerFsm &fsm);
  ~WaitingState() override;

  void enter() override;
  bool process(::Fsm::FsmEvent event) override;
  void exit() override;
};

} // namespace Fsm
} // namespace NewsTracker
