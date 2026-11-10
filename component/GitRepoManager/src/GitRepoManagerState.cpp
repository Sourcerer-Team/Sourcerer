// MIT License
// Copyright (c) 2026 Sourcerer

#include "GitRepoManagerState.h"

namespace GitRepoManager {

GitRepoManagerState::GitRepoManagerState(StateType type, GitRepoManagerFsm &fsm)
    : Fsm::FsmState((int)type), _fsm{fsm} {}

} // namespace GitRepoManager
