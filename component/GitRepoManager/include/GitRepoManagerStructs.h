#pragma once

#include <GitRepo.h>

#include <BlockingQueue.h>

#include <QString>
#include <QThread>
#include <QVariant>

#include <git2/repository.h>

namespace GitRepoManager::Structs {

enum class GitRepoCommandType : int {
  Unknown = 0,
  AbortMerge,
  AddSubmodule,
  AddRemote,
  Amend,
  Blame,
  ApplyPatch,
  ApplyStash,
  CheckoutBranch,
  CheckoutCommit,
  Clone,
  Close,
  CommitStaged,
  CreateBranchFromCommit,
  CreatePatchFromCommit,
  CreateTagFromCommit,
  DeleteBranch,
  DeleteRemoteBranch,
  DeleteStash,
  DeleteAllStashes,
  DeleteSubmodule,
  DeleteTag,
  DeleteRemoteTag,
  DiffFiles,
  DiffFilesFromCommit,
  DiscardFiles,
  FetchAll,
  FetchRemote,
  GetActiveBranch,
  Rebase,
  GetFileSizeFromCommit,
  GetLfsStorageStatus,
  GetBranchHistory,
  GetCommitBranches,
  GetCommitTags,
  GetCommitHeads,
  GetCommitIsHead,
  GetCommitStatus,
  GetIsMergeInProgress,
  GetStashStatus,
  GetSubmodulePath,
  GetUpstream,
  Init,
  ListConflicts,
  ListLocalBranches,
  ListRemoteBranches,
  ListRemoteBranchesWithStatus,
  ListRemotes,
  ListStashes,
  ListSubmodules,
  ListSubmodulesWithStatus,
  ListTags,
  Merge,
  Open,
  Pull,
  Push,
  PushTag,
  PushAllTags,
  RemoveRemote,
  RenameBranch,
  RenameTag,
  Reset,
  Revert,
  CherryPick,
  SetDefaultRemote,
  AddTarget,
  EditTarget,
  DeleteTarget,
  SetDefaultTarget,
  SetUpstream,
  StageFiles,
  Stash,
  Status,
  SubmoduleUpdate,
  UnstageFiles,
  UpdateRemote,
  Quit,
  Last
};

enum class GitRepoErrorType : int {
  Unknown = 0,
  AlreadyExist,
  ErrorWhileOpening,
  InternalError,
  NotCloned,
  NotInitialized,
  NotOpened,
  Last
};

struct GitRepoCommand {
  explicit GitRepoCommand(const GitRepoCommandType commandType)
      : type(commandType) {}

  virtual ~GitRepoCommand() = default;
  GitRepoCommandType type;
  QString uuid;
  QString syncUuid;
};

struct GitRepoAbortMergeCommand final : GitRepoCommand {
  GitRepoAbortMergeCommand() : GitRepoCommand(GitRepoCommandType::AbortMerge) {}

  ~GitRepoAbortMergeCommand() override = default;
};

struct GitRepoAddRemoteCommand final : GitRepoCommand {
  GitRepoAddRemoteCommand() : GitRepoCommand(GitRepoCommandType::AddRemote) {}

  ~GitRepoAddRemoteCommand() override = default;

  QString remoteName;
  QString fetchUrl;
  QString pushUrl;
};

struct GitRepoAddSubmoduleCommand final : GitRepoCommand {
  GitRepoAddSubmoduleCommand()
      : GitRepoCommand(GitRepoCommandType::AddSubmodule) {}

  ~GitRepoAddSubmoduleCommand() override = default;

  QString url;
  QString name;
  QString branch;
};

struct GitRepoAmendCommand final : GitRepoCommand {
  GitRepoAmendCommand() : GitRepoCommand(GitRepoCommandType::Amend) {}

  ~GitRepoAmendCommand() override = default;

  QString author;
  QString email;
  QString summary;
  QString message;
  bool signoff = false;
  bool signCommit = false;
};

struct GitRepoBlameCommand final : GitRepoCommand {
  GitRepoBlameCommand() : GitRepoCommand(GitRepoCommandType::Blame) {}

  ~GitRepoBlameCommand() override = default;

