#pragma once

#include "GitRepoManagerFsm.h"

#include <Component.h>
#include <EventManager.h>

namespace GitRepoManager {

class GitRepoManager : public ComponentManager::Component {
public:
  GitRepoManager();
  virtual ~GitRepoManager();

  void start() override;
  void stop() override;

private:
  void eventCallback(const EventManager::Event &event);

private:
  EventManager::EventManager &_eventManager;
  GitRepoManagerFsm _fsm;
};

} // namespace GitRepoManager
