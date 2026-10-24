#include "HgRepoManagerEvent.h"

namespace HgRepoManager {

HgRepoManagerEvent::HgRepoManagerEvent(EventType type)
    : Fsm::FsmEvent{static_cast<int>(type)} {}

} // namespace HgRepoManager
