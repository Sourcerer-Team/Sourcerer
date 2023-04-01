#include "GitRepoManager.h"
#include "GitRepoManagerEvent.h"
#include "GitRepoManagerStructs.h"

#include <Log.h>

#include <functional>

#define THREAD_ID "GitRepoManager"

using namespace GitRepoManager::Structs;

namespace GitRepoManager {

GitRepoManager::GitRepoManager()
    : _eventManager(EventManager::EventManager::getInstance()) {
  LOG_PERF(0, "Enter");

  const auto callbackFunc =
      std::bind(&GitRepoManager::eventCallback, this, std::placeholders::_1);

  _eventManager.subscribe(EventManager::EventType::GitRepoManagerCloseRepo,
                          callbackFunc);
  _eventManager.subscribe(
      EventManager::EventType::GitRepoManagerListLocalBranches, callbackFunc);
  _eventManager.subscribe(
      EventManager::EventType::GitRepoManagerListRemoteBranches, callbackFunc);
  _eventManager.subscribe(
      EventManager::EventType::GitRepoManagerListTags, callbackFunc);
  _eventManager.subscribe(EventManager::EventType::GitRepoManagerOpenRepo,
                          callbackFunc);

  LOG_PERF(0, "Exit");
}

GitRepoManager::~GitRepoManager() {
  LOG_PERF(0, "Enter");

  const auto callbackFunc =
      std::bind(&GitRepoManager::eventCallback, this, std::placeholders::_1);

  _eventManager.unsubscribe(EventManager::EventType::GitRepoManagerCloseRepo,
                            callbackFunc);
  _eventManager.unsubscribe(
      EventManager::EventType::GitRepoManagerListLocalBranches, callbackFunc);
  _eventManager.unsubscribe(
      EventManager::EventType::GitRepoManagerListRemoteBranches, callbackFunc);
  _eventManager.unsubscribe(
      EventManager::EventType::GitRepoManagerListTags, callbackFunc);
  _eventManager.unsubscribe(EventManager::EventType::GitRepoManagerOpenRepo,
                            callbackFunc);

  LOG_PERF(0, "Exit");
}

void GitRepoManager::start() {
  LOG_PERF(0, "Enter");

  _fsm.start();

  EventManager::Event startedEvent(EventManager::EventType::ComponentStarted);
  _eventManager.send(startedEvent);

  LOG_PERF(0, "Exit");
}

void GitRepoManager::stop() {
  LOG_PERF(0, "Enter");

  _fsm.stop();

  EventManager::Event stoppedEvent(EventManager::EventType::ComponentStopped);
  _eventManager.send(stoppedEvent);

  LOG_PERF(0, "Exit");
}

void GitRepoManager::eventCallback(const EventManager::Event &event) {
  LOG_PERF(0, "Enter");

  const EventManager::EventType eventType = event.type();

  LOG_DEBUG("Received event of type {}", (int)eventType);

  switch (eventType) {
  case EventManager::EventType::GitRepoManagerCloseRepo: {
    const GitRepoCloseCommand closeCommand = event.data<GitRepoCloseCommand>();
    GitRepoManagerEvent closeRepoEvent(EventType::CloseRepo);
    closeRepoEvent.addParam(QVariant::fromValue(closeCommand));
    _fsm.send(closeRepoEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerListLocalBranches: {
    const GitRepoListLocalBranchesCommand listLocalCommand =
        event.data<GitRepoListLocalBranchesCommand>();
    GitRepoManagerEvent listLocalEvent(EventType::ListLocalBranches);
    listLocalEvent.addParam(QVariant::fromValue(listLocalCommand));
    _fsm.send(listLocalEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerListRemoteBranches: {
    const GitRepoListRemoteBranchesCommand listRemoteCommand =
        event.data<GitRepoListRemoteBranchesCommand>();
    GitRepoManagerEvent listRemoteEvent(EventType::ListRemoteBranches);
    listRemoteEvent.addParam(QVariant::fromValue(listRemoteCommand));
    _fsm.send(listRemoteEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerListTags: {
    const GitRepoListTagsCommand listTagsCommand =
        event.data<GitRepoListTagsCommand>();
    GitRepoManagerEvent listTagsEvent(EventType::ListTags);
    listTagsEvent.addParam(QVariant::fromValue(listTagsCommand));
    _fsm.send(listTagsEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerOpenRepo: {
    const GitRepoOpenCommand openCommand = event.data<GitRepoOpenCommand>();
    GitRepoManagerEvent openRepoEvent(EventType::OpenRepo);
    openRepoEvent.addParam(QVariant::fromValue(openCommand));
    _fsm.send(openRepoEvent);
    break;
  }
  default: {
    LOG_WARN("Unknown event type.")
    break;
  }
  }

  LOG_PERF(0, "Exit");
}

} // namespace GitRepoManager
