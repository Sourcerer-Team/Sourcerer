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
      EventManager::EventType::GitRepoManagerApplyPatch, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerApplyStash, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCheckoutBranch, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCheckoutCommit, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCloneRepo, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCreateBranchFromCommit,
      callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCreatePatchFromCommit,
      callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCreateTagFromCommit,
      callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetBranchHistory, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCloseRepo, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCommitStaged, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDeleteBranch, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDeleteStash, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDiffFiles, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDiscardFiles, callbackFunc));
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
      EventManager::EventType::GitRepoManagerGetUpstream, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerInitRepo, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListLocalBranches, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListRemoteBranches, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListStashes, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListSubmodules, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListTags, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerOpenRepo, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerPull, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerPush, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerRenameBranch, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerReset, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerRevert, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerSetUpstream, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerStageFiles, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerStash, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerStatus, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerUnstageFiles, callbackFunc));

  LOG_PERF(0, "Exit");
}

GitRepoManager::~GitRepoManager() {
  LOG_PERF(0, "Enter");

  const auto callbackFunc =
      std::bind(&GitRepoManager::eventCallback, this, std::placeholders::_1);

  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerApplyPatch, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerApplyStash, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCheckoutBranch, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCheckoutCommit, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetBranchHistory, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCloneRepo, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCloseRepo, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCommitStaged, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCreateBranchFromCommit,
      callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCreatePatchFromCommit,
      callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCreateTagFromCommit,
      callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDeleteBranch, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDeleteStash, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDiffFiles, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDiscardFiles, callbackFunc));
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
      EventManager::EventType::GitRepoManagerGetUpstream, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListLocalBranches, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListRemoteBranches, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListStashes, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListSubmodules, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListTags, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerOpenRepo, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerPull, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerPush, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerRenameBranch, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerReset, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerRevert, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerSetUpstream, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerStageFiles, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerStash, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerStatus, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerUnstageFiles, callbackFunc));

  LOG_PERF(0, "Exit");
}

