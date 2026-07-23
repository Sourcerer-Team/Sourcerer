#include "EventManager.h"

#include <General.h>
#include <Log.h>

#include <QDateTime>

#include <functional>

#define THREAD_ID "EventManager"

namespace EventManager {

std::atomic<int> EventManager::_waitId = 0;

static bool callbackComparator(EventCallback &callback0,
                               const EventCallback &callback1) {
  return (callback1._callback.target_type() ==
          callback0._callback.target_type()) &&
         (callback1._callback.target<void (*)(Event)>() ==
          callback0._callback.target<void (*)(Event)>()) &&
         (callback1._receiver == callback0._receiver);
}

EventManager::EventManager() : _thread(nullptr) {
  start();
}

EventManager::~EventManager() {
  stop();
}

EventManager &EventManager::getInstance() {
  static EventManager instance;
  return instance;
}

void EventManager::threadLoop() {
  General::setThreadName(THREAD_ID);

  while (true) {

    const EventRecord record = _eventsQueue.pop();

    if (record._type == EventRecordType::Quit) {
      break;
    }

    const auto predicate = std::bind(
        &callbackComparator, record._callbackRecord, std::placeholders::_1);

    _queueLocker.lock();

    const auto itSubscribersBegin = _subscribers[record._eventType].begin();
    const auto itSubscribersEnd = _subscribers[record._eventType].end();

    const auto it =
        std::find_if(itSubscribersBegin, itSubscribersEnd, predicate);

    if (it == itSubscribersEnd) {
      _queueLocker.unlock();
      LOG_DEBUG("Skipping event {}", eventName(record._eventType));
      continue;
    }

    _queueLocker.unlock();

    const qint64 start = QDateTime::currentMSecsSinceEpoch();

    LOG_DEBUG("Dispatching event {}", eventName(record._eventType));

    record._callback();

    const qint64 end = QDateTime::currentMSecsSinceEpoch();

    LOG_PERF(end - start, "Event execution duration");
  }
}

void EventManager::subscribe(EventCallback callback) {
  LOCKGUARD(_queueLocker);

  LOG_DEBUG("Subscribing to event {}", eventName(callback._type));

  _subscribers[callback._type].push_back(callback);
}

void EventManager::unsubscribe(EventCallback callback) {
  LOCKGUARD(_queueLocker);

  LOG_DEBUG("Unsubscribing from event {}", eventName(callback._type));

  const auto predicate =
      std::bind(&callbackComparator, callback, std::placeholders::_1);

  const auto itNewEnd =
      std::remove_if(_subscribers[callback._type].begin(),
                     _subscribers[callback._type].end(), predicate);

  LOG_DEBUG("Subscription removed: {}",
            itNewEnd != _subscribers[callback._type].end());

  _subscribers[callback._type].erase(itNewEnd,
                                     _subscribers[callback._type].end());
}

int EventManager::startWait() {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_queueLocker);

  const int index = _waitId++;

  const auto &queue = _waitEventsQueue[index];

  LOG_PERF(0, "Exit");

  return index;
}

std::tuple<bool, Event>
EventManager::waitOneOf(int token, const std::set<EventType> &eventTypes,
                        std::function<bool(Event)> predicate, ulong timeout) {
  LOG_PERF(0, "Enter");

  LOG_DEBUG("waitOneOf: token={}, timeout={}, eventTypes count={}", token,
            timeout, eventTypes.size());

  const int index = token;

  _queueLocker.lock();

  auto &queue = _waitEventsQueue[index];

  _queueLocker.unlock();

  const WaitEventRecord defaultEventRecord(
      {EventRecordType::Quit, {EventType::Null}});

  Event resultEvent(EventType::Null);

  const qint64 start = QDateTime::currentMSecsSinceEpoch();
  qint64 end = start;

  bool success = false;
  bool quit = false;

  do {
    const WaitEventRecord eventRecord = queue.pop(timeout, defaultEventRecord);

    switch (eventRecord._type) {
    case EventRecordType::Send: {
      resultEvent = eventRecord._event;
      const bool found = eventTypes.count(resultEvent.type()) > 0;
      if (predicate)
        success = found && predicate(resultEvent);
      else
        success = found;

      break;
    }
    case EventRecordType::Quit: {
      quit = true;
      break;
    }
    default: {
      LOG_WARN("Internal error. Unknown command type.");
      quit = true;
      break;
    }
    }

    end = QDateTime::currentMSecsSinceEpoch();

  } while (!quit && !success && (end - start) < timeout);

  LOG_DEBUG("waitOneOf: token={}, success={}, elapsed={}ms", token, success,
            (end - start));

  _queueLocker.lock();

  _waitEventsQueue.erase(index);

  _queueLocker.unlock();

  LOG_PERF(0, "Exit");

  return std::make_tuple(success, resultEvent);
}