  QStringList files;
};

struct GitRepoApplyPatchCommand final : GitRepoCommand {
  GitRepoApplyPatchCommand() : GitRepoCommand(GitRepoCommandType::ApplyPatch) {}

  ~GitRepoApplyPatchCommand() override = default;

  QString patchContent;
};

struct GitRepoApplyStashCommand final : GitRepoCommand {
  GitRepoApplyStashCommand() : GitRepoCommand(GitRepoCommandType::ApplyStash) {}

  ~GitRepoApplyStashCommand() override = default;

  int index = -1;
};

struct GitRepoCheckoutBranchCommand final : GitRepoCommand {
  GitRepoCheckoutBranchCommand()
      : GitRepoCommand(GitRepoCommandType::CheckoutBranch) {}

  ~GitRepoCheckoutBranchCommand() override = default;

  QString branchName;
  GitRepo::BranchType branchType = GitRepo::BranchType::Unknown;
};

struct GitRepoCheckoutBranchCommandResult {
  QString branchName;
  GitRepo::BranchType branchType = GitRepo::BranchType::Unknown;
};

struct GitRepoCheckoutCommitCommand final : GitRepoCommand {
  GitRepoCheckoutCommitCommand()
      : GitRepoCommand(GitRepoCommandType::CheckoutCommit) {}

  ~GitRepoCheckoutCommitCommand() override = default;

  QString commit;
};

struct GitRepoCherryPickCommand final : GitRepoCommand {
  GitRepoCherryPickCommand() : GitRepoCommand(GitRepoCommandType::CherryPick) {}

  ~GitRepoCherryPickCommand() override = default;

  QString commit;
  QString author;
  QString email;
};

struct GitRepoCloneCommand final : GitRepoCommand {
  GitRepoCloneCommand() : GitRepoCommand(GitRepoCommandType::Clone) {}

  ~GitRepoCloneCommand() override = default;

  QString url;
  QString destinationDir;
};

struct GitRepoCloseCommand final : GitRepoCommand {
  GitRepoCloseCommand() : GitRepoCommand(GitRepoCommandType::Close) {}

  ~GitRepoCloseCommand() override = default;
};

struct GitRepoCommitStagedCommand final : GitRepoCommand {
  GitRepoCommitStagedCommand()
      : GitRepoCommand(GitRepoCommandType::CommitStaged) {}

  ~GitRepoCommitStagedCommand() override = default;

  QString author;
  QString email;
  QString summary;
  QString message;
  bool cleanup = false;
  bool signoff = false;
  bool signCommit = false;
};

struct GitRepoCreateBranchFromCommitCommand final : GitRepoCommand {
  GitRepoCreateBranchFromCommitCommand()
      : GitRepoCommand(GitRepoCommandType::CreateBranchFromCommit) {}

  ~GitRepoCreateBranchFromCommitCommand() override = default;

  QString branchName;
  QString commit;
};

struct GitRepoCreatePatchFromCommitCommand final : GitRepoCommand {
  GitRepoCreatePatchFromCommitCommand()
      : GitRepoCommand(GitRepoCommandType::CreatePatchFromCommit) {}

  ~GitRepoCreatePatchFromCommitCommand() override = default;

  QString commit;
};

struct GitRepoCreatePatchFromCommitCommandResult {
  QString patchContent;
};

struct GitRepoCreateTagFromCommitCommand final : GitRepoCommand {
  GitRepoCreateTagFromCommitCommand()
      : GitRepoCommand(GitRepoCommandType::CreateTagFromCommit) {}

  ~GitRepoCreateTagFromCommitCommand() override = default;

  QString tagName;
  QString commit;
  QString author;
  QString email;
  QString message;
};

struct GitRepoDeleteBranchCommand final : GitRepoCommand {
  GitRepoDeleteBranchCommand()
      : GitRepoCommand(GitRepoCommandType::DeleteBranch) {}

  ~GitRepoDeleteBranchCommand() override = default;

  QString branchName;
};

struct GitRepoDeleteRemoteBranchCommand final : GitRepoCommand {
  GitRepoDeleteRemoteBranchCommand()
      : GitRepoCommand(GitRepoCommandType::DeleteRemoteBranch) {}

