#pragma once

#include "GitRepoManagerFsm.h"

#include <EventManager.h>

namespace GitRepoManager {

class GitRepoManager {
public:
  GitRepoManager();
  virtual ~GitRepoManager();

  void start();
  void stop();

private:
  void eventCallback(const EventManager::Event &event);

private:
  EventManager::EventManager &_eventManager;
  GitRepoManagerFsm _fsm;
};

} // namespace GitRepoManager
