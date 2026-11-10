// MIT License
// Copyright (c) 2026 Sourcerer

#include "HgRepoManagerState.h"

namespace HgRepoManager {

HgRepoManagerState::HgRepoManagerState(StateType type, HgRepoManagerFsm &fsm)
    : Fsm::FsmState((int)type), _fsm{fsm} {}

} // namespace HgRepoManager