  ~GitRepoDeleteRemoteBranchCommand() override = default;

  QString branchName;
};

struct GitRepoDeleteStashCommand final : GitRepoCommand {
  GitRepoDeleteStashCommand()
      : GitRepoCommand(GitRepoCommandType::DeleteStash) {}

  ~GitRepoDeleteStashCommand() override = default;

  int index = -1;
};

struct GitRepoDeleteAllStashesCommand final : GitRepoCommand {
  GitRepoDeleteAllStashesCommand()
      : GitRepoCommand(GitRepoCommandType::DeleteAllStashes) {}

  ~GitRepoDeleteAllStashesCommand() override = default;
};

struct GitRepoGetStashStatusCommand final : GitRepoCommand {
  GitRepoGetStashStatusCommand()
      : GitRepoCommand(GitRepoCommandType::GetStashStatus) {}

  ~GitRepoGetStashStatusCommand() override = default;

  int index = -1;
};

struct GitRepoDeleteSubmoduleCommand final : GitRepoCommand {
  GitRepoDeleteSubmoduleCommand()
      : GitRepoCommand(GitRepoCommandType::DeleteSubmodule) {}

  ~GitRepoDeleteSubmoduleCommand() override = default;

  QString submoduleName;
  QString submodulePath;
};

struct GitRepoDeleteTagCommand final : GitRepoCommand {
  GitRepoDeleteTagCommand() : GitRepoCommand(GitRepoCommandType::DeleteTag) {}

  ~GitRepoDeleteTagCommand() override = default;

  QString tagName;
};

struct GitRepoDeleteRemoteTagCommand final : GitRepoCommand {
  GitRepoDeleteRemoteTagCommand()
      : GitRepoCommand(GitRepoCommandType::DeleteRemoteTag) {}

  ~GitRepoDeleteRemoteTagCommand() override = default;

  QString tagName;
};

struct GitRepoDiffFilesCommand final : GitRepoCommand {
  GitRepoDiffFilesCommand() : GitRepoCommand(GitRepoCommandType::DiffFiles) {}

  ~GitRepoDiffFilesCommand() override = default;

  QStringList files;
};

struct GitRepoDiffFilesCommandResult {
  QString diff;
  QMap<QString, QString> filesContent;
  QMap<QString, QString> repoContent;
  QVector<GitRepo::FilesDiffDelta> deltas;
};

struct GitRepoDiffFilesFromCommitCommand final : GitRepoCommand {
  GitRepoDiffFilesFromCommitCommand()
      : GitRepoCommand(GitRepoCommandType::DiffFilesFromCommit) {}

  ~GitRepoDiffFilesFromCommitCommand() override = default;

  QStringList files;
  QString commit;
};

struct GitRepoDiffFilesFromCommitCommandResult {
  QString diff;
  QMap<QString, QString> filesContent;
  QMap<QString, QString> repoContent;
  QVector<GitRepo::FilesDiffDelta> deltas;
};

struct GitRepoBlameCommandResult {
  GitRepo::BlameReport report;
};

struct GitRepoDiscardFilesCommand final : GitRepoCommand {
  GitRepoDiscardFilesCommand()
      : GitRepoCommand(GitRepoCommandType::DiscardFiles) {}

  ~GitRepoDiscardFilesCommand() override = default;

  QStringList files;
};

struct GitRepoFetchAllCommand final : GitRepoCommand {
  GitRepoFetchAllCommand() : GitRepoCommand(GitRepoCommandType::FetchAll) {}

  ~GitRepoFetchAllCommand() override = default;
};

struct GitRepoFetchRemoteCommand final : GitRepoCommand {
  GitRepoFetchRemoteCommand()
      : GitRepoCommand(GitRepoCommandType::FetchRemote) {}

  ~GitRepoFetchRemoteCommand() override = default;

  QString remoteName;
};

struct GitRepoGetFileSizeFromCommitCommand final : GitRepoCommand {
  GitRepoGetFileSizeFromCommitCommand()
      : GitRepoCommand(GitRepoCommandType::GetFileSizeFromCommit) {}

  ~GitRepoGetFileSizeFromCommitCommand() override = default;

