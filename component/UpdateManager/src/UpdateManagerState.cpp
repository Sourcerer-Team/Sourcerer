// MIT License
// Copyright (c) 2026 Sourcerer

#include "UpdateManagerState.h"

namespace UpdateManager {

UpdateManagerState::UpdateManagerState(StateType type, UpdateManagerFsm &fsm)
    : Fsm::FsmState((int)type), _fsm{fsm} {}

} // namespace UpdateManager
