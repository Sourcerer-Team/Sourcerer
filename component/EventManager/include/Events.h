#pragma once

#include <QString>

#include <any>

namespace EventManager {

enum EventType {
  Null = 0,

  // Error and response handling
  Error,
  Success,

  // ComponentMonitor
  ComponentStarted,
  ComponentStopped,

  // GitRepoManager
  GitRepoManagerCloneRepo,
  GitRepoManagerCloneRepoError,
  GitRepoManagerCloneRepoResult,
  GitRepoManagerCloneRepoProgress,
  GitRepoManagerCloseRepo,
  GitRepoManagerCloseRepoResult,
  GitRepoManagerCloseRepoError,
  GitRepoManagerGetActiveBranch,
  GitRepoManagerGetActiveBranchResult,
  GitRepoManagerGetActiveBranchError,
  GitRepoManagerGetCommitBranches,
  GitRepoManagerGetCommitBranchesResult,
  GitRepoManagerGetCommitBranchesError,
  GitRepoManagerGetCommitHeads,
  GitRepoManagerGetCommitHeadsResult,
  GitRepoManagerGetCommitHeadsError,
  GitRepoManagerGetCommitIsHead,
  GitRepoManagerGetCommitIsHeadResult,
  GitRepoManagerGetCommitIsHeadError,
  GitRepoManagerGetCredentials,
  GitRepoManagerGetCredentialsError,
  GitRepoManagerGetCredentialsResult,
  GitRepoManagerGetHistory,
  GitRepoManagerGetHistoryError,
  GitRepoManagerGetHistoryResult,
  GitRepoManagerInitRepo,
  GitRepoManagerInitRepoError,
  GitRepoManagerInitRepoResult,
  GitRepoManagerListLocalBranches,
  GitRepoManagerListLocalBranchesResult,
  GitRepoManagerListLocalBranchesError,
  GitRepoManagerListRemoteBranches,
  GitRepoManagerListRemoteBranchesResult,
  GitRepoManagerListRemoteBranchesError,
  GitRepoManagerListSubmodules,
  GitRepoManagerListSubmodulesResult,
  GitRepoManagerListSubmodulesError,
  GitRepoManagerListTags,
  GitRepoManagerListTagsResult,
  GitRepoManagerListTagsError,
  GitRepoManagerOpenRepo,
  GitRepoManagerOpenRepoResult,
  GitRepoManagerOpenRepoError,
  GitRepoManagerQuit,
  GitRepoManagerQuitResult,
  GitRepoManagerQuitError,

  last
};

class Event {
public:
  Event(EventType eventType, const QString &uuid = QString())
      : _type(eventType), _uuid(uuid) {}

  void addData(std::any data) { _anyData = data; }

  template <typename T> T data() const { return std::any_cast<T>(_anyData); }

  EventType type() const { return _type; }

  const QString &uuid() const { return _uuid; }

private:
  EventType _type;
  QString _uuid;
  std::any _anyData;
};

} // namespace EventManager