  QString filePath;
  QString commit;
};

struct GitRepoGetFileSizeFromCommitCommandResult {
  qint64 fileSize = -1;
};

struct GitRepoGetActiveBranchCommand final : GitRepoCommand {
  GitRepoGetActiveBranchCommand()
      : GitRepoCommand(GitRepoCommandType::GetActiveBranch) {}

  ~GitRepoGetActiveBranchCommand() override = default;

  GitRepo::BranchType branchType = GitRepo::BranchType::Unknown;
};

struct GitRepoGetActiveBranchCommandResult {
  QString branchName;
  GitRepo::BranchType branchType = GitRepo::BranchType::Unknown;
};

struct GitRepoGetCommitBranchesCommand final : GitRepoCommand {
  GitRepoGetCommitBranchesCommand()
      : GitRepoCommand(GitRepoCommandType::GetCommitBranches) {}

  ~GitRepoGetCommitBranchesCommand() override = default;

  QString commitOid;
};

struct GitRepoGetCommitTagsCommand final : GitRepoCommand {
  GitRepoGetCommitTagsCommand()
      : GitRepoCommand(GitRepoCommandType::GetCommitTags) {}

  ~GitRepoGetCommitTagsCommand() override = default;

  QString commitOid;
};

struct GitRepoGetCommitHeadsCommand final : GitRepoCommand {
  GitRepoGetCommitHeadsCommand()
      : GitRepoCommand(GitRepoCommandType::GetCommitHeads) {}

  ~GitRepoGetCommitHeadsCommand() override = default;

  QString commitOid;
};

struct GitRepoGetCommitIsHeadCommand final : GitRepoCommand {
  GitRepoGetCommitIsHeadCommand()
      : GitRepoCommand(GitRepoCommandType::GetCommitIsHead) {}

  ~GitRepoGetCommitIsHeadCommand() override = default;

  QString commitOid;
};

struct GitRepoGetCommitStatusCommand final : GitRepoCommand {
  GitRepoGetCommitStatusCommand()
      : GitRepoCommand(GitRepoCommandType::GetCommitStatus) {}

  ~GitRepoGetCommitStatusCommand() override = default;

  QString commitOid;
};

struct GitRepoGetBranchHistoryCommand final : GitRepoCommand {
  GitRepoGetBranchHistoryCommand()
      : GitRepoCommand(GitRepoCommandType::GetBranchHistory) {}

  ~GitRepoGetBranchHistoryCommand() override = default;

  QString branchName;
  GitRepo::BranchType branchType = GitRepo::BranchType::Unknown;
};

struct GitRepoGetIsMergeInProgressCommand final : GitRepoCommand {
  GitRepoGetIsMergeInProgressCommand()
      : GitRepoCommand(GitRepoCommandType::GetIsMergeInProgress) {}

  ~GitRepoGetIsMergeInProgressCommand() override = default;
};

struct GitRepoGetSubmodulePathCommand final : GitRepoCommand {
  GitRepoGetSubmodulePathCommand()
      : GitRepoCommand(GitRepoCommandType::GetSubmodulePath) {}

  ~GitRepoGetSubmodulePathCommand() override = default;

  QString submoduleName;
};

struct GitRepoGetSubmodulePathCommandResult {
  QString submodulePath;
};

struct GitRepoGetUpstreamCommand final : GitRepoCommand {
  GitRepoGetUpstreamCommand()
      : GitRepoCommand(GitRepoCommandType::GetUpstream) {}

  ~GitRepoGetUpstreamCommand() override = default;

  QString localBranchName;
};

struct GitRepoGetUpstreamCommandResult {
  QString remoteName;
  QString remoteBranchName;
};

struct GitRepoInitRepoCommand final : GitRepoCommand {
  GitRepoInitRepoCommand() : GitRepoCommand(GitRepoCommandType::Init) {}

  ~GitRepoInitRepoCommand() override = default;

  QString repoName;
  QString workingDir;
  QString defaultBranch;
  QString author;
  QString email;
};

struct GitRepoListConflictsCommand final : GitRepoCommand {
  GitRepoListConflictsCommand()
      : GitRepoCommand(GitRepoCommandType::ListConflicts) {}

