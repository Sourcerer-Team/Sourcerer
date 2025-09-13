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
      EventManager::EventType::GitRepoManagerCheckoutBranch, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCloneRepo, callbackFunc));
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
      EventManager::EventType::GitRepoManagerGetCommitStatus, callbackFunc));
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
      EventManager::EventType::GitRepoManagerStatus, callbackFunc));

  LOG_PERF(0, "Exit");
}

GitRepoManager::~GitRepoManager() {
  LOG_PERF(0, "Enter");

  const auto callbackFunc =
      std::bind(&GitRepoManager::eventCallback, this, std::placeholders::_1);

  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCheckoutBranch, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetBranchHistory, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCloneRepo, callbackFunc));
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
      EventManager::EventType::GitRepoManagerGetCommitStatus, callbackFunc));
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
        EventManager::EventType::GitRepoManagerStatus, callbackFunc));

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
  case EventManager::EventType::GitRepoManagerCheckoutBranch: {
    const auto checkoutCommand = event.data<GitRepoCheckoutBranchCommand>();
    GitRepoManagerEvent checkoutBranchEvent(EventType::CheckoutBranch);
    checkoutBranchEvent.addParam(QVariant::fromValue(checkoutCommand));
    _fsm.send(checkoutBranchEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerCloneRepo: {
    const auto cloneCommand = event.data<GitRepoCloneCommand>();
    GitRepoManagerEvent cloneRepoEvent(EventType::CloneRepo);
    cloneRepoEvent.addParam(QVariant::fromValue(cloneCommand));
    _fsm.send(cloneRepoEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerCloseRepo: {
    const auto closeCommand = event.data<GitRepoCloseCommand>();
    GitRepoManagerEvent closeRepoEvent(EventType::CloseRepo);
    closeRepoEvent.addParam(QVariant::fromValue(closeCommand));
    _fsm.send(closeRepoEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerGetActiveBranch: {
    const auto getActiveBranchCommand =
        event.data<GitRepoGetActiveBranchCommand>();
    GitRepoManagerEvent getActiveBranchEvent(EventType::GetActiveBranch);
    getActiveBranchEvent.addParam(QVariant::fromValue(getActiveBranchCommand));
    _fsm.send(getActiveBranchEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerGetBranchHistory: {
    const auto branchHistoryCommand =
        event.data<GitRepoGetBranchHistoryCommand>();
    GitRepoManagerEvent historyEvent(EventType::GetBranchHistory);
    historyEvent.addParam(QVariant::fromValue(branchHistoryCommand));
    _fsm.send(historyEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerGetCommitBranches: {
    const auto getCommitBranchesCommand =
        event.data<GitRepoGetCommitBranchesCommand>();
    GitRepoManagerEvent getCommitBranchesEvent(EventType::GetCommitBranches);
    getCommitBranchesEvent.addParam(
        QVariant::fromValue(getCommitBranchesCommand));
    _fsm.send(getCommitBranchesEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerGetCommitHeads: {
    const auto getCommitHeadsCommand =
        event.data<GitRepoGetCommitHeadsCommand>();
    GitRepoManagerEvent getCommitHeadsEvent(EventType::GetCommitHeads);
    getCommitHeadsEvent.addParam(QVariant::fromValue(getCommitHeadsCommand));
    _fsm.send(getCommitHeadsEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerGetCommitIsHead: {
    const auto getCommitIsHeadCommand =
        event.data<GitRepoGetCommitIsHeadCommand>();
    GitRepoManagerEvent getCommitIsHeadEvent(EventType::GetCommitIsHead);
    getCommitIsHeadEvent.addParam(QVariant::fromValue(getCommitIsHeadCommand));
    _fsm.send(getCommitIsHeadEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerGetCommitStatus: {
    const auto getCommitStatusCommand =
        event.data<GitRepoGetCommitStatusCommand>();
    GitRepoManagerEvent getCommitStatusEvent(EventType::GetCommitStatus);
    getCommitStatusEvent.addParam(QVariant::fromValue(getCommitStatusCommand));
    _fsm.send(getCommitStatusEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerInitRepo: {
    const auto initRepoCommand = event.data<GitRepoInitRepoCommand>();
    GitRepoManagerEvent initRepoEvent(EventType::Init);
    initRepoEvent.addParam(QVariant::fromValue(initRepoCommand));
    _fsm.send(initRepoEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerListLocalBranches: {
    const auto listLocalCommand = event.data<GitRepoListLocalBranchesCommand>();
    GitRepoManagerEvent listLocalEvent(EventType::ListLocalBranches);
    listLocalEvent.addParam(QVariant::fromValue(listLocalCommand));
    _fsm.send(listLocalEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerListRemoteBranches: {
    const auto listRemoteCommand =
        event.data<GitRepoListRemoteBranchesCommand>();
    GitRepoManagerEvent listRemoteEvent(EventType::ListRemoteBranches);
    listRemoteEvent.addParam(QVariant::fromValue(listRemoteCommand));
    _fsm.send(listRemoteEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerListSubmodules: {
    const auto listSubmodulesCommand =
        event.data<GitRepoListSubmodulesCommand>();
    GitRepoManagerEvent listSubmodulesEvent(EventType::ListSubmodules);
    listSubmodulesEvent.addParam(QVariant::fromValue(listSubmodulesCommand));
    _fsm.send(listSubmodulesEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerListTags: {
    const auto listTagsCommand = event.data<GitRepoListTagsCommand>();
    GitRepoManagerEvent listTagsEvent(EventType::ListTags);
    listTagsEvent.addParam(QVariant::fromValue(listTagsCommand));
    _fsm.send(listTagsEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerOpenRepo: {
    const auto openRepoCommand = event.data<GitRepoOpenCommand>();
    GitRepoManagerEvent openRepoEvent(EventType::OpenRepo);
    openRepoEvent.addParam(QVariant::fromValue(openRepoCommand));
    _fsm.send(openRepoEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerStatus: {
    const auto statusCommand = event.data<GitRepoStatusCommand>();
    GitRepoManagerEvent statusRepoEvent(EventType::Status);
    statusRepoEvent.addParam(QVariant::fromValue(statusCommand));
    _fsm.send(statusRepoEvent);
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
