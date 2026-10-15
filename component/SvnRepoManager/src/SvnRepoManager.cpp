#include "SvnRepoManager.h"
#include "SvnRepoManagerEvent.h"
#include "SvnRepoManagerStructs.h"

#include <Log.h>

#include <functional>

#define THREAD_ID "SvnRepoManager"

using namespace SvnRepoManager::Structs;

namespace SvnRepoManager {

SvnRepoManager::SvnRepoManager()
    : _eventManager(EventManager::EventManager::getInstance()) {
  LOG_PERF(0, "Enter");

  const auto callbackFunc =
      std::bind(&SvnRepoManager::eventCallback, this, std::placeholders::_1);

  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerAbortMerge, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerResolveTreeConflict, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerAddRemote, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerAddSubmodule, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerAddTarget, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerEditTarget, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerDeleteTarget, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerSetDefaultTarget, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerAmend, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerBlame, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerApplyPatch, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerApplyStash, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerCheckoutBranch, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerCheckoutCommit, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerCherryPick, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerCloneRepo, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerCreateBranchFromCommit,
      callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerCreatePatchFromCommit,
      callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerCreateTagFromCommit,
      callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerGetBranchHistory, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerCloseRepo, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerCommitStaged, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerDeleteAllStashes, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerDeleteBranch, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerDeleteRemoteBranch, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerDeleteStash, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerDeleteSubmodule, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerDeleteTag, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerDeleteRemoteTag, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerDiffFiles, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerDiffFilesFromCommit,
      callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerDiscardFiles, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerFetchAll, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerFetchRemote, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerGetActiveBranch, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerGetCommitBranches, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerGetCommitTags, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerGetCommitHeads, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerGetCommitIsHead, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerGetIsMergeInProgress,
      callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerGetCommitStatus, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerGetFileSizeFromCommit,
      callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerGetStashStatus, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerGetSubmodulePath, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerGetUpstream, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerInitRepo, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerListConflicts, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerListLocalBranches, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerListRemoteBranches, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerListRemoteBranchesWithStatus,
      callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerListRemotes, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerListStashes, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerListSubmodules, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerListSubmodulesWithStatus,
      callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerListTags, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerListRemoteTags, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerMerge, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerRebase, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerOpenRepo, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerPull, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerPush, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerPushTag, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerPushAllTags, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerRemoveRemote, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerRenameBranch, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerRenameTag, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerReset, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerRevert, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerSetDefaultRemote, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerSetUpstream, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerIgnoreFiles, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerStageFiles, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerStash, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerStatus, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerSubmoduleUpdate, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerUnstageFiles, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerUpdateRemote, callbackFunc));

  LOG_PERF(0, "Exit");
}

SvnRepoManager::~SvnRepoManager() {
  LOG_PERF(0, "Enter");

  const auto callbackFunc =
      std::bind(&SvnRepoManager::eventCallback, this, std::placeholders::_1);

  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerAbortMerge, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerResolveTreeConflict, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerAddRemote, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerAddSubmodule, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerAddTarget, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerEditTarget, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerDeleteTarget, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerSetDefaultTarget, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerAmend, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerBlame, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerApplyPatch, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerApplyStash, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerCheckoutBranch, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerCheckoutCommit, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerCherryPick, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerGetBranchHistory, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerCloneRepo, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerCloseRepo, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerCommitStaged, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerCreateBranchFromCommit,
      callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerCreatePatchFromCommit,
      callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerCreateTagFromCommit,
      callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerDeleteAllStashes, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerDeleteBranch, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerDeleteRemoteBranch, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerDeleteStash, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerDeleteSubmodule, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerDeleteTag, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerDeleteRemoteTag, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerDiffFiles, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerDiffFilesFromCommit,
      callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerDiscardFiles, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerFetchAll, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerFetchRemote, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerGetActiveBranch, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerGetCommitBranches, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerGetCommitTags, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerGetCommitHeads, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerGetCommitIsHead, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerGetIsMergeInProgress,
      callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerGetCommitStatus, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerGetFileSizeFromCommit,
      callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerGetStashStatus, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerGetSubmodulePath, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerGetUpstream, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerListConflicts, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerListLocalBranches, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerListRemoteBranches, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerListRemoteBranchesWithStatus,
      callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerListStashes, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerListSubmodules, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerListSubmodulesWithStatus,
      callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerListRemotes, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerListTags, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerListRemoteTags, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerMerge, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerRebase, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerOpenRepo, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerPull, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerPush, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerPushTag, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerPushAllTags, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerRemoveRemote, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerRenameBranch, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerRenameTag, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerReset, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerRevert, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerSetDefaultRemote, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerSetUpstream, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerIgnoreFiles, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerStageFiles, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerStash, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerStatus, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerSubmoduleUpdate, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerUnstageFiles, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerUpdateRemote, callbackFunc));

  LOG_PERF(0, "Exit");
}