  ~GitRepoListConflictsCommand() override = default;

  QString filePath;
};

struct GitRepoListConflictsCommandResult {
  QString fileContentWithMarkers;
};

struct GitRepoListLocalBranchesCommand final : GitRepoCommand {
  GitRepoListLocalBranchesCommand()
      : GitRepoCommand(GitRepoCommandType::ListLocalBranches) {}

  ~GitRepoListLocalBranchesCommand() override = default;
};

struct GitRepoListRemoteBranchesCommand final : GitRepoCommand {
  GitRepoListRemoteBranchesCommand()
      : GitRepoCommand(GitRepoCommandType::ListRemoteBranches) {}

  ~GitRepoListRemoteBranchesCommand() override = default;
};

struct GitRepoListRemoteBranchesWithStatusCommand final : GitRepoCommand {
  GitRepoListRemoteBranchesWithStatusCommand()
      : GitRepoCommand(GitRepoCommandType::ListRemoteBranchesWithStatus) {}

  ~GitRepoListRemoteBranchesWithStatusCommand() override = default;
};

struct GitRepoListRemotesCommand final : GitRepoCommand {
  GitRepoListRemotesCommand()
      : GitRepoCommand(GitRepoCommandType::ListRemotes) {}

  ~GitRepoListRemotesCommand() override = default;
};

struct GitRepoListStashesCommand final : GitRepoCommand {
  GitRepoListStashesCommand()
      : GitRepoCommand(GitRepoCommandType::ListStashes) {}

  ~GitRepoListStashesCommand() override = default;
};

struct GitRepoListSubmodulesCommand final : GitRepoCommand {
  GitRepoListSubmodulesCommand()
      : GitRepoCommand(GitRepoCommandType::ListSubmodules) {}

  ~GitRepoListSubmodulesCommand() override = default;
};

struct GitRepoListSubmodulesWithStatusCommand final : GitRepoCommand {
  GitRepoListSubmodulesWithStatusCommand()
      : GitRepoCommand(GitRepoCommandType::ListSubmodulesWithStatus) {}

  ~GitRepoListSubmodulesWithStatusCommand() override = default;
};

struct GitRepoListTagsCommand final : GitRepoCommand {
  GitRepoListTagsCommand() : GitRepoCommand(GitRepoCommandType::ListTags) {}

  ~GitRepoListTagsCommand() override = default;
};

struct GitRepoMergeCommand final : GitRepoCommand {
  GitRepoMergeCommand() : GitRepoCommand(GitRepoCommandType::Merge) {}

  ~GitRepoMergeCommand() override = default;

  QString sourceBranch;
  QString destinationBranch;
  QString author;
  QString email;
};

struct GitRepoRebaseCommand final : GitRepoCommand {
  GitRepoRebaseCommand() : GitRepoCommand(GitRepoCommandType::Rebase) {}

  ~GitRepoRebaseCommand() override = default;

  QString sourceBranch;
  QString destinationBranch;
  QString author;
  QString email;
};

struct GitRepoOpenCommand final : GitRepoCommand {
  GitRepoOpenCommand() : GitRepoCommand(GitRepoCommandType::Open) {}

  ~GitRepoOpenCommand() override = default;
  QString path;
  QString author;
  QString email;
};

struct GitRepoPullCommand final : GitRepoCommand {
  GitRepoPullCommand() : GitRepoCommand(GitRepoCommandType::Pull) {}

  ~GitRepoPullCommand() override = default;

  QString activeBranchName; // The currently checked out branch name
  bool rebase = false;
};

struct GitRepoPushCommand final : GitRepoCommand {
  GitRepoPushCommand() : GitRepoCommand(GitRepoCommandType::Push) {}

  ~GitRepoPushCommand() override = default;

  QString activeBranchName; // The currently checked out branch name
  bool force = false;
};

struct GitRepoPushTagCommand final : GitRepoCommand {
  GitRepoPushTagCommand() : GitRepoCommand(GitRepoCommandType::PushTag) {}

  ~GitRepoPushTagCommand() override = default;

