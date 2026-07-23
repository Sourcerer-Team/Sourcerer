#include "NewsTrackerFsm.h"

#include "TrackingState.h"
#include "WaitingState.h"

#include <Log.h>

#define THREAD_ID "NewsTrackerFsm"

namespace NewsTracker {

NewsTrackerFsm::NewsTrackerFsm() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

NewsTrackerFsm::~NewsTrackerFsm() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

void NewsTrackerFsm::changeState(StateType state) {
  LOG_PERF(0, "Enter");

  Fsm::FsmEvent event(0);
  event.addParam(static_cast<int>(state));
  send(event);

  LOG_PERF(0, "Exit");
}

QString NewsTrackerFsm::stateName(int state) const {
  switch (static_cast<StateType>(state)) {
  case StateType::Waiting:
    return QStringLiteral("Waiting");
  case StateType::Tracking:
    return QStringLiteral("Tracking");
  default:
    return QStringLiteral("Unknown");
  }
}

QString NewsTrackerFsm::eventName(int event) const {
  if (event == 0) {
    return QStringLiteral("ChangeState");
  }

  return QStringLiteral("Unknown");
}

void NewsTrackerFsm::initStateTable(QMap<int, Fsm::FsmState *> &table) {
  table[(int)StateType::Waiting] = new WaitingState(*this);
  table[(int)StateType::Tracking] = new TrackingState(*this);
}

void NewsTrackerFsm::initChangeTable(QMap<int, QMap<int, bool>> &table) {
  table[(int)StateType::Waiting][(int)StateType::Tracking] = true;
}

Fsm::FsmState *
NewsTrackerFsm::firstState(const QMap<int, Fsm::FsmState *> &table) const {
  const int state = (int)StateType::Waiting;
  if (!table.contains(state)) {
    LOG_ERROR(Log::ErrorId::Generic,
              "Internal error. First state not initialized.");
    return nullptr;
  }

  return table.value(state, nullptr);
}

} // namespace NewsTracker
