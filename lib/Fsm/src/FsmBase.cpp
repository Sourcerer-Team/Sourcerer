
#include "FsmBase.h"

#include <General.h>
#include <Log.h>

#include <QString>

#define THREAD_ID "FsmBase"

const int STOP_EVENT = -1;
const int CHANGESTATE_EVENT = 0;

namespace Fsm {

FsmBase::FsmBase() : QObject(), _thread(nullptr) {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

FsmBase::~FsmBase() {
  LOG_PERF(0, "Enter");

  for (auto item : qAsConst(_stateTable))
    delete item;

  stop();

  delete _thread;

  LOG_PERF(0, "Exit");
}

void FsmBase::start() {
  LOG_PERF(0, "Enter");

  _stateTable.clear();
  _stateChangeTable.clear();

  initStateTable(_stateTable);
  initChangeTable(_stateChangeTable);

  _currentState = firstState(_stateTable);
  if (!_currentState) {
    LOG_ERROR(Log::ErrorId::Generic,
              "Internal error. First state not initialized.");
    LOG_PERF(0, "Exit");
    return;
  }

  std::function<void()> mainLoopFunction = std::bind(&FsmBase::mainLoop, this);
  _thread = QThread::create(mainLoopFunction);
  _thread->start();

  LOG_PERF(0, "Exit");
}

void FsmBase::stop() {
  LOG_PERF(0, "Enter");

  if (!_thread) {
    LOG_PERF(0, "Exit");
    return;
  }

  FsmEvent stopEvent(STOP_EVENT);
  _queueEvents.push(stopEvent);

  _thread->wait();

  while (!_queueEvents.empty())
    _queueEvents.pop();

  _currentState = nullptr;

  LOG_PERF(0, "Exit");
}

void FsmBase::send(const FsmEvent &event) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Sent event {}", eventName(event.type()));

  _queueEvents.push(event);

  LOG_PERF(0, "Exit");
}

void FsmBase::chageState(int from, int to) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Change state from {} to {}", stateName(from), stateName(to));

  if (from != to) {
    if (_stateChangeTable[from][to] == true) {
      _currentState->exit();

      if (_stateTable.find(to) != _stateTable.end()) {
        _currentState = _stateTable[to];
        _currentState->enter();
      }
    } else
      LOG_WARN("Can't switch state from {} to {}", stateName(from),
               stateName(to));
  } else
    LOG_WARN("Can't switch to the same state {}", stateName(from));

  LOG_PERF(0, "Exit");
}

void FsmBase::mainLoop() {
  LOG_PERF(0, "Enter");

  General::setThreadName("Fsm thread");

  _currentState->enter();

  while (true) {
    FsmEvent event = _queueEvents.pop();
    const int eventType = event.type();
    const QString _eventName = eventName(eventType);
    const int stateType = _currentState->type();
    const QString _stateName = stateName(stateType);

    LOG_DEBUG("Processing event {} in state {}", _eventName, _stateName);

    const long long int startTime = QDateTime::currentMSecsSinceEpoch();

    LOG_PERF(startTime - event.startTime(), "Event '{}' lag", _eventName);

    const bool eventHandled = _currentState->process(event);

    if (eventHandled == false) {
      switch (event.type()) {
      case CHANGESTATE_EVENT: {
        chageState(stateType, event.nextParam<int>());
        break;
      }
      case STOP_EVENT: {
        _currentState->exit();
        return;
      }
      default: {
        LOG_WARN("Unhandled event {}", _eventName);
        break;
      }
      }
    }

    const long long int endTime = QDateTime::currentMSecsSinceEpoch();

    LOG_PERF(endTime - startTime, "Event '{}' processing time", _eventName);
  }

  LOG_PERF(0, "Exit");
}

} // namespace Fsm