  QString tagName;
};

struct GitRepoPushAllTagsCommand final : GitRepoCommand {
  GitRepoPushAllTagsCommand()
      : GitRepoCommand(GitRepoCommandType::PushAllTags) {}

  ~GitRepoPushAllTagsCommand() override = default;
};

struct GitRepoRemoveRemoteCommand final : GitRepoCommand {
  GitRepoRemoveRemoteCommand()
      : GitRepoCommand(GitRepoCommandType::RemoveRemote) {}

  ~GitRepoRemoveRemoteCommand() override = default;

  QString remoteName;
};

struct GitRepoRenameBranchCommand final : GitRepoCommand {
  GitRepoRenameBranchCommand()
      : GitRepoCommand(GitRepoCommandType::RenameBranch) {}

  ~GitRepoRenameBranchCommand() override = default;

  QString oldBranchName;
  QString newBranchName;
};

struct GitRepoRenameTagCommand final : GitRepoCommand {
  GitRepoRenameTagCommand() : GitRepoCommand(GitRepoCommandType::RenameTag) {}

  ~GitRepoRenameTagCommand() override = default;

  QString oldTagName;
  QString newTagName;
};

struct GitRepoResetCommand final : GitRepoCommand {
  GitRepoResetCommand() : GitRepoCommand(GitRepoCommandType::Reset) {}

  ~GitRepoResetCommand() override = default;

  QString commit;
  GitRepo::ResetType resetType = GitRepo::ResetType::Unknown;
};

struct GitRepoRevertCommand final : GitRepoCommand {
  GitRepoRevertCommand() : GitRepoCommand(GitRepoCommandType::Revert) {}

  ~GitRepoRevertCommand() override = default;

  QString commit;
  QString author;
  QString email;
  QString message;
};

struct GitRepoSetDefaultRemoteCommand final : GitRepoCommand {
  GitRepoSetDefaultRemoteCommand()
      : GitRepoCommand(GitRepoCommandType::SetDefaultRemote) {}

  ~GitRepoSetDefaultRemoteCommand() override = default;

  QString remoteName;
};

struct GitRepoAddTargetCommand final : GitRepoCommand {
  GitRepoAddTargetCommand() : GitRepoCommand(GitRepoCommandType::AddTarget) {}

  ~GitRepoAddTargetCommand() override = default;

  QString remoteName;
  QString targetName;
  QString fetchUrl;
  QString pushUrl;
};

struct GitRepoEditTargetCommand final : GitRepoCommand {
  GitRepoEditTargetCommand() : GitRepoCommand(GitRepoCommandType::EditTarget) {}

  ~GitRepoEditTargetCommand() override = default;

  QString remoteName;
  QString targetName;
  QString newFetchUrl;
  QString newPushUrl;
};

struct GitRepoDeleteTargetCommand final : GitRepoCommand {
  GitRepoDeleteTargetCommand()
      : GitRepoCommand(GitRepoCommandType::DeleteTarget) {}

  ~GitRepoDeleteTargetCommand() override = default;

  QString remoteName;
  QString targetName;
};

struct GitRepoSetDefaultTargetCommand final : GitRepoCommand {
  GitRepoSetDefaultTargetCommand()
      : GitRepoCommand(GitRepoCommandType::SetDefaultTarget) {}

  ~GitRepoSetDefaultTargetCommand() override = default;

  QString remoteName;
  QString targetName;
};

struct GitRepoSetUpstreamCommand final : GitRepoCommand {
  GitRepoSetUpstreamCommand()
      : GitRepoCommand(GitRepoCommandType::SetUpstream) {}

  ~GitRepoSetUpstreamCommand() override = default;

  QString remoteName;
  QString remoteBranchName;
  QString localBranchName;
};

struct GitRepoStageFilesCommand final : GitRepoCommand {
  GitRepoStageFilesCommand() : GitRepoCommand(GitRepoCommandType::StageFiles) {}

  ~GitRepoStageFilesCommand() override = default;

  QStringList files;
};

struct GitRepoStashCommand final : GitRepoCommand {
  GitRepoStashCommand() : GitRepoCommand(GitRepoCommandType::Stash) {}

