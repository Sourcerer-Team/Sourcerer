#pragma once

#include <FsmState.h>

#include <QObject>

namespace UpdateManager {

class UpdateManagerFsm;

enum class StateType : int { Unknown = 0, Waiting, Updating, last };

class UpdateManagerState : public QObject, public Fsm::FsmState {
  Q_OBJECT
public:
  UpdateManagerState(StateType type, UpdateManagerFsm &fsm);

  UpdateManagerFsm &fsm() { return _fsm; }

private:
  UpdateManagerFsm &_fsm;
};

} // namespace UpdateManager
