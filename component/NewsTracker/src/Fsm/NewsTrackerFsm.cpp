#include "Fsm/NewsTrackerFsm.h"
#include "Fsm/NewsTrackerEvent.h"
#include "Fsm/TrackingState.h"
#include "Fsm/WaitingState.h"

#include <Log.h>

#define THREAD_ID "NewsTrackerFsm"

namespace NewsTracker {

namespace Fsm {

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

  ::Fsm::FsmEvent event(0);
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
  switch(static_cast<EventType>(event)) {
  case EventType::Stop:
    return QStringLiteral("Stop");
  case EventType::ChangeState:
    return QStringLiteral("ChangeState");
  case EventType::Initialize:
    return QStringLiteral("Initialize");
  case EventType::RetrieveNews:
    return QStringLiteral("RetrieveNews");
  default:
    break;
  }

  return QStringLiteral("Unknown");
}

void NewsTrackerFsm::initStateTable(QMap<int, ::Fsm::FsmState *> &table) {
  table[(int)StateType::Waiting] = new WaitingState(*this);
  table[(int)StateType::Tracking] = new TrackingState(*this);
}

void NewsTrackerFsm::initChangeTable(QMap<int, QMap<int, bool>> &table) {
  table[(int)StateType::Waiting][(int)StateType::Tracking] = true;
}

::Fsm::FsmState *
NewsTrackerFsm::firstState(const QMap<int, ::Fsm::FsmState *> &table) const {
  const int state = (int)StateType::Waiting;
  if (!table.contains(state)) {
    LOG_ERROR(Log::ErrorId::Generic,
              "Internal error. First state not initialized.");
    return nullptr;
  }

  return table.value(state, nullptr);
}

} // namespace Fsm
} // namespace NewsTracker
