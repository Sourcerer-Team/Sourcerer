#pragma once

#include <NewsTrackerState.h>

#include <EventManager.h>

namespace NewsTracker {

class WaitingState : public NewsTrackerState {
public:
  explicit WaitingState(NewsTrackerFsm &fsm);
  ~WaitingState() override;

  void enter() override;
  bool process(Fsm::FsmEvent event) override;
  void exit() override;

private:
  void eventCallback(EventManager::Event event);

private:
  EventManager::EventManager &_eventManager;
};

} // namespace NewsTracker
