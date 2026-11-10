// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "SvnRepoManagerFsm.h"

#include <FsmState.h>

#include <QObject>

namespace SvnRepoManager {

enum class StateType : int { Unknown = 0, Initialization, Processing, last };

class SvnRepoManagerState : public QObject, public Fsm::FsmState {
  Q_OBJECT
public:
  SvnRepoManagerState(StateType type, SvnRepoManagerFsm &fsm);

  SvnRepoManagerFsm &fsm() { return _fsm; }

private:
  SvnRepoManagerFsm &_fsm;
};

} // namespace SvnRepoManager
