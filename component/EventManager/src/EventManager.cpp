#include "EventManager.h"

#include <General.h>
#include <Log.h>

#include <QDateTime>

#include <functional>

#define THREAD_ID "EventManager"

namespace EventManager {

std::atomic<int> EventManager::_waitId = 0;

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
    if (record._type == EventRecordType::Quit)
      break;

    const qint64 start = QDateTime::currentMSecsSinceEpoch();

    LOG_DEBUG("Dispatching event {}", eventName(record._eventType));

    record._callback();

    const qint64 end = QDateTime::currentMSecsSinceEpoch();

    LOG_PERF(end - start, "Event execution duration");
  }
}

void EventManager::subscribe(EventCallback callback) {
  LOCKGUARD(_queueLocker);

  _subscribers[callback._type].push_back(callback);
}

void EventManager::unsubscribe(EventCallback callback) {
  LOCKGUARD(_queueLocker);

  _subscribers[callback._type].erase(
      std::remove_if(_subscribers[callback._type].begin(),
                     _subscribers[callback._type].end(),
                     [=](const EventCallback &cb) {
                       return (cb._callback.target_type() ==
                               callback._callback.target_type()) &&
                              (cb._callback.target<void (*)(Event)>() ==
                               callback._callback.target<void (*)(Event)>()) &&
                              (cb._receiver == callback._receiver);
                     }),
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

QString EventManager::eventName(EventType type) const
{
  switch(type) {
  case EventType::Null:
    return "Null";
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
  case EventType::GitRepoManagerCheckoutBranch:
    return "GitRepoManagerCheckoutBranch";
  case EventType::GitRepoManagerCheckoutBranchResult:
    return "GitRepoManagerCheckoutBranchResult";
  case EventType::GitRepoManagerCheckoutBranchError:
    return "GitRepoManagerCheckoutBranchError";
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
  case EventType::GitRepoManagerDiffFiles:
    return "GitRepoManagerDiffFiles";
  case EventType::GitRepoManagerDiffFilesResult:
    return "GitRepoManagerDiffFilesResult";
  case EventType::GitRepoManagerDiffFilesError:
    return "GitRepoManagerDiffFilesError";
  case EventType::GitRepoManagerDiscardFiles:
    return "GitRepoManagerDiscardFiles";
  case EventType::GitRepoManagerDiscardFilesResult:
    return "GitRepoManagerDiscardFilesResult";
  case EventType::GitRepoManagerDiscardFilesError:
    return "GitRepoManagerDiscardFilesError";
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
  case EventType::GitRepoManagerGetCommitStatus:
    return "GitRepoManagerGetCommitStatus";
  case EventType::GitRepoManagerGetCommitStatusResult:
    return "GitRepoManagerGetCommitStatusResult";
  case EventType::GitRepoManagerGetCommitStatusError:
    return "GitRepoManagerGetCommitStatusError";
  case EventType::GitRepoManagerGetCredentials:
    return "GitRepoManagerGetCredentials";
  case EventType::GitRepoManagerGetCredentialsFinished:
    return "GitRepoManagerGetCredentialsFinished";
  case EventType::GitRepoManagerGetCredentialsSubmit:
    return "GitRepoManagerGetCredentialsSubmit";
  case EventType::GitRepoManagerGetCredentialsCancel:
    return "GitRepoManagerGetCredentialsCancel";
  case EventType::GitRepoManagerInitRepo:
    return "GitRepoManagerInitRepo";
  case EventType::GitRepoManagerInitRepoResult:
    return "GitRepoManagerInitRepoResult";
  case EventType::GitRepoManagerInitRepoError:
    return "GitRepoManagerInitRepoError";
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
  case EventType::GitRepoManagerListSubmodules:
    return "GitRepoManagerListSubmodules";
  case EventType::GitRepoManagerListSubmodulesResult:
    return "GitRepoManagerListSubmodulesResult";
  case EventType::GitRepoManagerListSubmodulesError:
    return "GitRepoManagerListSubmodulesError";
  case EventType::GitRepoManagerListTags:
    return "GitRepoManagerListTags";
  case EventType::GitRepoManagerListTagsResult:
    return "GitRepoManagerListTagsResult";
  case EventType::GitRepoManagerListTagsError:
    return "GitRepoManagerListTagsError";
  case EventType::GitRepoManagerOpenRepo:
    return "GitRepoManagerOpenRepo";
  case EventType::GitRepoManagerOpenRepoResult:
    return "GitRepoManagerOpenRepoResult";
  case EventType::GitRepoManagerOpenRepoError:
    return "GitRepoManagerOpenRepoError";
  case EventType::GitRepoManagerStageFiles:
    return "GitRepoManagerStageFiles";
  case EventType::GitRepoManagerStageFilesResult:
    return "GitRepoManagerStageFilesResult";
  case EventType::GitRepoManagerStageFilesError:
    return "GitRepoManagerStageFilesError";
  case EventType::GitRepoManagerStatus:
    return "GitRepoManagerStatus";
  case EventType::GitRepoManagerStatusResult:
    return "GitRepoManagerStatusResult";
  case EventType::GitRepoManagerStatusError:
    return "GitRepoManagerStatusError";
  case EventType::GitRepoManagerUnstageFiles:
    return "GitRepoManagerUnstageFiles";
  case EventType::GitRepoManagerUnstageFilesResult:
    return "GitRepoManagerUnstageFilesResult";
  case EventType::GitRepoManagerUnstageFilesError:
    return "GitRepoManagerUnstageFilesError";
  case EventType::GitRepoManagerQuit:
    return "GitRepoManagerQuit";
  case EventType::GitRepoManagerQuitResult:
    return "GitRepoManagerQuitResult";
  case EventType::GitRepoManagerQuitError:
    return "GitRepoManagerQuitError";
  default:
    return QString("Unknown(%1)").arg(static_cast<int>(type));
  }
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

      _eventsQueue.push({EventDataFunction, EventRecordType::Send, event.type()});

      LOG_DEBUG("Event has been queued: {}", eventName(type));
    }
  } else
    LOG_DEBUG("Event has NOT been queued: {}", eventName(type));

  for (auto &pair : _waitEventsQueue) {
    pair.second.push({EventRecordType::Send, event});
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

    _eventsQueue.push({nullptr, EventRecordType::Quit, EventType::Quit});
  }

  _thread->wait();

  _thread = nullptr;

  LOG_DEBUG("EventManager stopped.");
}

} // namespace EventManager
