#pragma once

#include "SvnRepoManagerFsm.h"

#include <Component.h>
#include <EventManager.h>

namespace SvnRepoManager {

class SvnRepoManager : public ComponentManager::Component {
public:
  SvnRepoManager();
  virtual ~SvnRepoManager();

  void start() override;
  void stop() override;

private:
  void eventCallback(const EventManager::Event &event);

private:
  EventManager::EventManager &_eventManager;
  SvnRepoManagerFsm _fsm;
};

} // namespace SvnRepoManager
