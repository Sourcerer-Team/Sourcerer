#pragma once

#include "HgRepoManagerFsm.h"

#include <FsmState.h>

#include <QObject>

namespace HgRepoManager {

enum class StateType : int { Unknown = 0, Initialization, Processing, last };

class HgRepoManagerState : public QObject, public Fsm::FsmState {
  Q_OBJECT
public:
  HgRepoManagerState(StateType type, HgRepoManagerFsm &fsm);

  HgRepoManagerFsm &fsm() { return _fsm; }

private:
  HgRepoManagerFsm &_fsm;
};

} // namespace HgRepoManager