  ~GitRepoStashCommand() override = default;
};

struct GitRepoStatusCommand final : GitRepoCommand {
  GitRepoStatusCommand() : GitRepoCommand(GitRepoCommandType::Status) {}

  ~GitRepoStatusCommand() override = default;
};

struct GitRepoGetLfsStorageStatusCommand final : GitRepoCommand {
  GitRepoGetLfsStorageStatusCommand()
      : GitRepoCommand(GitRepoCommandType::GetLfsStorageStatus) {}

  ~GitRepoGetLfsStorageStatusCommand() override = default;
};

struct GitRepoSubmoduleUpdateCommand final : GitRepoCommand {
  GitRepoSubmoduleUpdateCommand()
      : GitRepoCommand(GitRepoCommandType::SubmoduleUpdate) {}

  ~GitRepoSubmoduleUpdateCommand() override = default;

  QString submoduleName;
};

struct GitRepoUnstageFilesCommand final : GitRepoCommand {
  GitRepoUnstageFilesCommand()
      : GitRepoCommand(GitRepoCommandType::UnstageFiles) {}

  ~GitRepoUnstageFilesCommand() override = default;

  QStringList files;
};

struct GitRepoUpdateRemoteCommand final : GitRepoCommand {
  GitRepoUpdateRemoteCommand()
      : GitRepoCommand(GitRepoCommandType::UpdateRemote) {}

  ~GitRepoUpdateRemoteCommand() override = default;

  QString remoteName;
  QString newRemoteName;
  QString fetchUrl;
  QString pushUrl;
};

struct GitRepoQuitCommand final : GitRepoCommand {
  GitRepoQuitCommand() : GitRepoCommand(GitRepoCommandType::Quit) {}

  ~GitRepoQuitCommand() override = default;
};

struct GitRepoError {
  QString uuid;
  QString message;
  int error = -1;
  GitRepo::GitError gitError = GitRepo::GitError::Unknown;
};

int decodeGitError(int errorCode);

/**
 * @brief Translates a libgit2 error code into a user-friendly error message.
 *
 * This function takes the combined error code (containing both the error class
 * and the libgit2 error code) and returns a localized, user-friendly
 * description of the error.
 *
 * @param errorCode The combined error code from GitRepoError::error
 * @return QString A translated, user-friendly error message
 */
QString translateGitError(int errorCode);

struct GitRepoResult {
  QString uuid;
  GitRepoCommandType type = GitRepoCommandType::Unknown;
  QVariant value;
};

struct GitRepoContext {
  QString uuid;
  BlockingQueue<GitRepoCommand *> commandsQueue;
  QThread *workersThread = nullptr;
  ;
  git_repository *gitRepo = nullptr;
  bool quit = false;
};

} // namespace GitRepoManager::Structs

Q_DECLARE_METATYPE(GitRepo::GitError)
Q_DECLARE_METATYPE(GitRepo::BranchStatusType)
Q_DECLARE_METATYPE(GitRepo::BranchStatus)
Q_DECLARE_METATYPE(GitRepo::BranchInfo)
Q_DECLARE_METATYPE(QVector<GitRepo::BranchInfo>)
Q_DECLARE_METATYPE(GitRepo::SubmoduleStatusType)
Q_DECLARE_METATYPE(GitRepo::SubmoduleStatus)
Q_DECLARE_METATYPE(GitRepo::SubmoduleInfo)
Q_DECLARE_METATYPE(QVector<GitRepo::SubmoduleInfo>)
Q_DECLARE_METATYPE(GitRepo::RemoteTarget)
Q_DECLARE_METATYPE(QVector<GitRepo::RemoteTarget>)
Q_DECLARE_METATYPE(GitRepo::RemoteInfo)
Q_DECLARE_METATYPE(QVector<GitRepo::RemoteInfo>)
Q_DECLARE_METATYPE(GitRepo::BlameLineInfo)
Q_DECLARE_METATYPE(GitRepo::BlameFileInfo)
Q_DECLARE_METATYPE(GitRepo::BlameReport)
Q_DECLARE_METATYPE(GitRepoManager::Structs::GitRepoBlameCommand)
Q_DECLARE_METATYPE(GitRepoManager::Structs::GitRepoBlameCommandResult)
