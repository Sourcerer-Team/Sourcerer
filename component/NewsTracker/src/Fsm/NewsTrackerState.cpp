// MIT License
// Copyright (c) 2026 Sourcerer

#include "Fsm/NewsTrackerState.h"

namespace NewsTracker {

namespace Fsm {

NewsTrackerState::NewsTrackerState(StateType type, NewsTrackerFsm &fsm)
    : ::Fsm::FsmState((int)type), _fsm{fsm} {}

} // namespace Fsm
} // namespace NewsTracker
