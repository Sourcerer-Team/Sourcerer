// MIT License
// Copyright (c) 2026 Sourcerer

#include "HgRepoManagerFsm.h"
#include "HgRepoManagerEvent.h"
#include "HgRepoManagerInitializationState.h"
#include "HgRepoManagerProcessingState.h"
#include "HgRepoManagerState.h"

#include <Log.h>

#define THREAD_ID "HgRepoManagerFsm"

namespace HgRepoManager {

HgRepoManagerFsm::HgRepoManagerFsm() : Fsm::FsmBase() {}

HgRepoManagerFsm::~HgRepoManagerFsm() {}

void HgRepoManagerFsm::start() {
  LOG_PERF(0, "Enter");

  Fsm::FsmBase::start();

  HgRepoManagerEvent setupEvent(EventType::Setup);
  send(setupEvent);

  LOG_PERF(0, "Exit");
}

QString HgRepoManagerFsm::stateName(int state) const {
  switch (static_cast<StateType>(state)) {
  case StateType::Unknown:
    return "Unknown";
  case StateType::Initialization:
    return "Initialization";
  case StateType::Processing:
    return "Processing";
  case StateType::last:
    return "last";
  }

  return "Unknown";
}

QString HgRepoManagerFsm::eventName(int event) const {
  switch (static_cast<EventType>(event)) {
  case EventType::Unknown:
    return "Unknown";
  case EventType::Stop:
    return "Stop";
  case EventType::ChangeState:
    return "ChangeState";
  case EventType::Setup:
    return "Setup";
  case EventType::Command:
    return "Command";
  case EventType::RetireContext:
    return "RetireContext";
  case EventType::RetireTask:
    return "RetireTask";
  case EventType::last:
    return "last";
  }

  return "Unknown";
}

void HgRepoManagerFsm::initStateTable(QMap<int, Fsm::FsmState *> &table) {
  LOG_PERF(0, "Enter");

  table[(int)StateType::Initialization] =
      new HgRepoManagerInitializationState(*this);
  table[(int)StateType::Processing] = new HgRepoManagerProcessingState(*this);

  LOG_PERF(0, "Exit");
}

void HgRepoManagerFsm::initChangeTable(QMap<int, QMap<int, bool>> &table) {
  LOG_PERF(0, "Enter");

  table[(int)StateType::Initialization][(int)StateType::Processing] = true;
  table[(int)StateType::Processing][(int)StateType::Initialization] = true;

  LOG_PERF(0, "Exit");
}

Fsm::FsmState *
HgRepoManagerFsm::firstState(const QMap<int, Fsm::FsmState *> &table) const {
  const auto it = table.constFind((int)StateType::Initialization);
  return it == table.constEnd() ? nullptr : *it;
}

} // namespace HgRepoManager