void GitRepoManager::start() {
  LOG_PERF(0, "Enter");

  _fsm.start();

  const EventManager::Event startedEvent(
      EventManager::EventType::ComponentStarted);
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

  LOG_DEBUG("Received event {}", _eventManager.eventName(eventType));

  switch (eventType) {
  case EventManager::EventType::GitRepoManagerApplyPatch: {
    const auto applyPatchCommand = event.data<GitRepoApplyPatchCommand>();
    GitRepoManagerEvent applyPatchEvent(EventType::ApplyPatch);
    applyPatchEvent.addParam(QVariant::fromValue(applyPatchCommand));
    _fsm.send(applyPatchEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerApplyStash: {
    const auto applyStashCommand = event.data<GitRepoApplyStashCommand>();
    GitRepoManagerEvent applyStashEvent(EventType::ApplyStash);
    applyStashEvent.addParam(QVariant::fromValue(applyStashCommand));
    _fsm.send(applyStashEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerCheckoutBranch: {
    const auto checkoutCommand = event.data<GitRepoCheckoutBranchCommand>();
    GitRepoManagerEvent checkoutBranchEvent(EventType::CheckoutBranch);
    checkoutBranchEvent.addParam(QVariant::fromValue(checkoutCommand));
    _fsm.send(checkoutBranchEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerCheckoutCommit: {
    const auto checkoutCommand = event.data<GitRepoCheckoutCommitCommand>();
    GitRepoManagerEvent checkoutCommitEvent(EventType::CheckoutCommit);
    checkoutCommitEvent.addParam(QVariant::fromValue(checkoutCommand));
    _fsm.send(checkoutCommitEvent);
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
  case EventManager::EventType::GitRepoManagerCommitStaged: {
    const auto commitStagedCommand = event.data<GitRepoCommitStagedCommand>();
    GitRepoManagerEvent commitStagedRepoEvent(EventType::CommitStaged);
    commitStagedRepoEvent.addParam(QVariant::fromValue(commitStagedCommand));
    _fsm.send(commitStagedRepoEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerCreateBranchFromCommit: {
    const auto createBranchFromCommitCommand =
        event.data<GitRepoCreateBranchFromCommitCommand>();
    GitRepoManagerEvent createBranchFromCommitEvent(
        EventType::CreateBranchFromCommit);
    createBranchFromCommitEvent.addParam(
        QVariant::fromValue(createBranchFromCommitCommand));
    _fsm.send(createBranchFromCommitEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerCreatePatchFromCommit: {
    const auto createPatchFromCommitCommand =
        event.data<GitRepoCreatePatchFromCommitCommand>();
    GitRepoManagerEvent createPatchFromCommitEvent(
        EventType::CreatePatchFromCommit);
    createPatchFromCommitEvent.addParam(
        QVariant::fromValue(createPatchFromCommitCommand));
    _fsm.send(createPatchFromCommitEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerCreateTagFromCommit: {
    const auto createTagFromCommitCommand =
        event.data<GitRepoCreateTagFromCommitCommand>();
    GitRepoManagerEvent createTagFromCommitEvent(
        EventType::CreateTagFromCommit);
    createTagFromCommitEvent.addParam(
        QVariant::fromValue(createTagFromCommitCommand));
    _fsm.send(createTagFromCommitEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerDeleteBranch: {
    const auto deleteBranchCommand = event.data<GitRepoDeleteBranchCommand>();
    GitRepoManagerEvent deleteBranchEvent(EventType::DeleteBranch);
    deleteBranchEvent.addParam(QVariant::fromValue(deleteBranchCommand));
    _fsm.send(deleteBranchEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerDeleteStash: {
    const auto deleteStashCommand = event.data<GitRepoDeleteStashCommand>();
    GitRepoManagerEvent deleteStashEvent(EventType::DeleteStash);
    deleteStashEvent.addParam(QVariant::fromValue(deleteStashCommand));
    _fsm.send(deleteStashEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerDiffFiles: {
    const auto diffFilesCommand = event.data<GitRepoDiffFilesCommand>();
    GitRepoManagerEvent diffFilesEvent(EventType::DiffFiles);
    diffFilesEvent.addParam(QVariant::fromValue(diffFilesCommand));
    _fsm.send(diffFilesEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerDiscardFiles: {
    const auto discardFilesCommand = event.data<GitRepoDiscardFilesCommand>();
    GitRepoManagerEvent discardFilesEvent(EventType::DiscardFiles);
    discardFilesEvent.addParam(QVariant::fromValue(discardFilesCommand));
    _fsm.send(discardFilesEvent);
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
  case EventManager::EventType::GitRepoManagerGetUpstream: {
    const auto getUpstreamCommand = event.data<GitRepoGetUpstreamCommand>();
    GitRepoManagerEvent getUpstreamEvent(EventType::GetUpstream);
    getUpstreamEvent.addParam(QVariant::fromValue(getUpstreamCommand));
    _fsm.send(getUpstreamEvent);
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
  case EventManager::EventType::GitRepoManagerListStashes: {
    const auto listStashesCommand = event.data<GitRepoListStashesCommand>();
    GitRepoManagerEvent listStashesEvent(EventType::ListStashes);
    listStashesEvent.addParam(QVariant::fromValue(listStashesCommand));
    _fsm.send(listStashesEvent);
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
  case EventManager::EventType::GitRepoManagerPull: {
    const auto pullCommand = event.data<GitRepoPullCommand>();
    GitRepoManagerEvent pullEvent(EventType::Pull);
    pullEvent.addParam(QVariant::fromValue(pullCommand));
    _fsm.send(pullEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerPush: {
    const auto pushCommand = event.data<GitRepoPushCommand>();
    GitRepoManagerEvent pushEvent(EventType::Push);
    pushEvent.addParam(QVariant::fromValue(pushCommand));
    _fsm.send(pushEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerRenameBranch: {
    const auto renameBranchCommand = event.data<GitRepoRenameBranchCommand>();
    GitRepoManagerEvent renameBranchEvent(EventType::RenameBranch);
    renameBranchEvent.addParam(QVariant::fromValue(renameBranchCommand));
    _fsm.send(renameBranchEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerReset: {
    const auto resetCommand = event.data<GitRepoResetCommand>();
    GitRepoManagerEvent resetEvent(EventType::Reset);
    resetEvent.addParam(QVariant::fromValue(resetCommand));
    _fsm.send(resetEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerRevert: {
    const auto revertCommand = event.data<GitRepoRevertCommand>();
    GitRepoManagerEvent revertEvent(EventType::Revert);
    revertEvent.addParam(QVariant::fromValue(revertCommand));
    _fsm.send(revertEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerSetUpstream: {
    const auto setUpstreamCommand = event.data<GitRepoSetUpstreamCommand>();
    GitRepoManagerEvent setUpstreamEvent(EventType::SetUpstream);
    setUpstreamEvent.addParam(QVariant::fromValue(setUpstreamCommand));
    _fsm.send(setUpstreamEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerStageFiles: {
    const auto stageFilesCommand = event.data<GitRepoStageFilesCommand>();
    GitRepoManagerEvent stageFilesRepoEvent(EventType::StageFiles);
    stageFilesRepoEvent.addParam(QVariant::fromValue(stageFilesCommand));
    _fsm.send(stageFilesRepoEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerStash: {
    const auto stashCommand = event.data<GitRepoStashCommand>();
    GitRepoManagerEvent stashEvent(EventType::Stash);
    stashEvent.addParam(QVariant::fromValue(stashCommand));
    _fsm.send(stashEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerStatus: {
    const auto statusCommand = event.data<GitRepoStatusCommand>();
    GitRepoManagerEvent statusRepoEvent(EventType::Status);
    statusRepoEvent.addParam(QVariant::fromValue(statusCommand));
    _fsm.send(statusRepoEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerUnstageFiles: {
    const auto unstageFilesCommand = event.data<GitRepoUnstageFilesCommand>();
    GitRepoManagerEvent unstageFilesRepoEvent(EventType::UnstageFiles);
    unstageFilesRepoEvent.addParam(QVariant::fromValue(unstageFilesCommand));
    _fsm.send(unstageFilesRepoEvent);
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