// MIT License
// Copyright (c) 2026 Sourcerer

#include "UpdateManagerFsm.h"
#include "UpdateManagerEvent.h"

#include "UpdatingState.h"
#include "WaitingState.h"

#include <Log.h>

#define THREAD_ID "UpdateManagerFsm"

namespace UpdateManager {

UpdateManagerFsm::UpdateManagerFsm() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

UpdateManagerFsm::~UpdateManagerFsm() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

void UpdateManagerFsm::changeState(StateType state) {
  LOG_PERF(0, "Enter");

  Fsm::FsmEvent event(0);
  event.addParam(static_cast<int>(state));
  send(event);

  LOG_PERF(0, "Exit");
}

QString UpdateManagerFsm::stateName(int state) const {
  switch (static_cast<StateType>(state)) {
  case StateType::Waiting:
    return QStringLiteral("Waiting");
  case StateType::Updating:
    return QStringLiteral("Updating");
  default:
    return QStringLiteral("Unknown");
  }
}

QString UpdateManagerFsm::eventName(int event) const {
  switch (static_cast<EventType>(event)) {
  case EventType::Stop: {
    return QStringLiteral("Stop");
  }
  case EventType::ChangeState: {
    return QStringLiteral("ChangeState");
  }
  case EventType::ProcessUpdate: {
    return QStringLiteral("ProcessUpdate");
  }
  case EventType::UpdateNewsReceived: {
    return QStringLiteral("UpdateNewsReceived");
  }
  default: {
    break;
  }
  }

  return QStringLiteral("Unknown");
}

void UpdateManagerFsm::initStateTable(QMap<int, Fsm::FsmState *> &table) {
  table[(int)StateType::Waiting] = new WaitingState(*this);
  table[(int)StateType::Updating] = new UpdatingState(*this);
}

void UpdateManagerFsm::initChangeTable(QMap<int, QMap<int, bool>> &table) {
  table[(int)StateType::Waiting][(int)StateType::Updating] = true;
  table[(int)StateType::Updating][(int)StateType::Waiting] = true;
}

Fsm::FsmState *
UpdateManagerFsm::firstState(const QMap<int, Fsm::FsmState *> &table) const {
  const int state = (int)StateType::Waiting;
  if (!table.contains(state)) {
    LOG_ERROR(Log::ErrorId::Generic,
              "Internal error. First state not initialized.");
    return nullptr;
  }

  return table.value(state, nullptr);
}

} // namespace UpdateManager
