#include "GitRepoManager.h"

#include <EventManager.h>
#include <Log.h>

#define THREAD_ID "GitRepoManager"

namespace GitRepoManager {

GitRepoManager::GitRepoManager()
{
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

GitRepoManager::~GitRepoManager()
{
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

void GitRepoManager::start()
{
  LOG_PERF(0, "Enter");

  EventManager::Event startedEvent(EventManager::EventType::ComponentStarted);
  EventManager::EventManager& eventManager = EventManager::EventManager::getInstance();
  eventManager.send(startedEvent);

  LOG_PERF(0, "Exit");
}

void GitRepoManager::stop()
{
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

}
