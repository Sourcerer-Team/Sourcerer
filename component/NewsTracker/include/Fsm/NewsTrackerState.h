// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include <FsmState.h>

#include <QObject>

namespace NewsTracker {

namespace Fsm {

class NewsTrackerFsm;

enum class StateType : int { Unknown = 0, Waiting, Tracking, last };

class NewsTrackerState : public QObject, public ::Fsm::FsmState {
  Q_OBJECT
public:
  NewsTrackerState(StateType type, NewsTrackerFsm &fsm);

  NewsTrackerFsm &fsm() { return _fsm; }

private:
  NewsTrackerFsm &_fsm;
};

} // namespace Fsm
} // namespace NewsTracker
