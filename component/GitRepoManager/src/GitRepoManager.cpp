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
      EventManager::EventType::GitRepoManagerAbortMerge, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerAbortRebase, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerAddRemote, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerAddSubmodule, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerAddTarget, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerAddWorktree, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerEditTarget, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDeleteTarget, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerSetDefaultTarget, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerAmend, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerBlame, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerApplyPatch, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerApplyStash, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCheckoutBranch, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCheckoutCommit, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCherryPick, callbackFunc));
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
      EventManager::EventType::GitRepoManagerCommitRebase, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDeleteAllStashes, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDeleteBranch, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDeleteRemoteBranch, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDeleteStash, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDeleteSubmodule, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDeleteTag, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDeleteRemoteTag, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDiffFiles, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDiffFilesFromCommit,
      callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDiscardFiles, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerFetchAll, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerFetchRemote, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetActiveBranch, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetCommitBranches, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetCommitTags, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetCommitHeads, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetCommitIsHead, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetIsMergeInProgress,
      callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetIsRebaseInProgress,
      callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetCommitStatus, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetFileSizeFromCommit,
      callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetStashStatus, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetSubmodulePath, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetUpstream, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGitFlow, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerInitRepo, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListConflicts, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListLocalBranches, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListRemoteBranches, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListRemoteBranchesWithStatus,
      callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListRemotes, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListStashes, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListSubmodules, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListSubmodulesWithStatus,
      callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListTags, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListWorktrees, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerLockWorktree, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerUnlockWorktree, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerMerge, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerRebase, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerSquash, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerBisect, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerOpenRepo, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerPull, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerPush, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerPushTag, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerPushAllTags, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerRemoveRemote, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerRemoveWorktree, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerRenameBranch, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerRenameTag, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerReset, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerRevert, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerSetDefaultRemote, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerWorkflow, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerSetUpstream, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerStageFiles, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerStash, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerStatus, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetLfsStorageStatus,
      callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerSubmoduleUpdate, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerUnstageFiles, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerUpdateRemote, callbackFunc));

  LOG_PERF(0, "Exit");
}