std::tuple<bool, Event>
EventManager::waitForAnswer(int token, const QString &uuid, ulong timeout) {
  LOG_PERF(0, "Enter");

  static const std::set<EventType> types{EventType::Error, EventType::Success};

  LOG_PERF(0, "Exit");

  return waitForAnswer(token, uuid, types, timeout);
}

std::tuple<bool, Event>
EventManager::waitForAnswer(int token, const QString &uuid,
                            const std::set<EventType> &types, ulong timeout) {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");

  return waitOneOf(
      token, types, [&](Event event) -> bool { return event.uuid() == uuid; },
      timeout);
}

QString EventManager::eventName(const EventType type) const {
  switch (type) {
  case EventType::Quit:
    return "Quit";
  case EventType::Error:
    return "Error";
  case EventType::Success:
    return "Success";
  case EventType::ComponentStarted:
    return "ComponentStarted";
  case EventType::ComponentStopped:
    return "ComponentStopped";
  case EventType::NewsTrackerNews:
    return "NewsTrackerNews";
  case EventType::GitRepoManagerAbortMerge:
    return "GitRepoManagerAbortMerge";
  case EventType::GitRepoManagerAbortMergeResult:
    return "GitRepoManagerAbortMergeResult";
  case EventType::GitRepoManagerAbortMergeError:
    return "GitRepoManagerAbortMergeError";
  case EventType::GitRepoManagerAddRemote:
    return "GitRepoManagerAddRemote";
  case EventType::GitRepoManagerAddRemoteResult:
    return "GitRepoManagerAddRemoteResult";
  case EventType::GitRepoManagerAddRemoteError:
    return "GitRepoManagerAddRemoteError";
  case EventType::GitRepoManagerAddSubmodule:
    return "GitRepoManagerAddSubmodule";
  case EventType::GitRepoManagerAddSubmoduleResult:
    return "GitRepoManagerAddSubmoduleResult";
  case EventType::GitRepoManagerAddSubmoduleError:
    return "GitRepoManagerAddSubmoduleError";
  case EventType::GitRepoManagerAddTarget:
    return "GitRepoManagerAddTarget";
  case EventType::GitRepoManagerAddTargetResult:
    return "GitRepoManagerAddTargetResult";
  case EventType::GitRepoManagerAddTargetError:
    return "GitRepoManagerAddTargetError";
  case EventType::GitRepoManagerEditTarget:
    return "GitRepoManagerEditTarget";
  case EventType::GitRepoManagerEditTargetResult:
    return "GitRepoManagerEditTargetResult";
  case EventType::GitRepoManagerEditTargetError:
    return "GitRepoManagerEditTargetError";
  case EventType::GitRepoManagerDeleteTarget:
    return "GitRepoManagerDeleteTarget";
  case EventType::GitRepoManagerDeleteTargetResult:
    return "GitRepoManagerDeleteTargetResult";
  case EventType::GitRepoManagerDeleteTargetError:
    return "GitRepoManagerDeleteTargetError";
  case EventType::GitRepoManagerSetDefaultTarget:
    return "GitRepoManagerSetDefaultTarget";
  case EventType::GitRepoManagerSetDefaultTargetResult:
    return "GitRepoManagerSetDefaultTargetResult";
  case EventType::GitRepoManagerSetDefaultTargetError:
    return "GitRepoManagerSetDefaultTargetError";
  case EventType::GitRepoManagerAmend:
    return "GitRepoManagerAmend";
  case EventType::GitRepoManagerAmendResult:
    return "GitRepoManagerAmendResult";
  case EventType::GitRepoManagerAmendError:
    return "GitRepoManagerAmendError";
  case EventType::GitRepoManagerApplyPatch:
    return "GitRepoManagerApplyPatch";
  case EventType::GitRepoManagerApplyPatchResult:
    return "GitRepoManagerApplyPatchResult";
  case EventType::GitRepoManagerApplyPatchError:
    return "GitRepoManagerApplyPatchError";
  case EventType::GitRepoManagerApplyStash:
    return "GitRepoManagerApplyStash";
  case EventType::GitRepoManagerApplyStashResult:
    return "GitRepoManagerApplyStashResult";
  case EventType::GitRepoManagerApplyStashError:
    return "GitRepoManagerApplyStashError";
  case EventType::GitRepoManagerCheckoutBranch:
    return "GitRepoManagerCheckoutBranch";
  case EventType::GitRepoManagerCheckoutBranchResult:
    return "GitRepoManagerCheckoutBranchResult";
  case EventType::GitRepoManagerCheckoutBranchError:
    return "GitRepoManagerCheckoutBranchError";
  case EventType::GitRepoManagerCheckoutCommit:
    return "GitRepoManagerCheckoutCommit";
  case EventType::GitRepoManagerCheckoutCommitResult:
    return "GitRepoManagerCheckoutCommitResult";
  case EventType::GitRepoManagerCheckoutCommitError:
    return "GitRepoManagerCheckoutCommitError";
  case EventType::GitRepoManagerCherryPick:
    return "GitRepoManagerCherryPick";
  case EventType::GitRepoManagerCherryPickResult:
    return "GitRepoManagerCherryPickResult";
  case EventType::GitRepoManagerCherryPickError:
    return "GitRepoManagerCherryPickError";
  case EventType::GitRepoManagerCloneRepo:
    return "GitRepoManagerCloneRepo";
  case EventType::GitRepoManagerCloneRepoResult:
    return "GitRepoManagerCloneRepoResult";
  case EventType::GitRepoManagerCloneRepoError:
    return "GitRepoManagerCloneRepoError";
  case EventType::GitRepoManagerCloneRepoProgress:
    return "GitRepoManagerCloneRepoProgress";
  case EventType::GitRepoManagerCloseRepo:
    return "GitRepoManagerCloseRepo";
  case EventType::GitRepoManagerCloseRepoResult:
    return "GitRepoManagerCloseRepoResult";
  case EventType::GitRepoManagerCloseRepoError:
    return "GitRepoManagerCloseRepoError";
  case EventType::GitRepoManagerCommitStaged:
    return "GitRepoManagerCommitStaged";
  case EventType::GitRepoManagerCommitStagedResult:
    return "GitRepoManagerCommitStagedResult";
  case EventType::GitRepoManagerCommitStagedError:
    return "GitRepoManagerCommitStagedError";
  case EventType::GitRepoManagerCreateBranchFromCommit:
    return "GitRepoManagerCreateBranchFromCommit";
  case EventType::GitRepoManagerCreateBranchFromCommitResult:
    return "GitRepoManagerCreateBranchFromCommitResult";
  case EventType::GitRepoManagerCreateBranchFromCommitError:
    return "GitRepoManagerCreateBranchFromCommitError";
  case EventType::GitRepoManagerCreatePatchFromCommit:
    return "GitRepoManagerCreatePatchFromCommit";
  case EventType::GitRepoManagerCreatePatchFromCommitResult:
    return "GitRepoManagerCreatePatchFromCommitResult";
  case EventType::GitRepoManagerCreatePatchFromCommitError:
    return "GitRepoManagerCreatePatchFromCommitError";
  case EventType::GitRepoManagerCreateTagFromCommit:
    return "GitRepoManagerCreateTagFromCommit";
  case EventType::GitRepoManagerCreateTagFromCommitResult:
    return "GitRepoManagerCreateTagFromCommitResult";
  case EventType::GitRepoManagerCreateTagFromCommitError:
    return "GitRepoManagerCreateTagFromCommitError";
  case EventType::GitRepoManagerDeleteBranch:
    return "GitRepoManagerDeleteBranch";
  case EventType::GitRepoManagerDeleteBranchResult:
    return "GitRepoManagerDeleteBranchResult";
  case EventType::GitRepoManagerDeleteBranchError:
    return "GitRepoManagerDeleteBranchError";
  case EventType::GitRepoManagerDeleteRemoteBranch:
    return "GitRepoManagerDeleteRemoteBranch";
  case EventType::GitRepoManagerDeleteRemoteBranchResult:
    return "GitRepoManagerDeleteRemoteBranchResult";
  case EventType::GitRepoManagerDeleteRemoteBranchError:
    return "GitRepoManagerDeleteRemoteBranchError";
  case EventType::GitRepoManagerDeleteStash:
    return "GitRepoManagerDeleteStash";
  case EventType::GitRepoManagerDeleteStashResult:
    return "GitRepoManagerDeleteStashResult";
  case EventType::GitRepoManagerDeleteStashError:
    return "GitRepoManagerDeleteStashError";
  case EventType::GitRepoManagerDeleteAllStashes:
    return "GitRepoManagerDeleteAllStashes";
  case EventType::GitRepoManagerDeleteAllStashesResult:
    return "GitRepoManagerDeleteAllStashesResult";
  case EventType::GitRepoManagerDeleteAllStashesError:
    return "GitRepoManagerDeleteAllStashesError";
  case EventType::GitRepoManagerDeleteSubmodule:
    return "GitRepoManagerDeleteSubmodule";
  case EventType::GitRepoManagerDeleteSubmoduleResult:
    return "GitRepoManagerDeleteSubmoduleResult";
  case EventType::GitRepoManagerDeleteSubmoduleError:
    return "GitRepoManagerDeleteSubmoduleError";
  case EventType::GitRepoManagerDeleteTag:
    return "GitRepoManagerDeleteTag";
  case EventType::GitRepoManagerDeleteTagResult:
    return "GitRepoManagerDeleteTagResult";
  case EventType::GitRepoManagerDeleteTagError:
    return "GitRepoManagerDeleteTagError";
  case EventType::GitRepoManagerDeleteRemoteTag:
    return "GitRepoManagerDeleteRemoteTag";
  case EventType::GitRepoManagerDeleteRemoteTagResult:
    return "GitRepoManagerDeleteRemoteTagResult";
  case EventType::GitRepoManagerDeleteRemoteTagError:
    return "GitRepoManagerDeleteRemoteTagError";
  case EventType::GitRepoManagerDiffFiles:
    return "GitRepoManagerDiffFiles";
  case EventType::GitRepoManagerDiffFilesResult:
    return "GitRepoManagerDiffFilesResult";
  case EventType::GitRepoManagerDiffFilesError:
    return "GitRepoManagerDiffFilesError";
  case EventType::GitRepoManagerDiffFilesFromCommit:
    return "GitRepoManagerDiffFilesFromCommit";
  case EventType::GitRepoManagerDiffFilesFromCommitResult:
    return "GitRepoManagerDiffFilesFromCommitResult";
  case EventType::GitRepoManagerDiffFilesFromCommitError:
    return "GitRepoManagerDiffFilesFromCommitError";
  case EventType::GitRepoManagerDiscardFiles:
    return "GitRepoManagerDiscardFiles";
  case EventType::GitRepoManagerDiscardFilesResult:
    return "GitRepoManagerDiscardFilesResult";
  case EventType::GitRepoManagerDiscardFilesError:
    return "GitRepoManagerDiscardFilesError";
  case EventType::GitRepoManagerGetFileSizeFromCommit:
    return "GitRepoManagerGetFileSizeFromCommit";
  case EventType::GitRepoManagerGetFileSizeFromCommitResult:
    return "GitRepoManagerGetFileSizeFromCommitResult";
  case EventType::GitRepoManagerGetFileSizeFromCommitError:
    return "GitRepoManagerGetFileSizeFromCommitError";
  case EventType::GitRepoManagerGetActiveBranch:
    return "GitRepoManagerGetActiveBranch";
  case EventType::GitRepoManagerGetActiveBranchResult:
    return "GitRepoManagerGetActiveBranchResult";
  case EventType::GitRepoManagerGetActiveBranchError:
    return "GitRepoManagerGetActiveBranchError";
  case EventType::GitRepoManagerGetBranchHistory:
    return "GitRepoManagerGetBranchHistory";
  case EventType::GitRepoManagerGetBranchHistoryResult:
    return "GitRepoManagerGetBranchHistoryResult";
  case EventType::GitRepoManagerGetBranchHistoryError:
    return "GitRepoManagerGetBranchHistoryError";
  case EventType::GitRepoManagerGetCommitBranches:
    return "GitRepoManagerGetCommitBranches";
  case EventType::GitRepoManagerGetCommitBranchesResult:
    return "GitRepoManagerGetCommitBranchesResult";
  case EventType::GitRepoManagerGetCommitBranchesError:
    return "GitRepoManagerGetCommitBranchesError";
  case EventType::GitRepoManagerGetCommitTags:
    return "GitRepoManagerGetCommitTags";
  case EventType::GitRepoManagerGetCommitTagsResult:
    return "GitRepoManagerGetCommitTagsResult";
  case EventType::GitRepoManagerGetCommitTagsError:
    return "GitRepoManagerGetCommitTagsError";
  case EventType::GitRepoManagerGetCommitHeads:
    return "GitRepoManagerGetCommitHeads";
  case EventType::GitRepoManagerGetCommitHeadsResult:
    return "GitRepoManagerGetCommitHeadsResult";
  case EventType::GitRepoManagerGetCommitHeadsError:
    return "GitRepoManagerGetCommitHeadsError";
  case EventType::GitRepoManagerGetCommitIsHead:
    return "GitRepoManagerGetCommitIsHead";
  case EventType::GitRepoManagerGetCommitIsHeadResult:
    return "GitRepoManagerGetCommitIsHeadResult";
  case EventType::GitRepoManagerGetCommitIsHeadError:
    return "GitRepoManagerGetCommitIsHeadError";
  case EventType::GitRepoManagerGetIsMergeInProgress:
    return "GitRepoManagerGetIsMergeInProgress";
  case EventType::GitRepoManagerGetIsMergeInProgressResult:
    return "GitRepoManagerGetIsMergeInProgressResult";
  case EventType::GitRepoManagerGetIsMergeInProgressError:
    return "GitRepoManagerGetIsMergeInProgressError";
  case EventType::GitRepoManagerGetCommitStatus:
    return "GitRepoManagerGetCommitStatus";
  case EventType::GitRepoManagerGetCommitStatusResult:
    return "GitRepoManagerGetCommitStatusResult";
  case EventType::GitRepoManagerGetCommitStatusError:
    return "GitRepoManagerGetCommitStatusError";
  case EventType::GitRepoManagerGetStashStatus:
    return "GitRepoManagerGetStashStatus";
  case EventType::GitRepoManagerGetStashStatusResult:
    return "GitRepoManagerGetStashStatusResult";
  case EventType::GitRepoManagerGetStashStatusError:
    return "GitRepoManagerGetStashStatusError";
  case EventType::GitRepoManagerGetSubmodulePath:
    return "GitRepoManagerGetSubmodulePath";
  case EventType::GitRepoManagerGetSubmodulePathResult:
    return "GitRepoManagerGetSubmodulePathResult";
  case EventType::GitRepoManagerGetSubmodulePathError:
    return "GitRepoManagerGetSubmodulePathError";
  case EventType::GitRepoManagerGetCredentials:
    return "GitRepoManagerGetCredentials";
  case EventType::GitRepoManagerGetCredentialsInvalid:
    return "GitRepoManagerGetCredentialsInvalid";
  case EventType::GitRepoManagerGetCredentialsFinished:
    return "GitRepoManagerGetCredentialsFinished";
  case EventType::GitRepoManagerGetCredentialsSubmit:
    return "GitRepoManagerGetCredentialsSubmit";
  case EventType::GitRepoManagerGetCredentialsCancel:
    return "GitRepoManagerGetCredentialsCancel";
  case EventType::GitRepoManagerGetUpstream:
    return "GitRepoManagerGetUpstream";
  case EventType::GitRepoManagerGetUpstreamResult:
    return "GitRepoManagerGetUpstreamResult";
  case EventType::GitRepoManagerGetUpstreamError:
    return "GitRepoManagerGetUpstreamError";
  case EventType::GitRepoManagerInitRepo:
    return "GitRepoManagerInitRepo";
  case EventType::GitRepoManagerInitRepoResult:
    return "GitRepoManagerInitRepoResult";
  case EventType::GitRepoManagerInitRepoError:
    return "GitRepoManagerInitRepoError";
  case EventType::GitRepoManagerListConflicts:
    return "GitRepoManagerListConflicts";
  case EventType::GitRepoManagerListConflictsResult:
    return "GitRepoManagerListConflictsResult";
  case EventType::GitRepoManagerListConflictsError:
    return "GitRepoManagerListConflictsError";
  case EventType::GitRepoManagerListLocalBranches:
    return "GitRepoManagerListLocalBranches";
  case EventType::GitRepoManagerListLocalBranchesResult:
    return "GitRepoManagerListLocalBranchesResult";
  case EventType::GitRepoManagerListLocalBranchesError:
    return "GitRepoManagerListLocalBranchesError";
  case EventType::GitRepoManagerListRemoteBranches:
    return "GitRepoManagerListRemoteBranches";
  case EventType::GitRepoManagerListRemoteBranchesResult:
    return "GitRepoManagerListRemoteBranchesResult";
  case EventType::GitRepoManagerListRemoteBranchesError:
    return "GitRepoManagerListRemoteBranchesError";
  case EventType::GitRepoManagerListRemoteBranchesWithStatus:
    return "GitRepoManagerListRemoteBranchesWithStatus";
  case EventType::GitRepoManagerListRemoteBranchesWithStatusResult:
    return "GitRepoManagerListRemoteBranchesWithStatusResult";
  case EventType::GitRepoManagerListRemoteBranchesWithStatusError:
    return "GitRepoManagerListRemoteBranchesWithStatusError";
  case EventType::GitRepoManagerListRemotes:
    return "GitRepoManagerListRemotes";
  case EventType::GitRepoManagerListRemotesResult:
    return "GitRepoManagerListRemotesResult";
  case EventType::GitRepoManagerListRemotesError:
    return "GitRepoManagerListRemotesError";
  case EventType::GitRepoManagerListStashes:
    return "GitRepoManagerListStashes";
  case EventType::GitRepoManagerListStashesResult:
    return "GitRepoManagerListStashesResult";
  case EventType::GitRepoManagerListStashesError:
    return "GitRepoManagerListStashesError";
  case EventType::GitRepoManagerListSubmodules:
    return "GitRepoManagerListSubmodules";
  case EventType::GitRepoManagerListSubmodulesResult:
    return "GitRepoManagerListSubmodulesResult";
  case EventType::GitRepoManagerListSubmodulesError:
    return "GitRepoManagerListSubmodulesError";
  case EventType::GitRepoManagerListSubmodulesWithStatus:
    return "GitRepoManagerListSubmodulesWithStatus";
  case EventType::GitRepoManagerListSubmodulesWithStatusResult:
    return "GitRepoManagerListSubmodulesWithStatusResult";
  case EventType::GitRepoManagerListSubmodulesWithStatusError:
    return "GitRepoManagerListSubmodulesWithStatusError";
  case EventType::GitRepoManagerListTags:
    return "GitRepoManagerListTags";
  case EventType::GitRepoManagerListTagsResult:
    return "GitRepoManagerListTagsResult";
  case EventType::GitRepoManagerListTagsError:
    return "GitRepoManagerListTagsError";
  case EventType::GitRepoManagerMerge:
    return "GitRepoManagerMerge";
  case EventType::GitRepoManagerMergeResult:
    return "GitRepoManagerMergeResult";
  case EventType::GitRepoManagerMergeError:
    return "GitRepoManagerMergeError";
  case EventType::GitRepoManagerOpenRepo:
    return "GitRepoManagerOpenRepo";
  case EventType::GitRepoManagerOpenRepoResult:
    return "GitRepoManagerOpenRepoResult";
  case EventType::GitRepoManagerOpenRepoError:
    return "GitRepoManagerOpenRepoError";
  case EventType::GitRepoManagerPull:
    return "GitRepoManagerPull";
  case EventType::GitRepoManagerPullResult:
    return "GitRepoManagerPullResult";
  case EventType::GitRepoManagerPullError:
    return "GitRepoManagerPullError";
  case EventType::GitRepoManagerPush:
    return "GitRepoManagerPush";
  case EventType::GitRepoManagerPushResult:
    return "GitRepoManagerPushResult";
  case EventType::GitRepoManagerPushError:
    return "GitRepoManagerPushError";
  case EventType::GitRepoManagerPushTag:
    return "GitRepoManagerPushTag";
  case EventType::GitRepoManagerPushTagResult:
    return "GitRepoManagerPushTagResult";
  case EventType::GitRepoManagerPushTagError:
    return "GitRepoManagerPushTagError";
  case EventType::GitRepoManagerPushAllTags:
    return "GitRepoManagerPushAllTags";
  case EventType::GitRepoManagerPushAllTagsResult:
    return "GitRepoManagerPushAllTagsResult";
  case EventType::GitRepoManagerPushAllTagsError:
    return "GitRepoManagerPushAllTagsError";
  case EventType::GitRepoManagerFetchAll:
    return "GitRepoManagerFetchAll";
  case EventType::GitRepoManagerFetchAllResult:
    return "GitRepoManagerFetchAllResult";
  case EventType::GitRepoManagerFetchAllError:
    return "GitRepoManagerFetchAllError";
  case EventType::GitRepoManagerFetchRemote:
    return "GitRepoManagerFetchRemote";
  case EventType::GitRepoManagerFetchRemoteResult:
    return "GitRepoManagerFetchRemoteResult";
  case EventType::GitRepoManagerFetchRemoteError:
    return "GitRepoManagerFetchRemoteError";
  case EventType::GitRepoManagerRebase:
    return "GitRepoManagerRebase";
  case EventType::GitRepoManagerRebaseResult:
    return "GitRepoManagerRebaseResult";
  case EventType::GitRepoManagerRebaseError:
    return "GitRepoManagerRebaseError";
  case EventType::GitRepoManagerRemoveRemote:
    return "GitRepoManagerRemoveRemote";
  case EventType::GitRepoManagerRemoveRemoteResult:
    return "GitRepoManagerRemoveRemoteResult";
  case EventType::GitRepoManagerRemoveRemoteError:
    return "GitRepoManagerRemoveRemoteError";
  case EventType::GitRepoManagerRenameBranch:
    return "GitRepoManagerRenameBranch";
  case EventType::GitRepoManagerRenameBranchResult:
    return "GitRepoManagerRenameBranchResult";
  case EventType::GitRepoManagerRenameBranchError:
    return "GitRepoManagerRenameBranchError";
  case EventType::GitRepoManagerRenameTag:
    return "GitRepoManagerRenameTag";
  case EventType::GitRepoManagerRenameTagResult:
    return "GitRepoManagerRenameTagResult";
  case EventType::GitRepoManagerRenameTagError:
    return "GitRepoManagerRenameTagError";
  case EventType::GitRepoManagerReset:
    return "GitRepoManagerReset";
  case EventType::GitRepoManagerResetResult:
    return "GitRepoManagerResetResult";
  case EventType::GitRepoManagerResetError:
    return "GitRepoManagerResetError";
  case EventType::GitRepoManagerRevert:
    return "GitRepoManagerRevert";
  case EventType::GitRepoManagerRevertResult:
    return "GitRepoManagerRevertResult";
  case EventType::GitRepoManagerRevertError:
    return "GitRepoManagerRevertError";
  case EventType::GitRepoManagerSetDefaultRemote:
    return "GitRepoManagerSetDefaultRemote";
  case EventType::GitRepoManagerSetDefaultRemoteResult:
    return "GitRepoManagerSetDefaultRemoteResult";
  case EventType::GitRepoManagerSetDefaultRemoteError:
    return "GitRepoManagerSetDefaultRemoteError";
  case EventType::GitRepoManagerSetUpstream:
    return "GitRepoManagerSetUpstream";
  case EventType::GitRepoManagerSetUpstreamResult:
    return "GitRepoManagerSetUpstreamResult";
  case EventType::GitRepoManagerSetUpstreamError:
    return "GitRepoManagerSetUpstreamError";
  case EventType::GitRepoManagerStageFiles:
    return "GitRepoManagerStageFiles";
  case EventType::GitRepoManagerStageFilesResult:
    return "GitRepoManagerStageFilesResult";
  case EventType::GitRepoManagerStageFilesError:
    return "GitRepoManagerStageFilesError";
  case EventType::GitRepoManagerStash:
    return "GitRepoManagerStash";
  case EventType::GitRepoManagerStashResult:
    return "GitRepoManagerStashResult";
  case EventType::GitRepoManagerStashError:
    return "GitRepoManagerStashError";
  case EventType::GitRepoManagerStatus:
    return "GitRepoManagerStatus";
  case EventType::GitRepoManagerStatusResult:
    return "GitRepoManagerStatusResult";
  case EventType::GitRepoManagerStatusError:
    return "GitRepoManagerStatusError";
  case EventType::GitRepoManagerSubmoduleUpdate:
    return "GitRepoManagerSubmoduleUpdate";
  case EventType::GitRepoManagerSubmoduleUpdateResult:
    return "GitRepoManagerSubmoduleUpdateResult";
  case EventType::GitRepoManagerSubmoduleUpdateError:
    return "GitRepoManagerSubmoduleUpdateError";
  case EventType::GitRepoManagerUnstageFiles:
    return "GitRepoManagerUnstageFiles";
  case EventType::GitRepoManagerUnstageFilesResult:
    return "GitRepoManagerUnstageFilesResult";
  case EventType::GitRepoManagerUnstageFilesError:
    return "GitRepoManagerUnstageFilesError";
  case EventType::GitRepoManagerUpdateRemote:
    return "GitRepoManagerUpdateRemote";
  case EventType::GitRepoManagerUpdateRemoteResult:
    return "GitRepoManagerUpdateRemoteResult";
  case EventType::GitRepoManagerUpdateRemoteError:
    return "GitRepoManagerUpdateRemoteError";
  case EventType::GitRepoManagerQuit:
    return "GitRepoManagerQuit";
  case EventType::GitRepoManagerQuitResult:
    return "GitRepoManagerQuitResult";
  case EventType::GitRepoManagerQuitError:
    return "GitRepoManagerQuitError";
  }

  return QString("Unknown(%1)").arg(static_cast<int>(type));
}

