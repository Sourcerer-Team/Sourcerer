// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "NewsTrackerState.h"

#include <FsmBase.h>
#include <FsmState.h>

namespace NewsTracker {

namespace Fsm {

class NewsTrackerFsm : public ::Fsm::FsmBase {
public:
  NewsTrackerFsm();
  virtual ~NewsTrackerFsm();

  void changeState(StateType state);

protected:
  QString stateName(int state) const override;
  QString eventName(int event) const override;
  void initStateTable(QMap<int, ::Fsm::FsmState *> &table) override;
  void initChangeTable(QMap<int, QMap<int, bool>> &table) override;
  ::Fsm::FsmState *
  firstState(const QMap<int, ::Fsm::FsmState *> &table) const override;
};

} // namespace Fsm

} // namespace NewsTracker