GitRepoManager::~GitRepoManager() {
  LOG_PERF(0, "Enter");

  const auto callbackFunc =
      std::bind(&GitRepoManager::eventCallback, this, std::placeholders::_1);

  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerAbortMerge, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerAbortRebase, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerAddRemote, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerAddSubmodule, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerAddTarget, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerAddWorktree, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerEditTarget, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDeleteTarget, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerSetDefaultTarget, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerAmend, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerBlame, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerApplyPatch, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerApplyStash, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCheckoutBranch, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCheckoutCommit, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCherryPick, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetBranchHistory, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCloneRepo, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCloseRepo, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCommitStaged, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCommitRebase, callbackFunc));
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
      EventManager::EventType::GitRepoManagerDeleteAllStashes, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDeleteBranch, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDeleteRemoteBranch, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDeleteStash, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDeleteSubmodule, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDeleteTag, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDeleteRemoteTag, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDiffFiles, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDiffFilesFromCommit,
      callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerDiscardFiles, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerFetchAll, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerFetchRemote, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetActiveBranch, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetCommitBranches, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetCommitTags, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetCommitHeads, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetCommitIsHead, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetIsMergeInProgress,
      callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetIsRebaseInProgress,
      callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetCommitStatus, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetFileSizeFromCommit,
      callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetStashStatus, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetSubmodulePath, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetUpstream, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGitFlow, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListConflicts, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListLocalBranches, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListRemoteBranches, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListRemoteBranchesWithStatus,
      callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListStashes, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListSubmodules, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListSubmodulesWithStatus,
      callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListRemotes, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListTags, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListWorktrees, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerLockWorktree, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerUnlockWorktree, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerMerge, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerRebase, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerSquash, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerBisect, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerOpenRepo, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerPull, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerPush, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerPushTag, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerPushAllTags, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerRemoveRemote, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerRemoveWorktree, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerRenameBranch, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerRenameTag, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerReset, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerRevert, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerSetDefaultRemote, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerWorkflow, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerSetUpstream, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerStageFiles, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerStash, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerStatus, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetLfsStorageStatus,
      callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerSubmoduleUpdate, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerUnstageFiles, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerUpdateRemote, callbackFunc));

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
  case EventManager::EventType::GitRepoManagerAbortMerge: {
    const auto abortMergeCommand = event.data<GitRepoAbortMergeCommand>();
    GitRepoManagerEvent abortMergeEvent(EventType::AbortMerge);
    abortMergeEvent.addParam(QVariant::fromValue(abortMergeCommand));
    _fsm.send(abortMergeEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerAbortRebase: {
    const auto abortRebaseCommand = event.data<GitRepoAbortRebaseCommand>();
    GitRepoManagerEvent abortRebaseEvent(EventType::AbortRebase);
    abortRebaseEvent.addParam(QVariant::fromValue(abortRebaseCommand));
    _fsm.send(abortRebaseEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerAddRemote: {
    const auto addRemoteCommand = event.data<GitRepoAddRemoteCommand>();
    GitRepoManagerEvent addRemoteEvent(EventType::AddRemote);
    addRemoteEvent.addParam(QVariant::fromValue(addRemoteCommand));
    _fsm.send(addRemoteEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerAddSubmodule: {
    const auto addSubmoduleCommand = event.data<GitRepoAddSubmoduleCommand>();
    GitRepoManagerEvent addSubmoduleEvent(EventType::AddSubmodule);
    addSubmoduleEvent.addParam(QVariant::fromValue(addSubmoduleCommand));
    _fsm.send(addSubmoduleEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerAddTarget: {
    const auto addTargetCommand = event.data<GitRepoAddTargetCommand>();
    GitRepoManagerEvent addTargetEvent(EventType::AddTarget);
    addTargetEvent.addParam(QVariant::fromValue(addTargetCommand));
    _fsm.send(addTargetEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerAddWorktree: {
    const auto addWorktreeCommand = event.data<GitRepoAddWorktreeCommand>();
    GitRepoManagerEvent addWorktreeEvent(EventType::AddWorktree);
    addWorktreeEvent.addParam(QVariant::fromValue(addWorktreeCommand));
    _fsm.send(addWorktreeEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerEditTarget: {
    const auto editTargetCommand = event.data<GitRepoEditTargetCommand>();
    GitRepoManagerEvent editTargetEvent(EventType::EditTarget);
    editTargetEvent.addParam(QVariant::fromValue(editTargetCommand));
    _fsm.send(editTargetEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerDeleteTarget: {
    const auto deleteTargetCommand = event.data<GitRepoDeleteTargetCommand>();
    GitRepoManagerEvent deleteTargetEvent(EventType::DeleteTarget);
    deleteTargetEvent.addParam(QVariant::fromValue(deleteTargetCommand));
    _fsm.send(deleteTargetEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerSetDefaultTarget: {
    const auto setDefaultTargetCommand =
        event.data<GitRepoSetDefaultTargetCommand>();
    GitRepoManagerEvent setDefaultTargetEvent(EventType::SetDefaultTarget);
    setDefaultTargetEvent.addParam(
        QVariant::fromValue(setDefaultTargetCommand));
    _fsm.send(setDefaultTargetEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerAmend: {
    const auto amendCommand = event.data<GitRepoAmendCommand>();
    GitRepoManagerEvent amendEvent(EventType::Amend);
    amendEvent.addParam(QVariant::fromValue(amendCommand));
    _fsm.send(amendEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerBlame: {
    const auto blameCommand = event.data<GitRepoBlameCommand>();
    GitRepoManagerEvent blameEvent(EventType::Blame);
    blameEvent.addParam(QVariant::fromValue(blameCommand));
    _fsm.send(blameEvent);
    break;
  }
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
  case EventManager::EventType::GitRepoManagerCherryPick: {
    const auto cherryPickCommand = event.data<GitRepoCherryPickCommand>();
    GitRepoManagerEvent cherryPickEvent(EventType::CherryPick);
    cherryPickEvent.addParam(QVariant::fromValue(cherryPickCommand));
    _fsm.send(cherryPickEvent);
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
  case EventManager::EventType::GitRepoManagerCommitRebase: {
    const auto commitRebaseCommand = event.data<GitRepoCommitRebaseCommand>();
    GitRepoManagerEvent commitRebaseRepoEvent(EventType::CommitRebase);
    commitRebaseRepoEvent.addParam(QVariant::fromValue(commitRebaseCommand));
    _fsm.send(commitRebaseRepoEvent);
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
  case EventManager::EventType::GitRepoManagerDeleteRemoteBranch: {
    const auto deleteRemoteBranchCommand =
        event.data<GitRepoDeleteRemoteBranchCommand>();
    GitRepoManagerEvent deleteRemoteBranchEvent(EventType::DeleteRemoteBranch);
    deleteRemoteBranchEvent.addParam(
        QVariant::fromValue(deleteRemoteBranchCommand));
    _fsm.send(deleteRemoteBranchEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerDeleteStash: {
    const auto deleteStashCommand = event.data<GitRepoDeleteStashCommand>();
    GitRepoManagerEvent deleteStashEvent(EventType::DeleteStash);
    deleteStashEvent.addParam(QVariant::fromValue(deleteStashCommand));
    _fsm.send(deleteStashEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerDeleteAllStashes: {
    const auto deleteAllStashesCommand =
        event.data<GitRepoDeleteAllStashesCommand>();
    GitRepoManagerEvent deleteAllStashesEvent(EventType::DeleteAllStashes);
    deleteAllStashesEvent.addParam(
        QVariant::fromValue(deleteAllStashesCommand));
    _fsm.send(deleteAllStashesEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerDeleteSubmodule: {
    const auto deleteSubmoduleCommand =
        event.data<GitRepoDeleteSubmoduleCommand>();
    GitRepoManagerEvent deleteSubmoduleEvent(EventType::DeleteSubmodule);
    deleteSubmoduleEvent.addParam(QVariant::fromValue(deleteSubmoduleCommand));
    _fsm.send(deleteSubmoduleEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerDeleteTag: {
    const auto deleteTagCommand = event.data<GitRepoDeleteTagCommand>();
    GitRepoManagerEvent deleteTagEvent(EventType::DeleteTag);
    deleteTagEvent.addParam(QVariant::fromValue(deleteTagCommand));
    _fsm.send(deleteTagEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerDeleteRemoteTag: {
    const auto deleteRemoteTagCommand =
        event.data<GitRepoDeleteRemoteTagCommand>();
    GitRepoManagerEvent deleteRemoteTagEvent(EventType::DeleteRemoteTag);
    deleteRemoteTagEvent.addParam(QVariant::fromValue(deleteRemoteTagCommand));
    _fsm.send(deleteRemoteTagEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerDiffFiles: {
    const auto diffFilesCommand = event.data<GitRepoDiffFilesCommand>();
    GitRepoManagerEvent diffFilesEvent(EventType::DiffFiles);
    diffFilesEvent.addParam(QVariant::fromValue(diffFilesCommand));
    _fsm.send(diffFilesEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerDiffFilesFromCommit: {
    const auto diffFilesFromCommitCommand =
        event.data<GitRepoDiffFilesFromCommitCommand>();
    GitRepoManagerEvent diffFilesFromCommitEvent(
        EventType::DiffFilesFromCommit);
    diffFilesFromCommitEvent.addParam(
        QVariant::fromValue(diffFilesFromCommitCommand));
    _fsm.send(diffFilesFromCommitEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerDiscardFiles: {
    const auto discardFilesCommand = event.data<GitRepoDiscardFilesCommand>();
    GitRepoManagerEvent discardFilesEvent(EventType::DiscardFiles);
    discardFilesEvent.addParam(QVariant::fromValue(discardFilesCommand));
    _fsm.send(discardFilesEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerFetchAll: {
    const auto fetchAllCommand = event.data<GitRepoFetchAllCommand>();
    GitRepoManagerEvent fetchAllEvent(EventType::FetchAll);
    fetchAllEvent.addParam(QVariant::fromValue(fetchAllCommand));
    _fsm.send(fetchAllEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerFetchRemote: {
    const auto fetchRemoteCommand = event.data<GitRepoFetchRemoteCommand>();
    GitRepoManagerEvent fetchRemoteEvent(EventType::FetchRemote);
    fetchRemoteEvent.addParam(QVariant::fromValue(fetchRemoteCommand));
    _fsm.send(fetchRemoteEvent);
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
  case EventManager::EventType::GitRepoManagerGetCommitTags: {
    const auto getCommitTagsCommand = event.data<GitRepoGetCommitTagsCommand>();
    GitRepoManagerEvent getCommitTagsEvent(EventType::GetCommitTags);
    getCommitTagsEvent.addParam(QVariant::fromValue(getCommitTagsCommand));
    _fsm.send(getCommitTagsEvent);
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
  case EventManager::EventType::GitRepoManagerGetIsMergeInProgress: {
    const auto getIsMergeInProgressCommand =
        event.data<GitRepoGetIsMergeInProgressCommand>();
    GitRepoManagerEvent getIsMergeInProgressEvent(
        EventType::GetIsMergeInProgress);
    getIsMergeInProgressEvent.addParam(
        QVariant::fromValue(getIsMergeInProgressCommand));
    _fsm.send(getIsMergeInProgressEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerGetIsRebaseInProgress: {
    const auto getIsRebaseInProgressCommand =
        event.data<GitRepoGetIsRebaseInProgressCommand>();
    GitRepoManagerEvent getIsRebaseInProgressEvent(
        EventType::GetIsRebaseInProgress);
    getIsRebaseInProgressEvent.addParam(
        QVariant::fromValue(getIsRebaseInProgressCommand));
    _fsm.send(getIsRebaseInProgressEvent);
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
  case EventManager::EventType::GitRepoManagerGetFileSizeFromCommit: {
    const auto getFileSizeFromCommand =
        event.data<GitRepoGetFileSizeFromCommitCommand>();
    GitRepoManagerEvent getFileSizeFromCommitEvent(
        EventType::GetFileSizeFromCommit);
    getFileSizeFromCommitEvent.addParam(
        QVariant::fromValue(getFileSizeFromCommand));
    _fsm.send(getFileSizeFromCommitEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerGetStashStatus: {
    const auto getSStashStatusCommand =
        event.data<GitRepoGetStashStatusCommand>();
    GitRepoManagerEvent getStashStatusEvent(EventType::GetStashStatus);
    getStashStatusEvent.addParam(QVariant::fromValue(getSStashStatusCommand));
    _fsm.send(getStashStatusEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerGetSubmodulePath: {
    const auto getSubmodulePathCommand =
        event.data<GitRepoGetSubmodulePathCommand>();
    GitRepoManagerEvent getSubmodulePathEvent(EventType::GetSubmodulePath);
    getSubmodulePathEvent.addParam(
        QVariant::fromValue(getSubmodulePathCommand));
    _fsm.send(getSubmodulePathEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerGetUpstream: {
    const auto getUpstreamCommand = event.data<GitRepoGetUpstreamCommand>();
    GitRepoManagerEvent getUpstreamEvent(EventType::GetUpstream);
    getUpstreamEvent.addParam(QVariant::fromValue(getUpstreamCommand));
    _fsm.send(getUpstreamEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerGitFlow: {
    const auto gitFlowCommand = event.data<GitRepoGitFlowCommand>();
    GitRepoManagerEvent gitFlowEvent(EventType::GitFlow);
    gitFlowEvent.addParam(QVariant::fromValue(gitFlowCommand));
    _fsm.send(gitFlowEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerInitRepo: {
    const auto initRepoCommand = event.data<GitRepoInitRepoCommand>();
    GitRepoManagerEvent initRepoEvent(EventType::Init);
    initRepoEvent.addParam(QVariant::fromValue(initRepoCommand));
    _fsm.send(initRepoEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerListConflicts: {
    const auto listConflictsCommand = event.data<GitRepoListConflictsCommand>();
    GitRepoManagerEvent listConflictsEvent(EventType::ListConflicts);
    listConflictsEvent.addParam(QVariant::fromValue(listConflictsCommand));
    _fsm.send(listConflictsEvent);
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
  case EventManager::EventType::GitRepoManagerListRemoteBranchesWithStatus: {
    const auto listRemoteStatusCommand =
        event.data<GitRepoListRemoteBranchesWithStatusCommand>();
    GitRepoManagerEvent listRemoteStatusEvent(
        EventType::ListRemoteBranchesWithStatus);
    listRemoteStatusEvent.addParam(
        QVariant::fromValue(listRemoteStatusCommand));
    _fsm.send(listRemoteStatusEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerListRemotes: {
    const auto listRemotesCommand = event.data<GitRepoListRemotesCommand>();
    GitRepoManagerEvent listRemotesEvent(EventType::ListRemotes);
    listRemotesEvent.addParam(QVariant::fromValue(listRemotesCommand));
    _fsm.send(listRemotesEvent);
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
  case EventManager::EventType::GitRepoManagerListSubmodulesWithStatus: {
    const auto listSubmodulesWithStatusCommand =
        event.data<GitRepoListSubmodulesWithStatusCommand>();
    GitRepoManagerEvent listSubmodulesWithStatusEvent(
        EventType::ListSubmodulesWithStatus);
    listSubmodulesWithStatusEvent.addParam(
        QVariant::fromValue(listSubmodulesWithStatusCommand));
    _fsm.send(listSubmodulesWithStatusEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerListTags: {
    const auto listTagsCommand = event.data<GitRepoListTagsCommand>();
    GitRepoManagerEvent listTagsEvent(EventType::ListTags);
    listTagsEvent.addParam(QVariant::fromValue(listTagsCommand));
    _fsm.send(listTagsEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerListWorktrees: {
    const auto listWorktreesCommand = event.data<GitRepoListWorktreesCommand>();
    GitRepoManagerEvent listWorktreesEvent(EventType::ListWorktrees);
    listWorktreesEvent.addParam(QVariant::fromValue(listWorktreesCommand));
    _fsm.send(listWorktreesEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerLockWorktree: {
    const auto lockWorktreeCommand = event.data<GitRepoLockWorktreeCommand>();
    GitRepoManagerEvent lockWorktreeEvent(EventType::LockWorktree);
    lockWorktreeEvent.addParam(QVariant::fromValue(lockWorktreeCommand));
    _fsm.send(lockWorktreeEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerUnlockWorktree: {
    const auto unlockWorktreeCommand =
        event.data<GitRepoUnlockWorktreeCommand>();
    GitRepoManagerEvent unlockWorktreeEvent(EventType::UnlockWorktree);
    unlockWorktreeEvent.addParam(QVariant::fromValue(unlockWorktreeCommand));
    _fsm.send(unlockWorktreeEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerMerge: {
    const auto mergeCommand = event.data<GitRepoMergeCommand>();
    GitRepoManagerEvent mergeEvent(EventType::Merge);
    mergeEvent.addParam(QVariant::fromValue(mergeCommand));
    _fsm.send(mergeEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerRebase: {
    const auto rebaseCommand = event.data<GitRepoRebaseCommand>();
    GitRepoManagerEvent rebaseEvent(EventType::Rebase);
    rebaseEvent.addParam(QVariant::fromValue(rebaseCommand));
    _fsm.send(rebaseEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerSquash: {
    const auto squashCommand = event.data<GitRepoSquashCommand>();
    GitRepoManagerEvent squashEvent(EventType::Squash);
    squashEvent.addParam(QVariant::fromValue(squashCommand));
    _fsm.send(squashEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerBisect: {
    const auto bisectCommand = event.data<GitRepoBisectCommand>();
    GitRepoManagerEvent bisectEvent(EventType::Bisect);
    bisectEvent.addParam(QVariant::fromValue(bisectCommand));
    _fsm.send(bisectEvent);
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
  case EventManager::EventType::GitRepoManagerPushTag: {
    const auto pushTagCommand = event.data<GitRepoPushTagCommand>();
    GitRepoManagerEvent pushTagEvent(EventType::PushTag);
    pushTagEvent.addParam(QVariant::fromValue(pushTagCommand));
    _fsm.send(pushTagEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerPushAllTags: {
    const auto pushAllTagsCommand = event.data<GitRepoPushAllTagsCommand>();
    GitRepoManagerEvent pushAllTagsEvent(EventType::PushAllTags);
    pushAllTagsEvent.addParam(QVariant::fromValue(pushAllTagsCommand));
    _fsm.send(pushAllTagsEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerRemoveRemote: {
    const auto removeRemoteCommand = event.data<GitRepoRemoveRemoteCommand>();
    GitRepoManagerEvent removeRemoteEvent(EventType::RemoveRemote);
    removeRemoteEvent.addParam(QVariant::fromValue(removeRemoteCommand));
    _fsm.send(removeRemoteEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerRemoveWorktree: {
    const auto removeWorktreeCommand =
        event.data<GitRepoRemoveWorktreeCommand>();
    GitRepoManagerEvent removeWorktreeEvent(EventType::RemoveWorktree);
    removeWorktreeEvent.addParam(QVariant::fromValue(removeWorktreeCommand));
    _fsm.send(removeWorktreeEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerRenameBranch: {
    const auto renameBranchCommand = event.data<GitRepoRenameBranchCommand>();
    GitRepoManagerEvent renameBranchEvent(EventType::RenameBranch);
    renameBranchEvent.addParam(QVariant::fromValue(renameBranchCommand));
    _fsm.send(renameBranchEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerRenameTag: {
    const auto renameTagCommand = event.data<GitRepoRenameTagCommand>();
    GitRepoManagerEvent renameTagEvent(EventType::RenameTag);
    renameTagEvent.addParam(QVariant::fromValue(renameTagCommand));
    _fsm.send(renameTagEvent);
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
  case EventManager::EventType::GitRepoManagerSetDefaultRemote: {
    const auto setDefaultRemoteCommand =
        event.data<GitRepoSetDefaultRemoteCommand>();
    GitRepoManagerEvent setDefaultRemoteEvent(EventType::SetDefaultRemote);
    setDefaultRemoteEvent.addParam(
        QVariant::fromValue(setDefaultRemoteCommand));
    _fsm.send(setDefaultRemoteEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerWorkflow: {
    const auto workflowCommand = event.data<GitRepoWorkflowCommand>();
    GitRepoManagerEvent workflowEvent(EventType::Workflow);
    workflowEvent.addParam(QVariant::fromValue(workflowCommand));
    _fsm.send(workflowEvent);
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
  case EventManager::EventType::GitRepoManagerGetLfsStorageStatus: {
    const auto lfsCommand = event.data<GitRepoGetLfsStorageStatusCommand>();
    GitRepoManagerEvent lfsEvent(EventType::GetLfsStorageStatus);
    lfsEvent.addParam(QVariant::fromValue(lfsCommand));
    _fsm.send(lfsEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerStatus: {
    const auto statusCommand = event.data<GitRepoStatusCommand>();
    GitRepoManagerEvent statusRepoEvent(EventType::Status);
    statusRepoEvent.addParam(QVariant::fromValue(statusCommand));
    _fsm.send(statusRepoEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerSubmoduleUpdate: {
    const auto submoduleUpdateCommand =
        event.data<GitRepoSubmoduleUpdateCommand>();
    GitRepoManagerEvent submoduleUpdateEvent(EventType::SubmoduleUpdate);
    submoduleUpdateEvent.addParam(QVariant::fromValue(submoduleUpdateCommand));
    _fsm.send(submoduleUpdateEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerUnstageFiles: {
    const auto unstageFilesCommand = event.data<GitRepoUnstageFilesCommand>();
    GitRepoManagerEvent unstageFilesRepoEvent(EventType::UnstageFiles);
    unstageFilesRepoEvent.addParam(QVariant::fromValue(unstageFilesCommand));
    _fsm.send(unstageFilesRepoEvent);
    break;
  }
  case EventManager::EventType::GitRepoManagerUpdateRemote: {
    const auto updateRemoteCommand = event.data<GitRepoUpdateRemoteCommand>();
    GitRepoManagerEvent updateRemoteEvent(EventType::UpdateRemote);
    updateRemoteEvent.addParam(QVariant::fromValue(updateRemoteCommand));
    _fsm.send(updateRemoteEvent);
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
