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

  _eventManager.subscribe(CAPTURE_CALLBACK(
    EventManager::EventType::GitRepoManagerGetBranchHistory, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCloseRepo, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetActiveBranch, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
    EventManager::EventType::GitRepoManagerGetCommitBranches, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
    EventManager::EventType::GitRepoManagerGetCommitHeads, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
    EventManager::EventType::GitRepoManagerGetCommitIsHead, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerInitRepo, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListLocalBranches, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListRemoteBranches, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListSubmodules, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListTags, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerOpenRepo, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCloneRepo, callbackFunc));

  LOG_PERF(0, "Exit");
}

GitRepoManager::~GitRepoManager() {
  LOG_PERF(0, "Enter");

  const auto callbackFunc =
      std::bind(&GitRepoManager::eventCallback, this, std::placeholders::_1);

  _eventManager.unsubscribe(CAPTURE_CALLBACK(
    EventManager::EventType::GitRepoManagerGetBranchHistory, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCloseRepo, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetActiveBranch, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
    EventManager::EventType::GitRepoManagerGetCommitBranches, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
    EventManager::EventType::GitRepoManagerGetCommitHeads, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
    EventManager::EventType::GitRepoManagerGetCommitIsHead, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListLocalBranches, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListRemoteBranches, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListSubmodules, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListTags, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerOpenRepo, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCloneRepo, callbackFunc));

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
  case EventManager::EventType::GitRepoManagerCloneRepo: {
    const GitRepoCloneCommand cloneCommand = event.data<GitRepoCloneCommand>();
    GitRepoManagerEvent cloneRepoEvent(EventType::CloneRepo);
    cloneRepoEvent.addParam(QVariant::fromValue(cloneCommand));
    _fsm.send(cloneRepoEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerCloseRepo: {
    const GitRepoCloseCommand closeCommand = event.data<GitRepoCloseCommand>();
    GitRepoManagerEvent closeRepoEvent(EventType::CloseRepo);
    closeRepoEvent.addParam(QVariant::fromValue(closeCommand));
    _fsm.send(closeRepoEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerGetActiveBranch: {
    const GitRepoGetActiveBranchCommand getActiveBranchCommand =
        event.data<GitRepoGetActiveBranchCommand>();
    GitRepoManagerEvent getActiveBranchEvent(EventType::GetActiveBranch);
    getActiveBranchEvent.addParam(QVariant::fromValue(getActiveBranchCommand));
    _fsm.send(getActiveBranchEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerGetBranchHistory: {
    const GitRepoGetBranchHistoryCommand branchHistoryCommand =
      event.data<GitRepoGetBranchHistoryCommand>();
    GitRepoManagerEvent historyEvent(EventType::GetBranchHistory);
    historyEvent.addParam(QVariant::fromValue(branchHistoryCommand));
    _fsm.send(historyEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerGetCommitBranches: {
    const GitRepoGetCommitBranchesCommand getCommitBranchesCommand =
        event.data<GitRepoGetCommitBranchesCommand>();
    GitRepoManagerEvent getCommitBranchesEvent(EventType::GetCommitBranches);
    getCommitBranchesEvent.addParam(
      QVariant::fromValue(getCommitBranchesCommand));
    _fsm.send(getCommitBranchesEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerGetCommitHeads: {
    const GitRepoGetCommitHeadsCommand getCommitHeadsCommand =
      event.data<GitRepoGetCommitHeadsCommand>();
    GitRepoManagerEvent getCommitHeadsEvent(EventType::GetCommitHeads);
    getCommitHeadsEvent.addParam(QVariant::fromValue(getCommitHeadsCommand));
    _fsm.send(getCommitHeadsEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerGetCommitIsHead: {
    const GitRepoGetCommitIsHeadCommand getCommitIsHeadCommand =
      event.data<GitRepoGetCommitIsHeadCommand>();
    GitRepoManagerEvent getCommitIsHeadEvent(EventType::GetCommitIsHead);
    getCommitIsHeadEvent.addParam(QVariant::fromValue(getCommitIsHeadCommand));
    _fsm.send(getCommitIsHeadEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerInitRepo: {
    const GitRepoInitRepoCommand initRepoCommand =
        event.data<GitRepoInitRepoCommand>();
    GitRepoManagerEvent initRepoEvent(EventType::Init);
    initRepoEvent.addParam(QVariant::fromValue(initRepoCommand));
    _fsm.send(initRepoEvent);
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
  case EventManager::EventType::GitRepoManagerListSubmodules: {
    const GitRepoListSubmodulesCommand listSubmodulesCommand =
        event.data<GitRepoListSubmodulesCommand>();
    GitRepoManagerEvent listSubmodulesEvent(EventType::ListSubmodules);
    listSubmodulesEvent.addParam(QVariant::fromValue(listSubmodulesCommand));
    _fsm.send(listSubmodulesEvent);
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
