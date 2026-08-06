#pragma once

#include "Fsm/NewsTrackerFsm.h"

#include <Component.h>
#include <ConfigManager.h>
#include <EventManager.h>
#include <Timer.h>

namespace NewsTracker {

class NewsTracker final : public ComponentManager::Component {
public:
  NewsTracker();
  ~NewsTracker() override;

  void start() override;
  void stop() override;

private:
  ConfigManager::ConfigManager &_configManager;
  EventManager::EventManager &_eventManager;
  Fsm::NewsTrackerFsm _fsm;
  Util::Timer _timer;
};

} // namespace NewsTracker