void EventManager::send(Event event) {
  LOCKGUARD(_queueLocker);
  const EventType type = event.type();
  const std::vector<EventCallback> &callbacks = _subscribers[type];
  const size_t callbackSize = callbacks.size();

  if (callbackSize > 0) {
    for (size_t i = 0; i < callbackSize; i++) {
      const EventCallback &callback = callbacks[i];
      const CallbackType &callbackFunc = callback._callback;
      const auto EventDataFunction = std::bind(callbackFunc, event);

      _eventsQueue.push(
          {callback, EventDataFunction, EventRecordType::Send, event.type()});

      LOG_DEBUG("Event has been queued: {}", eventName(type));
    }
  } else
    LOG_DEBUG("Event has NOT been queued: {}", eventName(type));

  size_t queueCount = 0;
  for (auto &pair : _waitEventsQueue) {
    pair.second.push({EventRecordType::Send, event});
    queueCount++;
  }

  if (queueCount == 0) {
    LOG_WARN("Event '{}' has no wait queues to deliver to (callbacks: {})",
             eventName(type), callbackSize);
  } else {
    LOG_DEBUG("Event '{}' added to {} wait queue(s)", eventName(type),
              queueCount);
  }
}

void EventManager::start() {
  if (_thread)
    return;

  _thread = QThread::create([&]() { threadLoop(); });

  connect(_thread, &QThread::finished, _thread, &QObject::deleteLater);

  _thread->start();

  LOG_DEBUG("EventManager started.");
}

void EventManager::stop() {
  {
    LOCKGUARD(_queueLocker);

    _eventsQueue.push({{}, nullptr, EventRecordType::Quit, EventType::Quit});
  }

  _thread->wait();

  _thread = nullptr;

  LOG_DEBUG("EventManager stopped.");
}

} // namespace EventManager
