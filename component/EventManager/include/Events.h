#pragma once

#include <QString>

#include <any>

namespace EventManager {

enum EventType {
  Null = 0,
  Quit,

  // Error and response handling
  Error,
  Success,

  // ComponentMonitor
  ComponentStarted,
  ComponentStopped,

  // GitRepoManager
  GitRepoManagerAddSubmodule,
  GitRepoManagerAddSubmoduleResult,
  GitRepoManagerAddSubmoduleError,
  GitRepoManagerApplyPatch,
  GitRepoManagerApplyPatchResult,
  GitRepoManagerApplyPatchError,
  GitRepoManagerApplyStash,
  GitRepoManagerApplyStashResult,
  GitRepoManagerApplyStashError,
  GitRepoManagerCheckoutBranch,
  GitRepoManagerCheckoutBranchResult,
  GitRepoManagerCheckoutBranchError,
  GitRepoManagerCheckoutCommit,
  GitRepoManagerCheckoutCommitResult,
  GitRepoManagerCheckoutCommitError,
  GitRepoManagerCloneRepo,
  GitRepoManagerCloneRepoResult,
  GitRepoManagerCloneRepoError,
  GitRepoManagerCloneRepoProgress,
  GitRepoManagerCloseRepo,
  GitRepoManagerCloseRepoResult,
  GitRepoManagerCloseRepoError,
  GitRepoManagerCommitStaged,
  GitRepoManagerCommitStagedResult,
  GitRepoManagerCommitStagedError,
  GitRepoManagerCreateBranchFromCommit,
  GitRepoManagerCreateBranchFromCommitResult,
  GitRepoManagerCreateBranchFromCommitError,
  GitRepoManagerCreatePatchFromCommit,
  GitRepoManagerCreatePatchFromCommitResult,
  GitRepoManagerCreatePatchFromCommitError,
  GitRepoManagerCreateTagFromCommit,
  GitRepoManagerCreateTagFromCommitResult,
  GitRepoManagerCreateTagFromCommitError,
  GitRepoManagerDeleteBranch,
  GitRepoManagerDeleteBranchResult,
  GitRepoManagerDeleteBranchError,
  GitRepoManagerDeleteStash,
  GitRepoManagerDeleteStashResult,
  GitRepoManagerDeleteStashError,
  GitRepoManagerDeleteSubmodule,
  GitRepoManagerDeleteSubmoduleResult,
  GitRepoManagerDeleteSubmoduleError,
  GitRepoManagerDeleteTag,
  GitRepoManagerDeleteTagResult,
  GitRepoManagerDeleteTagError,
  GitRepoManagerDiffFiles,
  GitRepoManagerDiffFilesResult,
  GitRepoManagerDiffFilesError,
  GitRepoManagerDiffFilesFromCommit,
  GitRepoManagerDiffFilesFromCommitResult,
  GitRepoManagerDiffFilesFromCommitError,
  GitRepoManagerDiscardFiles,
  GitRepoManagerDiscardFilesResult,
  GitRepoManagerDiscardFilesError,
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
  GitRepoManagerGetSubmodulePath,
  GitRepoManagerGetSubmodulePathResult,
  GitRepoManagerGetSubmodulePathError,
  GitRepoManagerGetCredentials,
  GitRepoManagerGetCredentialsInvalid,
  GitRepoManagerGetCredentialsFinished,
  GitRepoManagerGetCredentialsSubmit,
  GitRepoManagerGetCredentialsCancel,
  GitRepoManagerGetUpstream,
  GitRepoManagerGetUpstreamResult,
  GitRepoManagerGetUpstreamError,
  GitRepoManagerInitRepo,
  GitRepoManagerInitRepoResult,
  GitRepoManagerInitRepoError,
  GitRepoManagerListLocalBranches,
  GitRepoManagerListLocalBranchesResult,
  GitRepoManagerListLocalBranchesError,
  GitRepoManagerListRemoteBranches,
  GitRepoManagerListRemoteBranchesResult,
  GitRepoManagerListRemoteBranchesError,
  GitRepoManagerListStashes,
  GitRepoManagerListStashesResult,
  GitRepoManagerListStashesError,
  GitRepoManagerListSubmodules,
  GitRepoManagerListSubmodulesResult,
  GitRepoManagerListSubmodulesError,
  GitRepoManagerListTags,
  GitRepoManagerListTagsResult,
  GitRepoManagerListTagsError,
  GitRepoManagerMerge,
  GitRepoManagerMergeResult,
  GitRepoManagerMergeError,
  GitRepoManagerOpenRepo,
  GitRepoManagerOpenRepoResult,
  GitRepoManagerOpenRepoError,
  GitRepoManagerPull,
  GitRepoManagerPullResult,
  GitRepoManagerPullError,
  GitRepoManagerPush,
  GitRepoManagerPushResult,
  GitRepoManagerPushError,
  GitRepoManagerRenameBranch,
  GitRepoManagerRenameBranchResult,
  GitRepoManagerRenameBranchError,
  GitRepoManagerRenameTag,
  GitRepoManagerRenameTagResult,
  GitRepoManagerRenameTagError,
  GitRepoManagerReset,
  GitRepoManagerResetResult,
  GitRepoManagerResetError,
  GitRepoManagerRevert,
  GitRepoManagerRevertResult,
  GitRepoManagerRevertError,
  GitRepoManagerSetUpstream,
  GitRepoManagerSetUpstreamResult,
  GitRepoManagerSetUpstreamError,
  GitRepoManagerStageFiles,
  GitRepoManagerStageFilesResult,
  GitRepoManagerStageFilesError,
  GitRepoManagerStash,
  GitRepoManagerStashResult,
  GitRepoManagerStashError,
  GitRepoManagerStatus,
  GitRepoManagerStatusResult,
  GitRepoManagerStatusError,
  GitRepoManagerSubmoduleUpdate,
  GitRepoManagerSubmoduleUpdateResult,
  GitRepoManagerSubmoduleUpdateError,
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