void SvnRepoManager::start() {
  LOG_PERF(0, "Enter");

  _fsm.start();

  const EventManager::Event startedEvent(
      EventManager::EventType::ComponentStarted);
  _eventManager.send(startedEvent);

  LOG_PERF(0, "Exit");
}

void SvnRepoManager::stop() {
  LOG_PERF(0, "Enter");

  _fsm.stop();

  EventManager::Event stoppedEvent(EventManager::EventType::ComponentStopped);
  _eventManager.send(stoppedEvent);

  LOG_PERF(0, "Exit");
}

void SvnRepoManager::eventCallback(const EventManager::Event &event) {
  LOG_PERF(0, "Enter");

  const EventManager::EventType eventType = event.type();

  LOG_DEBUG("Received event {}", _eventManager.eventName(eventType));

  switch (eventType) {
  case EventManager::EventType::SvnRepoManagerAbortMerge: {
    const auto abortMergeCommand = event.data<SvnRepoAbortMergeCommand>();
    SvnRepoManagerEvent abortMergeEvent(EventType::AbortMerge);
    abortMergeEvent.addParam(QVariant::fromValue(abortMergeCommand));
    _fsm.send(abortMergeEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerResolveTreeConflict: {
    const auto resolveCommand =
        event.data<SvnRepoResolveTreeConflictCommand>();
    SvnRepoManagerEvent resolveEvent(EventType::ResolveTreeConflict);
    resolveEvent.addParam(QVariant::fromValue(resolveCommand));
    _fsm.send(resolveEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerAddRemote: {
    const auto addRemoteCommand = event.data<SvnRepoAddRemoteCommand>();
    SvnRepoManagerEvent addRemoteEvent(EventType::AddRemote);
    addRemoteEvent.addParam(QVariant::fromValue(addRemoteCommand));
    _fsm.send(addRemoteEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerAddSubmodule: {
    const auto addSubmoduleCommand = event.data<SvnRepoAddSubmoduleCommand>();
    SvnRepoManagerEvent addSubmoduleEvent(EventType::AddSubmodule);
    addSubmoduleEvent.addParam(QVariant::fromValue(addSubmoduleCommand));
    _fsm.send(addSubmoduleEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerAddTarget: {
    const auto addTargetCommand = event.data<SvnRepoAddTargetCommand>();
    SvnRepoManagerEvent addTargetEvent(EventType::AddTarget);
    addTargetEvent.addParam(QVariant::fromValue(addTargetCommand));
    _fsm.send(addTargetEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerEditTarget: {
    const auto editTargetCommand = event.data<SvnRepoEditTargetCommand>();
    SvnRepoManagerEvent editTargetEvent(EventType::EditTarget);
    editTargetEvent.addParam(QVariant::fromValue(editTargetCommand));
    _fsm.send(editTargetEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerDeleteTarget: {
    const auto deleteTargetCommand = event.data<SvnRepoDeleteTargetCommand>();
    SvnRepoManagerEvent deleteTargetEvent(EventType::DeleteTarget);
    deleteTargetEvent.addParam(QVariant::fromValue(deleteTargetCommand));
    _fsm.send(deleteTargetEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerSetDefaultTarget: {
    const auto setDefaultTargetCommand =
        event.data<SvnRepoSetDefaultTargetCommand>();
    SvnRepoManagerEvent setDefaultTargetEvent(EventType::SetDefaultTarget);
    setDefaultTargetEvent.addParam(
        QVariant::fromValue(setDefaultTargetCommand));
    _fsm.send(setDefaultTargetEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerAmend: {
    const auto amendCommand = event.data<SvnRepoAmendCommand>();
    SvnRepoManagerEvent amendEvent(EventType::Amend);
    amendEvent.addParam(QVariant::fromValue(amendCommand));
    _fsm.send(amendEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerBlame: {
    const auto blameCommand = event.data<SvnRepoBlameCommand>();
    SvnRepoManagerEvent blameEvent(EventType::Blame);
    blameEvent.addParam(QVariant::fromValue(blameCommand));
    _fsm.send(blameEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerApplyPatch: {
    const auto applyPatchCommand = event.data<SvnRepoApplyPatchCommand>();
    SvnRepoManagerEvent applyPatchEvent(EventType::ApplyPatch);
    applyPatchEvent.addParam(QVariant::fromValue(applyPatchCommand));
    _fsm.send(applyPatchEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerApplyStash: {
    const auto applyStashCommand = event.data<SvnRepoApplyStashCommand>();
    SvnRepoManagerEvent applyStashEvent(EventType::ApplyStash);
    applyStashEvent.addParam(QVariant::fromValue(applyStashCommand));
    _fsm.send(applyStashEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerCheckoutBranch: {
    const auto checkoutCommand = event.data<SvnRepoCheckoutBranchCommand>();
    SvnRepoManagerEvent checkoutBranchEvent(EventType::CheckoutBranch);
    checkoutBranchEvent.addParam(QVariant::fromValue(checkoutCommand));
    _fsm.send(checkoutBranchEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerCheckoutCommit: {
    const auto checkoutCommand = event.data<SvnRepoCheckoutCommitCommand>();
    SvnRepoManagerEvent checkoutCommitEvent(EventType::CheckoutCommit);
    checkoutCommitEvent.addParam(QVariant::fromValue(checkoutCommand));
    _fsm.send(checkoutCommitEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerCherryPick: {
    const auto cherryPickCommand = event.data<SvnRepoCherryPickCommand>();
    SvnRepoManagerEvent cherryPickEvent(EventType::CherryPick);
    cherryPickEvent.addParam(QVariant::fromValue(cherryPickCommand));
    _fsm.send(cherryPickEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerCloneRepo: {
    const auto cloneCommand = event.data<SvnRepoCloneCommand>();
    SvnRepoManagerEvent cloneRepoEvent(EventType::CloneRepo);
    cloneRepoEvent.addParam(QVariant::fromValue(cloneCommand));
    _fsm.send(cloneRepoEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerCloseRepo: {
    const auto closeCommand = event.data<SvnRepoCloseCommand>();
    SvnRepoManagerEvent closeRepoEvent(EventType::CloseRepo);
    closeRepoEvent.addParam(QVariant::fromValue(closeCommand));
    _fsm.send(closeRepoEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerCommitStaged: {
    const auto commitStagedCommand = event.data<SvnRepoCommitStagedCommand>();
    SvnRepoManagerEvent commitStagedRepoEvent(EventType::CommitStaged);
    commitStagedRepoEvent.addParam(QVariant::fromValue(commitStagedCommand));
    _fsm.send(commitStagedRepoEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerCreateBranchFromCommit: {
    const auto createBranchFromCommitCommand =
        event.data<SvnRepoCreateBranchFromCommitCommand>();
    SvnRepoManagerEvent createBranchFromCommitEvent(
        EventType::CreateBranchFromCommit);
    createBranchFromCommitEvent.addParam(
        QVariant::fromValue(createBranchFromCommitCommand));
    _fsm.send(createBranchFromCommitEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerCreatePatchFromCommit: {
    const auto createPatchFromCommitCommand =
        event.data<SvnRepoCreatePatchFromCommitCommand>();
    SvnRepoManagerEvent createPatchFromCommitEvent(
        EventType::CreatePatchFromCommit);
    createPatchFromCommitEvent.addParam(
        QVariant::fromValue(createPatchFromCommitCommand));
    _fsm.send(createPatchFromCommitEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerCreateTagFromCommit: {
    const auto createTagFromCommitCommand =
        event.data<SvnRepoCreateTagFromCommitCommand>();
    SvnRepoManagerEvent createTagFromCommitEvent(
        EventType::CreateTagFromCommit);
    createTagFromCommitEvent.addParam(
        QVariant::fromValue(createTagFromCommitCommand));
    _fsm.send(createTagFromCommitEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerDeleteBranch: {
    const auto deleteBranchCommand = event.data<SvnRepoDeleteBranchCommand>();
    SvnRepoManagerEvent deleteBranchEvent(EventType::DeleteBranch);
    deleteBranchEvent.addParam(QVariant::fromValue(deleteBranchCommand));
    _fsm.send(deleteBranchEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerDeleteRemoteBranch: {
    const auto deleteRemoteBranchCommand =
        event.data<SvnRepoDeleteRemoteBranchCommand>();
    SvnRepoManagerEvent deleteRemoteBranchEvent(EventType::DeleteRemoteBranch);
    deleteRemoteBranchEvent.addParam(
        QVariant::fromValue(deleteRemoteBranchCommand));
    _fsm.send(deleteRemoteBranchEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerDeleteStash: {
    const auto deleteStashCommand = event.data<SvnRepoDeleteStashCommand>();
    SvnRepoManagerEvent deleteStashEvent(EventType::DeleteStash);
    deleteStashEvent.addParam(QVariant::fromValue(deleteStashCommand));
    _fsm.send(deleteStashEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerDeleteAllStashes: {
    const auto deleteAllStashesCommand =
        event.data<SvnRepoDeleteAllStashesCommand>();
    SvnRepoManagerEvent deleteAllStashesEvent(EventType::DeleteAllStashes);
    deleteAllStashesEvent.addParam(
        QVariant::fromValue(deleteAllStashesCommand));
    _fsm.send(deleteAllStashesEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerDeleteSubmodule: {
    const auto deleteSubmoduleCommand =
        event.data<SvnRepoDeleteSubmoduleCommand>();
    SvnRepoManagerEvent deleteSubmoduleEvent(EventType::DeleteSubmodule);
    deleteSubmoduleEvent.addParam(QVariant::fromValue(deleteSubmoduleCommand));
    _fsm.send(deleteSubmoduleEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerDeleteTag: {
    const auto deleteTagCommand = event.data<SvnRepoDeleteTagCommand>();
    SvnRepoManagerEvent deleteTagEvent(EventType::DeleteTag);
    deleteTagEvent.addParam(QVariant::fromValue(deleteTagCommand));
    _fsm.send(deleteTagEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerDeleteRemoteTag: {
    const auto deleteRemoteTagCommand =
        event.data<SvnRepoDeleteRemoteTagCommand>();
    SvnRepoManagerEvent deleteRemoteTagEvent(EventType::DeleteRemoteTag);
    deleteRemoteTagEvent.addParam(QVariant::fromValue(deleteRemoteTagCommand));
    _fsm.send(deleteRemoteTagEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerDiffFiles: {
    const auto diffFilesCommand = event.data<SvnRepoDiffFilesCommand>();
    SvnRepoManagerEvent diffFilesEvent(EventType::DiffFiles);
    diffFilesEvent.addParam(QVariant::fromValue(diffFilesCommand));
    _fsm.send(diffFilesEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerDiffFilesFromCommit: {
    const auto diffFilesFromCommitCommand =
        event.data<SvnRepoDiffFilesFromCommitCommand>();
    SvnRepoManagerEvent diffFilesFromCommitEvent(
        EventType::DiffFilesFromCommit);
    diffFilesFromCommitEvent.addParam(
        QVariant::fromValue(diffFilesFromCommitCommand));
    _fsm.send(diffFilesFromCommitEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerDiscardFiles: {
    const auto discardFilesCommand = event.data<SvnRepoDiscardFilesCommand>();
    SvnRepoManagerEvent discardFilesEvent(EventType::DiscardFiles);
    discardFilesEvent.addParam(QVariant::fromValue(discardFilesCommand));
    _fsm.send(discardFilesEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerFetchAll: {
    const auto fetchAllCommand = event.data<SvnRepoFetchAllCommand>();
    SvnRepoManagerEvent fetchAllEvent(EventType::FetchAll);
    fetchAllEvent.addParam(QVariant::fromValue(fetchAllCommand));
    _fsm.send(fetchAllEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerFetchRemote: {
    const auto fetchRemoteCommand = event.data<SvnRepoFetchRemoteCommand>();
    SvnRepoManagerEvent fetchRemoteEvent(EventType::FetchRemote);
    fetchRemoteEvent.addParam(QVariant::fromValue(fetchRemoteCommand));
    _fsm.send(fetchRemoteEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerGetActiveBranch: {
    const auto getActiveBranchCommand =
        event.data<SvnRepoGetActiveBranchCommand>();
    SvnRepoManagerEvent getActiveBranchEvent(EventType::GetActiveBranch);
    getActiveBranchEvent.addParam(QVariant::fromValue(getActiveBranchCommand));
    _fsm.send(getActiveBranchEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerGetBranchHistory: {
    const auto branchHistoryCommand =
        event.data<SvnRepoGetBranchHistoryCommand>();
    SvnRepoManagerEvent historyEvent(EventType::GetBranchHistory);
    historyEvent.addParam(QVariant::fromValue(branchHistoryCommand));
    _fsm.send(historyEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerGetCommitBranches: {
    const auto getCommitBranchesCommand =
        event.data<SvnRepoGetCommitBranchesCommand>();
    SvnRepoManagerEvent getCommitBranchesEvent(EventType::GetCommitBranches);
    getCommitBranchesEvent.addParam(
        QVariant::fromValue(getCommitBranchesCommand));
    _fsm.send(getCommitBranchesEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerGetCommitTags: {
    const auto getCommitTagsCommand = event.data<SvnRepoGetCommitTagsCommand>();
    SvnRepoManagerEvent getCommitTagsEvent(EventType::GetCommitTags);
    getCommitTagsEvent.addParam(QVariant::fromValue(getCommitTagsCommand));
    _fsm.send(getCommitTagsEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerGetCommitHeads: {
    const auto getCommitHeadsCommand =
        event.data<SvnRepoGetCommitHeadsCommand>();
    SvnRepoManagerEvent getCommitHeadsEvent(EventType::GetCommitHeads);
    getCommitHeadsEvent.addParam(QVariant::fromValue(getCommitHeadsCommand));
    _fsm.send(getCommitHeadsEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerGetCommitIsHead: {
    const auto getCommitIsHeadCommand =
        event.data<SvnRepoGetCommitIsHeadCommand>();
    SvnRepoManagerEvent getCommitIsHeadEvent(EventType::GetCommitIsHead);
    getCommitIsHeadEvent.addParam(QVariant::fromValue(getCommitIsHeadCommand));
    _fsm.send(getCommitIsHeadEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerGetIsMergeInProgress: {
    const auto getIsMergeInProgressCommand =
        event.data<SvnRepoGetIsMergeInProgressCommand>();
    SvnRepoManagerEvent getIsMergeInProgressEvent(
        EventType::GetIsMergeInProgress);
    getIsMergeInProgressEvent.addParam(
        QVariant::fromValue(getIsMergeInProgressCommand));
    _fsm.send(getIsMergeInProgressEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerGetCommitStatus: {
    const auto getCommitStatusCommand =
        event.data<SvnRepoGetCommitStatusCommand>();
    SvnRepoManagerEvent getCommitStatusEvent(EventType::GetCommitStatus);
    getCommitStatusEvent.addParam(QVariant::fromValue(getCommitStatusCommand));
    _fsm.send(getCommitStatusEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerGetFileSizeFromCommit: {
    const auto getFileSizeFromCommand =
        event.data<SvnRepoGetFileSizeFromCommitCommand>();
    SvnRepoManagerEvent getFileSizeFromCommitEvent(
        EventType::GetFileSizeFromCommit);
    getFileSizeFromCommitEvent.addParam(
        QVariant::fromValue(getFileSizeFromCommand));
    _fsm.send(getFileSizeFromCommitEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerGetStashStatus: {
    const auto getSStashStatusCommand =
        event.data<SvnRepoGetStashStatusCommand>();
    SvnRepoManagerEvent getStashStatusEvent(EventType::GetStashStatus);
    getStashStatusEvent.addParam(QVariant::fromValue(getSStashStatusCommand));
    _fsm.send(getStashStatusEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerGetSubmodulePath: {
    const auto getSubmodulePathCommand =
        event.data<SvnRepoGetSubmodulePathCommand>();
    SvnRepoManagerEvent getSubmodulePathEvent(EventType::GetSubmodulePath);
    getSubmodulePathEvent.addParam(
        QVariant::fromValue(getSubmodulePathCommand));
    _fsm.send(getSubmodulePathEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerGetUpstream: {
    const auto getUpstreamCommand = event.data<SvnRepoGetUpstreamCommand>();
    SvnRepoManagerEvent getUpstreamEvent(EventType::GetUpstream);
    getUpstreamEvent.addParam(QVariant::fromValue(getUpstreamCommand));
    _fsm.send(getUpstreamEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerInitRepo: {
    const auto initRepoCommand = event.data<SvnRepoInitRepoCommand>();
    SvnRepoManagerEvent initRepoEvent(EventType::Init);
    initRepoEvent.addParam(QVariant::fromValue(initRepoCommand));
    _fsm.send(initRepoEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerListConflicts: {
    const auto listConflictsCommand = event.data<SvnRepoListConflictsCommand>();
    SvnRepoManagerEvent listConflictsEvent(EventType::ListConflicts);
    listConflictsEvent.addParam(QVariant::fromValue(listConflictsCommand));
    _fsm.send(listConflictsEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerListLocalBranches: {
    const auto listLocalCommand = event.data<SvnRepoListLocalBranchesCommand>();
    SvnRepoManagerEvent listLocalEvent(EventType::ListLocalBranches);
    listLocalEvent.addParam(QVariant::fromValue(listLocalCommand));
    _fsm.send(listLocalEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerListRemoteBranches: {
    const auto listRemoteCommand =
        event.data<SvnRepoListRemoteBranchesCommand>();
    SvnRepoManagerEvent listRemoteEvent(EventType::ListRemoteBranches);
    listRemoteEvent.addParam(QVariant::fromValue(listRemoteCommand));
    _fsm.send(listRemoteEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerListRemoteBranchesWithStatus: {
    const auto listRemoteStatusCommand =
        event.data<SvnRepoListRemoteBranchesWithStatusCommand>();
    SvnRepoManagerEvent listRemoteStatusEvent(
        EventType::ListRemoteBranchesWithStatus);
    listRemoteStatusEvent.addParam(
        QVariant::fromValue(listRemoteStatusCommand));
    _fsm.send(listRemoteStatusEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerListRemotes: {
    const auto listRemotesCommand = event.data<SvnRepoListRemotesCommand>();
    SvnRepoManagerEvent listRemotesEvent(EventType::ListRemotes);
    listRemotesEvent.addParam(QVariant::fromValue(listRemotesCommand));
    _fsm.send(listRemotesEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerListStashes: {
    const auto listStashesCommand = event.data<SvnRepoListStashesCommand>();
    SvnRepoManagerEvent listStashesEvent(EventType::ListStashes);
    listStashesEvent.addParam(QVariant::fromValue(listStashesCommand));
    _fsm.send(listStashesEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerListSubmodules: {
    const auto listSubmodulesCommand =
        event.data<SvnRepoListSubmodulesCommand>();
    SvnRepoManagerEvent listSubmodulesEvent(EventType::ListSubmodules);
    listSubmodulesEvent.addParam(QVariant::fromValue(listSubmodulesCommand));
    _fsm.send(listSubmodulesEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerListSubmodulesWithStatus: {
    const auto listSubmodulesWithStatusCommand =
        event.data<SvnRepoListSubmodulesWithStatusCommand>();
    SvnRepoManagerEvent listSubmodulesWithStatusEvent(
        EventType::ListSubmodulesWithStatus);
    listSubmodulesWithStatusEvent.addParam(
        QVariant::fromValue(listSubmodulesWithStatusCommand));
    _fsm.send(listSubmodulesWithStatusEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerListTags: {
    const auto listTagsCommand = event.data<SvnRepoListTagsCommand>();
    SvnRepoManagerEvent listTagsEvent(EventType::ListTags);
    listTagsEvent.addParam(QVariant::fromValue(listTagsCommand));
    _fsm.send(listTagsEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerListRemoteTags: {
    const auto listRemoteTagsCommand =
        event.data<SvnRepoListRemoteTagsCommand>();
    SvnRepoManagerEvent listRemoteTagsEvent(EventType::ListRemoteTags);
    listRemoteTagsEvent.addParam(QVariant::fromValue(listRemoteTagsCommand));
    _fsm.send(listRemoteTagsEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerMerge: {
    const auto mergeCommand = event.data<SvnRepoMergeCommand>();
    SvnRepoManagerEvent mergeEvent(EventType::Merge);
    mergeEvent.addParam(QVariant::fromValue(mergeCommand));
    _fsm.send(mergeEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerRebase: {
    const auto rebaseCommand = event.data<SvnRepoRebaseCommand>();
    SvnRepoManagerEvent rebaseEvent(EventType::Rebase);
    rebaseEvent.addParam(QVariant::fromValue(rebaseCommand));
    _fsm.send(rebaseEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerOpenRepo: {
    const auto openRepoCommand = event.data<SvnRepoOpenCommand>();
    SvnRepoManagerEvent openRepoEvent(EventType::OpenRepo);
    openRepoEvent.addParam(QVariant::fromValue(openRepoCommand));
    _fsm.send(openRepoEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerPull: {
    const auto pullCommand = event.data<SvnRepoPullCommand>();
    SvnRepoManagerEvent pullEvent(EventType::Pull);
    pullEvent.addParam(QVariant::fromValue(pullCommand));
    _fsm.send(pullEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerPush: {
    const auto pushCommand = event.data<SvnRepoPushCommand>();
    SvnRepoManagerEvent pushEvent(EventType::Push);
    pushEvent.addParam(QVariant::fromValue(pushCommand));
    _fsm.send(pushEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerPushTag: {
    const auto pushTagCommand = event.data<SvnRepoPushTagCommand>();
    SvnRepoManagerEvent pushTagEvent(EventType::PushTag);
    pushTagEvent.addParam(QVariant::fromValue(pushTagCommand));
    _fsm.send(pushTagEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerPushAllTags: {
    const auto pushAllTagsCommand = event.data<SvnRepoPushAllTagsCommand>();
    SvnRepoManagerEvent pushAllTagsEvent(EventType::PushAllTags);
    pushAllTagsEvent.addParam(QVariant::fromValue(pushAllTagsCommand));
    _fsm.send(pushAllTagsEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerRemoveRemote: {
    const auto removeRemoteCommand = event.data<SvnRepoRemoveRemoteCommand>();
    SvnRepoManagerEvent removeRemoteEvent(EventType::RemoveRemote);
    removeRemoteEvent.addParam(QVariant::fromValue(removeRemoteCommand));
    _fsm.send(removeRemoteEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerRenameBranch: {
    const auto renameBranchCommand = event.data<SvnRepoRenameBranchCommand>();
    SvnRepoManagerEvent renameBranchEvent(EventType::RenameBranch);
    renameBranchEvent.addParam(QVariant::fromValue(renameBranchCommand));
    _fsm.send(renameBranchEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerRenameTag: {
    const auto renameTagCommand = event.data<SvnRepoRenameTagCommand>();
    SvnRepoManagerEvent renameTagEvent(EventType::RenameTag);
    renameTagEvent.addParam(QVariant::fromValue(renameTagCommand));
    _fsm.send(renameTagEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerReset: {
    const auto resetCommand = event.data<SvnRepoResetCommand>();
    SvnRepoManagerEvent resetEvent(EventType::Reset);
    resetEvent.addParam(QVariant::fromValue(resetCommand));
    _fsm.send(resetEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerRevert: {
    const auto revertCommand = event.data<SvnRepoRevertCommand>();
    SvnRepoManagerEvent revertEvent(EventType::Revert);
    revertEvent.addParam(QVariant::fromValue(revertCommand));
    _fsm.send(revertEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerSetDefaultRemote: {
    const auto setDefaultRemoteCommand =
        event.data<SvnRepoSetDefaultRemoteCommand>();
    SvnRepoManagerEvent setDefaultRemoteEvent(EventType::SetDefaultRemote);
    setDefaultRemoteEvent.addParam(
        QVariant::fromValue(setDefaultRemoteCommand));
    _fsm.send(setDefaultRemoteEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerSetUpstream: {
    const auto setUpstreamCommand = event.data<SvnRepoSetUpstreamCommand>();
    SvnRepoManagerEvent setUpstreamEvent(EventType::SetUpstream);
    setUpstreamEvent.addParam(QVariant::fromValue(setUpstreamCommand));
    _fsm.send(setUpstreamEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerIgnoreFiles: {
    const auto ignoreFilesCommand = event.data<SvnRepoIgnoreFilesCommand>();
    SvnRepoManagerEvent ignoreFilesRepoEvent(EventType::IgnoreFiles);
    ignoreFilesRepoEvent.addParam(QVariant::fromValue(ignoreFilesCommand));
    _fsm.send(ignoreFilesRepoEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerStageFiles: {
    const auto stageFilesCommand = event.data<SvnRepoStageFilesCommand>();
    SvnRepoManagerEvent stageFilesRepoEvent(EventType::StageFiles);
    stageFilesRepoEvent.addParam(QVariant::fromValue(stageFilesCommand));
    _fsm.send(stageFilesRepoEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerStash: {
    const auto stashCommand = event.data<SvnRepoStashCommand>();
    SvnRepoManagerEvent stashEvent(EventType::Stash);
    stashEvent.addParam(QVariant::fromValue(stashCommand));
    _fsm.send(stashEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerStatus: {
    const auto statusCommand = event.data<SvnRepoStatusCommand>();
    SvnRepoManagerEvent statusRepoEvent(EventType::Status);
    statusRepoEvent.addParam(QVariant::fromValue(statusCommand));
    _fsm.send(statusRepoEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerSubmoduleUpdate: {
    const auto submoduleUpdateCommand =
        event.data<SvnRepoSubmoduleUpdateCommand>();
    SvnRepoManagerEvent submoduleUpdateEvent(EventType::SubmoduleUpdate);
    submoduleUpdateEvent.addParam(QVariant::fromValue(submoduleUpdateCommand));
    _fsm.send(submoduleUpdateEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerUnstageFiles: {
    const auto unstageFilesCommand = event.data<SvnRepoUnstageFilesCommand>();
    SvnRepoManagerEvent unstageFilesRepoEvent(EventType::UnstageFiles);
    unstageFilesRepoEvent.addParam(QVariant::fromValue(unstageFilesCommand));
    _fsm.send(unstageFilesRepoEvent);
    break;
  }
  case EventManager::EventType::SvnRepoManagerUpdateRemote: {
    const auto updateRemoteCommand = event.data<SvnRepoUpdateRemoteCommand>();
    SvnRepoManagerEvent updateRemoteEvent(EventType::UpdateRemote);
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

} // namespace SvnRepoManager
