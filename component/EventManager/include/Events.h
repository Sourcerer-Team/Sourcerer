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
  GitRepoManagerCheckoutBranch,
  GitRepoManagerCheckoutBranchResult,
  GitRepoManagerCheckoutBranchError,
  GitRepoManagerCloneRepo,
  GitRepoManagerCloneRepoResult,
  GitRepoManagerCloneRepoError,
  GitRepoManagerCloneRepoProgress,
  GitRepoManagerCloseRepo,
  GitRepoManagerCloseRepoResult,
  GitRepoManagerCloseRepoError,
  GitRepoManagerGetActiveBranch,
  GitRepoManagerGetActiveBranchResult,
  GitRepoManagerGetActiveBranchError,
  GitRepoManagerGetBranchHistory,
  GitRepoManagerGetBranchHistoryResult,
  GitRepoManagerGetBranchHistoryError,
  GitRepoManagerGetCommitBranches,
  GitRepoManagerGetCommitBranchesResult,
  GitRepoManagerGetCommitBranchesError,
  GitRepoManagerGetCommitHeads,
  GitRepoManagerGetCommitHeadsResult,
  GitRepoManagerGetCommitHeadsError,
  GitRepoManagerGetCommitIsHead,
  GitRepoManagerGetCommitIsHeadResult,
  GitRepoManagerGetCommitIsHeadError,
  GitRepoManagerGetCommitStatus,
  GitRepoManagerGetCommitStatusResult,
  GitRepoManagerGetCommitStatusError,
  GitRepoManagerGetCredentials,
  GitRepoManagerGetCredentialsResult,
  GitRepoManagerGetCredentialsError,
  GitRepoManagerInitRepo,
  GitRepoManagerInitRepoResult,
  GitRepoManagerInitRepoError,
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
  GitRepoManagerStageFiles,
  GitRepoManagerStageFilesResult,
  GitRepoManagerStageFilesError,
  GitRepoManagerStatus,
  GitRepoManagerStatusResult,
  GitRepoManagerStatusError,
  GitRepoManagerUnstageFiles,
  GitRepoManagerUnstageFilesResult,
  GitRepoManagerUnstageFilesError,
  GitRepoManagerQuit,
  GitRepoManagerQuitResult,
  GitRepoManagerQuitError,

  last
};

class Event {
public:
  Event(const EventType eventType, const QString &uuid = QString())
      : _type(eventType), _uuid(uuid) {}

  void addData(const std::any &data) { _anyData = data; }

  template <typename T> T data() const { return std::any_cast<T>(_anyData); }

  EventType type() const { return _type; }

  const QString &uuid() const { return _uuid; }

private:
  EventType _type;
  QString _uuid;
  std::any _anyData;
};

} // namespace EventManager
