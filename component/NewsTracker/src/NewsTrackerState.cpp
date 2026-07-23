#include "NewsTrackerState.h"

namespace NewsTracker {

NewsTrackerState::NewsTrackerState(StateType type, NewsTrackerFsm &fsm)
    : Fsm::FsmState((int)type), _fsm{fsm} {}

} // namespace NewsTracker
