#include "SvnRepoManagerState.h"

namespace SvnRepoManager {

SvnRepoManagerState::SvnRepoManagerState(StateType type, SvnRepoManagerFsm &fsm)
    : Fsm::FsmState((int)type), _fsm{fsm} {}

} // namespace SvnRepoManager
