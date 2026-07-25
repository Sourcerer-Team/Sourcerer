#pragma once

#include "UpdateManagerFsm.h"

#include <Component.h>
#include <EventManager.h>

namespace UpdateManager {

class UpdateManager final : public ComponentManager::Component {
public:
  UpdateManager();
  ~UpdateManager() override;

  void start() override;
  void stop() override;

private:
  void eventCallback(const EventManager::Event &event);

private:
  EventManager::EventManager &_eventManager;
  UpdateManagerFsm _fsm;
};

} // namespace UpdateManager
