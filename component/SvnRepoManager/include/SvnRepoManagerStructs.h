#pragma once

#include <SvnRepo.h>

#include <BlockingQueue.h>

#include <QString>
#include <QThread>
#include <QVariant>

namespace SvnRepoManager::Structs {

enum class SvnRepoCommandType : int {
  Unknown = 0,
  AbortMerge,
  AddSubmodule,
  AddRemote,
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
  ListRemoteTags,
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
  ResolveTreeConflict,
  CherryPick,
  SetDefaultRemote,
  AddTarget,
  EditTarget,
  DeleteTarget,
  SetDefaultTarget,
  IgnoreFiles,
  SetUpstream,
  StageFiles,
  Stash,
  Status,
  SubmoduleUpdate,
  UnstageFiles,
  UpdateRemote,
  Blame,
  Quit,
  Last
};

enum class SvnRepoErrorType : int {
  Unknown = 0,
  AlreadyExist,
  ErrorWhileOpening,
  InternalError,
  NotCloned,
  NotInitialized,
  NotOpened,
  Last
};

struct SvnRepoCommand {
  explicit SvnRepoCommand(const SvnRepoCommandType commandType)
      : type(commandType) {}

  virtual ~SvnRepoCommand() = default;
  SvnRepoCommandType type;
  QString uuid;
  QString syncUuid;
};

struct SvnRepoBlameCommand final : SvnRepoCommand {
  SvnRepoBlameCommand() : SvnRepoCommand(SvnRepoCommandType::Blame) {}

  ~SvnRepoBlameCommand() override = default;

  QStringList files;
  QString commit;
};

struct SvnRepoBlameCommandResult {
  SvnRepo::BlameReport report;
};

struct SvnRepoAbortMergeCommand final : SvnRepoCommand {
  SvnRepoAbortMergeCommand() : SvnRepoCommand(SvnRepoCommandType::AbortMerge) {}

  ~SvnRepoAbortMergeCommand() override = default;
};

struct SvnRepoAddRemoteCommand final : SvnRepoCommand {
  SvnRepoAddRemoteCommand() : SvnRepoCommand(SvnRepoCommandType::AddRemote) {}

  ~SvnRepoAddRemoteCommand() override = default;

  QString remoteName;
  QString fetchUrl;
  QString pushUrl;
};

struct SvnRepoAddSubmoduleCommand final : SvnRepoCommand {
  SvnRepoAddSubmoduleCommand()
      : SvnRepoCommand(SvnRepoCommandType::AddSubmodule) {}

  ~SvnRepoAddSubmoduleCommand() override = default;

  QString url;
  QString name;
  QString branch;
};

struct SvnRepoApplyPatchCommand final : SvnRepoCommand {
  SvnRepoApplyPatchCommand() : SvnRepoCommand(SvnRepoCommandType::ApplyPatch) {}

  ~SvnRepoApplyPatchCommand() override = default;

  QString patchContent;
};

struct SvnRepoApplyStashCommand final : SvnRepoCommand {
  SvnRepoApplyStashCommand() : SvnRepoCommand(SvnRepoCommandType::ApplyStash) {}

  ~SvnRepoApplyStashCommand() override = default;

  int index = -1;
};

struct SvnRepoCheckoutBranchCommand final : SvnRepoCommand {
  SvnRepoCheckoutBranchCommand()
      : SvnRepoCommand(SvnRepoCommandType::CheckoutBranch) {}

  ~SvnRepoCheckoutBranchCommand() override = default;

  QString branchName;
  SvnRepo::BranchType branchType = SvnRepo::BranchType::Unknown;
};

struct SvnRepoCheckoutBranchCommandResult {
  QString branchName;
  SvnRepo::BranchType branchType = SvnRepo::BranchType::Unknown;
};

struct SvnRepoCheckoutCommitCommand final : SvnRepoCommand {
  SvnRepoCheckoutCommitCommand()
      : SvnRepoCommand(SvnRepoCommandType::CheckoutCommit) {}

  ~SvnRepoCheckoutCommitCommand() override = default;

  QString commit;
};

struct SvnRepoCherryPickCommand final : SvnRepoCommand {
  SvnRepoCherryPickCommand() : SvnRepoCommand(SvnRepoCommandType::CherryPick) {}

  ~SvnRepoCherryPickCommand() override = default;

  QString commit;
  QString author;
  QString email;
};

struct SvnRepoCloneCommand final : SvnRepoCommand {
  SvnRepoCloneCommand() : SvnRepoCommand(SvnRepoCommandType::Clone) {}

  ~SvnRepoCloneCommand() override = default;

  QString url;
  QString destinationDir;
};

struct SvnRepoCloseCommand final : SvnRepoCommand {
  SvnRepoCloseCommand() : SvnRepoCommand(SvnRepoCommandType::Close) {}

  ~SvnRepoCloseCommand() override = default;
};

struct SvnRepoCommitStagedCommand final : SvnRepoCommand {
  SvnRepoCommitStagedCommand()
      : SvnRepoCommand(SvnRepoCommandType::CommitStaged) {}

  ~SvnRepoCommitStagedCommand() override = default;

  QString author;
  QString email;
  QString summary;
  QString message;
  bool cleanup = false;
  bool signoff = false;
  bool signCommit = false;
};

struct SvnRepoCreateBranchFromCommitCommand final : SvnRepoCommand {
  SvnRepoCreateBranchFromCommitCommand()
      : SvnRepoCommand(SvnRepoCommandType::CreateBranchFromCommit) {}

  ~SvnRepoCreateBranchFromCommitCommand() override = default;

  QString branchName;
  QString commit;
};

struct SvnRepoCreatePatchFromCommitCommand final : SvnRepoCommand {
  SvnRepoCreatePatchFromCommitCommand()
      : SvnRepoCommand(SvnRepoCommandType::CreatePatchFromCommit) {}

  ~SvnRepoCreatePatchFromCommitCommand() override = default;

  QString commit;
};

struct SvnRepoCreatePatchFromCommitCommandResult {
  QString patchContent;
};

struct SvnRepoCreateTagFromCommitCommand final : SvnRepoCommand {
  SvnRepoCreateTagFromCommitCommand()
      : SvnRepoCommand(SvnRepoCommandType::CreateTagFromCommit) {}

  ~SvnRepoCreateTagFromCommitCommand() override = default;

  QString tagName;
  QString commit;
  QString author;
  QString email;
  QString message;
};

struct SvnRepoDeleteBranchCommand final : SvnRepoCommand {
  SvnRepoDeleteBranchCommand()
      : SvnRepoCommand(SvnRepoCommandType::DeleteBranch) {}

  ~SvnRepoDeleteBranchCommand() override = default;

  QString branchName;
};

struct SvnRepoDeleteRemoteBranchCommand final : SvnRepoCommand {
  SvnRepoDeleteRemoteBranchCommand()
      : SvnRepoCommand(SvnRepoCommandType::DeleteRemoteBranch) {}

  ~SvnRepoDeleteRemoteBranchCommand() override = default;

  QString branchName;
};

struct SvnRepoDeleteStashCommand final : SvnRepoCommand {
  SvnRepoDeleteStashCommand()
      : SvnRepoCommand(SvnRepoCommandType::DeleteStash) {}

  ~SvnRepoDeleteStashCommand() override = default;

  int index = -1;
};

struct SvnRepoDeleteAllStashesCommand final : SvnRepoCommand {
  SvnRepoDeleteAllStashesCommand()
      : SvnRepoCommand(SvnRepoCommandType::DeleteAllStashes) {}

  ~SvnRepoDeleteAllStashesCommand() override = default;
};

struct SvnRepoGetStashStatusCommand final : SvnRepoCommand {
  SvnRepoGetStashStatusCommand()
      : SvnRepoCommand(SvnRepoCommandType::GetStashStatus) {}

  ~SvnRepoGetStashStatusCommand() override = default;

  int index = -1;
};

struct SvnRepoDeleteSubmoduleCommand final : SvnRepoCommand {
  SvnRepoDeleteSubmoduleCommand()
      : SvnRepoCommand(SvnRepoCommandType::DeleteSubmodule) {}

  ~SvnRepoDeleteSubmoduleCommand() override = default;

  QString submoduleName;
  QString submodulePath;
};

struct SvnRepoDeleteTagCommand final : SvnRepoCommand {
  SvnRepoDeleteTagCommand() : SvnRepoCommand(SvnRepoCommandType::DeleteTag) {}

  ~SvnRepoDeleteTagCommand() override = default;

  QString tagName;
};

struct SvnRepoDeleteRemoteTagCommand final : SvnRepoCommand {
  SvnRepoDeleteRemoteTagCommand()
      : SvnRepoCommand(SvnRepoCommandType::DeleteRemoteTag) {}

  ~SvnRepoDeleteRemoteTagCommand() override = default;

  QString tagName;
};

struct SvnRepoDiffFilesCommand final : SvnRepoCommand {
  SvnRepoDiffFilesCommand() : SvnRepoCommand(SvnRepoCommandType::DiffFiles) {}

  ~SvnRepoDiffFilesCommand() override = default;

  QStringList files;
};

struct SvnRepoDiffFilesCommandResult {
  QString diff;
  QMap<QString, QString> filesContent;
  QMap<QString, QString> repoContent;
  QVector<SvnRepo::FilesDiffDelta> deltas;
};

struct SvnRepoDiffFilesFromCommitCommand final : SvnRepoCommand {
  SvnRepoDiffFilesFromCommitCommand()
      : SvnRepoCommand(SvnRepoCommandType::DiffFilesFromCommit) {}

  ~SvnRepoDiffFilesFromCommitCommand() override = default;

  QStringList files;
  QString commit;
};

struct SvnRepoDiffFilesFromCommitCommandResult {
  QString diff;
  QMap<QString, QString> filesContent;
  QMap<QString, QString> repoContent;
  QVector<SvnRepo::FilesDiffDelta> deltas;
};

struct SvnRepoDiscardFilesCommand final : SvnRepoCommand {
  SvnRepoDiscardFilesCommand()
      : SvnRepoCommand(SvnRepoCommandType::DiscardFiles) {}

  ~SvnRepoDiscardFilesCommand() override = default;

  QStringList files;
};

struct SvnRepoFetchAllCommand final : SvnRepoCommand {
  SvnRepoFetchAllCommand() : SvnRepoCommand(SvnRepoCommandType::FetchAll) {}

  ~SvnRepoFetchAllCommand() override = default;
};

struct SvnRepoFetchRemoteCommand final : SvnRepoCommand {
  SvnRepoFetchRemoteCommand()
      : SvnRepoCommand(SvnRepoCommandType::FetchRemote) {}

  ~SvnRepoFetchRemoteCommand() override = default;

  QString remoteName;
};

struct SvnRepoGetFileSizeFromCommitCommand final : SvnRepoCommand {
  SvnRepoGetFileSizeFromCommitCommand()
      : SvnRepoCommand(SvnRepoCommandType::GetFileSizeFromCommit) {}

  ~SvnRepoGetFileSizeFromCommitCommand() override = default;

  QString filePath;
  QString commit;
};

struct SvnRepoGetFileSizeFromCommitCommandResult {
  qint64 fileSize = -1;
};

struct SvnRepoGetActiveBranchCommand final : SvnRepoCommand {
  SvnRepoGetActiveBranchCommand()
      : SvnRepoCommand(SvnRepoCommandType::GetActiveBranch) {}

  ~SvnRepoGetActiveBranchCommand() override = default;

  SvnRepo::BranchType branchType = SvnRepo::BranchType::Unknown;
};

struct SvnRepoGetActiveBranchCommandResult {
  QString branchName;
  SvnRepo::BranchType branchType = SvnRepo::BranchType::Unknown;
};

struct SvnRepoGetCommitBranchesCommand final : SvnRepoCommand {
  SvnRepoGetCommitBranchesCommand()
      : SvnRepoCommand(SvnRepoCommandType::GetCommitBranches) {}

  ~SvnRepoGetCommitBranchesCommand() override = default;

  QString commitOid;
};

struct SvnRepoGetCommitTagsCommand final : SvnRepoCommand {
  SvnRepoGetCommitTagsCommand()
      : SvnRepoCommand(SvnRepoCommandType::GetCommitTags) {}

  ~SvnRepoGetCommitTagsCommand() override = default;

  QString commitOid;
};

struct SvnRepoGetCommitHeadsCommand final : SvnRepoCommand {
  SvnRepoGetCommitHeadsCommand()
      : SvnRepoCommand(SvnRepoCommandType::GetCommitHeads) {}

  ~SvnRepoGetCommitHeadsCommand() override = default;

  QString commitOid;
};

struct SvnRepoGetCommitIsHeadCommand final : SvnRepoCommand {
  SvnRepoGetCommitIsHeadCommand()
      : SvnRepoCommand(SvnRepoCommandType::GetCommitIsHead) {}

  ~SvnRepoGetCommitIsHeadCommand() override = default;

  QString commitOid;
};

struct SvnRepoGetCommitStatusCommand final : SvnRepoCommand {
  SvnRepoGetCommitStatusCommand()
      : SvnRepoCommand(SvnRepoCommandType::GetCommitStatus) {}

  ~SvnRepoGetCommitStatusCommand() override = default;

  QString commitOid;
};

struct SvnRepoGetBranchHistoryCommand final : SvnRepoCommand {
  SvnRepoGetBranchHistoryCommand()
      : SvnRepoCommand(SvnRepoCommandType::GetBranchHistory) {}

  ~SvnRepoGetBranchHistoryCommand() override = default;

  QString branchName;
  SvnRepo::BranchType branchType = SvnRepo::BranchType::Unknown;
  int page = 0;
};

struct SvnRepoGetIsMergeInProgressCommand final : SvnRepoCommand {
  SvnRepoGetIsMergeInProgressCommand()
      : SvnRepoCommand(SvnRepoCommandType::GetIsMergeInProgress) {}

  ~SvnRepoGetIsMergeInProgressCommand() override = default;
};

struct SvnRepoGetSubmodulePathCommand final : SvnRepoCommand {
  SvnRepoGetSubmodulePathCommand()
      : SvnRepoCommand(SvnRepoCommandType::GetSubmodulePath) {}

  ~SvnRepoGetSubmodulePathCommand() override = default;

  QString submoduleName;
};

struct SvnRepoGetSubmodulePathCommandResult {
  QString submodulePath;
};

struct SvnRepoGetUpstreamCommand final : SvnRepoCommand {
  SvnRepoGetUpstreamCommand()
      : SvnRepoCommand(SvnRepoCommandType::GetUpstream) {}

  ~SvnRepoGetUpstreamCommand() override = default;

  QString localBranchName;
};

struct SvnRepoGetUpstreamCommandResult {
  QString remoteName;
  QString remoteBranchName;
};

struct SvnRepoInitRepoCommand final : SvnRepoCommand {
  SvnRepoInitRepoCommand() : SvnRepoCommand(SvnRepoCommandType::Init) {}

  ~SvnRepoInitRepoCommand() override = default;

  QString repoName;
  QString workingDir;
  QString defaultBranch;
  QString author;
  QString email;
};

struct SvnRepoListConflictsCommand final : SvnRepoCommand {
  SvnRepoListConflictsCommand()
      : SvnRepoCommand(SvnRepoCommandType::ListConflicts) {}

  ~SvnRepoListConflictsCommand() override = default;

  QString filePath;
};

struct SvnRepoListConflictsCommandResult {
  QString fileContentWithMarkers;
  bool treeConflict = false;
};

struct SvnRepoResolveTreeConflictCommand final : SvnRepoCommand {
  SvnRepoResolveTreeConflictCommand()
      : SvnRepoCommand(SvnRepoCommandType::ResolveTreeConflict) {}

  ~SvnRepoResolveTreeConflictCommand() override = default;

  QString filePath;
  bool keepLocal = false;
};

struct SvnRepoListLocalBranchesCommand final : SvnRepoCommand {
  SvnRepoListLocalBranchesCommand()
      : SvnRepoCommand(SvnRepoCommandType::ListLocalBranches) {}

  ~SvnRepoListLocalBranchesCommand() override = default;
};

struct SvnRepoListRemoteBranchesCommand final : SvnRepoCommand {
  SvnRepoListRemoteBranchesCommand()
      : SvnRepoCommand(SvnRepoCommandType::ListRemoteBranches) {}

  ~SvnRepoListRemoteBranchesCommand() override = default;
};

struct SvnRepoListRemoteBranchesWithStatusCommand final : SvnRepoCommand {
  SvnRepoListRemoteBranchesWithStatusCommand()
      : SvnRepoCommand(SvnRepoCommandType::ListRemoteBranchesWithStatus) {}

  ~SvnRepoListRemoteBranchesWithStatusCommand() override = default;
};

struct SvnRepoListRemotesCommand final : SvnRepoCommand {
  SvnRepoListRemotesCommand()
      : SvnRepoCommand(SvnRepoCommandType::ListRemotes) {}

  ~SvnRepoListRemotesCommand() override = default;
};

struct SvnRepoListStashesCommand final : SvnRepoCommand {
  SvnRepoListStashesCommand()
      : SvnRepoCommand(SvnRepoCommandType::ListStashes) {}

  ~SvnRepoListStashesCommand() override = default;
};

struct SvnRepoListSubmodulesCommand final : SvnRepoCommand {
  SvnRepoListSubmodulesCommand()
      : SvnRepoCommand(SvnRepoCommandType::ListSubmodules) {}

  ~SvnRepoListSubmodulesCommand() override = default;
};

struct SvnRepoListSubmodulesWithStatusCommand final : SvnRepoCommand {
  SvnRepoListSubmodulesWithStatusCommand()
      : SvnRepoCommand(SvnRepoCommandType::ListSubmodulesWithStatus) {}

  ~SvnRepoListSubmodulesWithStatusCommand() override = default;
};

struct SvnRepoListTagsCommand final : SvnRepoCommand {
  SvnRepoListTagsCommand() : SvnRepoCommand(SvnRepoCommandType::ListTags) {}

  ~SvnRepoListTagsCommand() override = default;
};

struct SvnRepoListRemoteTagsCommand final : SvnRepoCommand {
  SvnRepoListRemoteTagsCommand()
      : SvnRepoCommand(SvnRepoCommandType::ListRemoteTags) {}

  ~SvnRepoListRemoteTagsCommand() override = default;
};

struct SvnRepoMergeCommand final : SvnRepoCommand {
  SvnRepoMergeCommand() : SvnRepoCommand(SvnRepoCommandType::Merge) {}

  ~SvnRepoMergeCommand() override = default;

  QString sourceBranch;
  QString destinationBranch;
  QString author;
  QString email;
};

struct SvnRepoRebaseCommand final : SvnRepoCommand {
  SvnRepoRebaseCommand() : SvnRepoCommand(SvnRepoCommandType::Rebase) {}

  ~SvnRepoRebaseCommand() override = default;

  QString sourceBranch;
  QString destinationBranch;
  QString author;
  QString email;
  bool signCommit = false;
};

struct SvnRepoOpenCommand final : SvnRepoCommand {
  SvnRepoOpenCommand() : SvnRepoCommand(SvnRepoCommandType::Open) {}

  ~SvnRepoOpenCommand() override = default;
  QString path;
};

struct SvnRepoPullCommand final : SvnRepoCommand {
  SvnRepoPullCommand() : SvnRepoCommand(SvnRepoCommandType::Pull) {}

  ~SvnRepoPullCommand() override = default;

  QString activeBranchName; // The currently checked out branch name
  bool rebase = false;
};

struct SvnRepoPushCommand final : SvnRepoCommand {
  SvnRepoPushCommand() : SvnRepoCommand(SvnRepoCommandType::Push) {}

  ~SvnRepoPushCommand() override = default;

  QString activeBranchName; // The currently checked out branch name
  bool force = false;
};

struct SvnRepoPushTagCommand final : SvnRepoCommand {
  SvnRepoPushTagCommand() : SvnRepoCommand(SvnRepoCommandType::PushTag) {}

  ~SvnRepoPushTagCommand() override = default;

  QString tagName;
};

struct SvnRepoPushAllTagsCommand final : SvnRepoCommand {
  SvnRepoPushAllTagsCommand()
      : SvnRepoCommand(SvnRepoCommandType::PushAllTags) {}

  ~SvnRepoPushAllTagsCommand() override = default;
};

struct SvnRepoRemoveRemoteCommand final : SvnRepoCommand {
  SvnRepoRemoveRemoteCommand()
      : SvnRepoCommand(SvnRepoCommandType::RemoveRemote) {}

  ~SvnRepoRemoveRemoteCommand() override = default;

  QString remoteName;
};

struct SvnRepoRenameBranchCommand final : SvnRepoCommand {
  SvnRepoRenameBranchCommand()
      : SvnRepoCommand(SvnRepoCommandType::RenameBranch) {}

  ~SvnRepoRenameBranchCommand() override = default;

  QString oldBranchName;
  QString newBranchName;
};

struct SvnRepoRenameTagCommand final : SvnRepoCommand {
  SvnRepoRenameTagCommand() : SvnRepoCommand(SvnRepoCommandType::RenameTag) {}

  ~SvnRepoRenameTagCommand() override = default;

  QString oldTagName;
  QString newTagName;
};

struct SvnRepoResetCommand final : SvnRepoCommand {
  SvnRepoResetCommand() : SvnRepoCommand(SvnRepoCommandType::Reset) {}

  ~SvnRepoResetCommand() override = default;

  QString commit;
  SvnRepo::ResetType resetType = SvnRepo::ResetType::Unknown;
};

struct SvnRepoRevertCommand final : SvnRepoCommand {
  SvnRepoRevertCommand() : SvnRepoCommand(SvnRepoCommandType::Revert) {}

  ~SvnRepoRevertCommand() override = default;

  QString commit;
  QString author;
  QString email;
  QString message;
};

struct SvnRepoSetDefaultRemoteCommand final : SvnRepoCommand {
  SvnRepoSetDefaultRemoteCommand()
      : SvnRepoCommand(SvnRepoCommandType::SetDefaultRemote) {}

  ~SvnRepoSetDefaultRemoteCommand() override = default;

  QString remoteName;
};

struct SvnRepoAddTargetCommand final : SvnRepoCommand {
  SvnRepoAddTargetCommand() : SvnRepoCommand(SvnRepoCommandType::AddTarget) {}

  ~SvnRepoAddTargetCommand() override = default;

  QString remoteName;
  QString targetName;
  QString fetchUrl;
  QString pushUrl;
};

struct SvnRepoEditTargetCommand final : SvnRepoCommand {
  SvnRepoEditTargetCommand() : SvnRepoCommand(SvnRepoCommandType::EditTarget) {}

  ~SvnRepoEditTargetCommand() override = default;

  QString remoteName;
  QString targetName;
  QString newFetchUrl;
  QString newPushUrl;
};

struct SvnRepoDeleteTargetCommand final : SvnRepoCommand {
  SvnRepoDeleteTargetCommand()
      : SvnRepoCommand(SvnRepoCommandType::DeleteTarget) {}

  ~SvnRepoDeleteTargetCommand() override = default;

  QString remoteName;
  QString targetName;
};

struct SvnRepoSetDefaultTargetCommand final : SvnRepoCommand {
  SvnRepoSetDefaultTargetCommand()
      : SvnRepoCommand(SvnRepoCommandType::SetDefaultTarget) {}

  ~SvnRepoSetDefaultTargetCommand() override = default;

  QString remoteName;
  QString targetName;
};

struct SvnRepoSetUpstreamCommand final : SvnRepoCommand {
  SvnRepoSetUpstreamCommand()
      : SvnRepoCommand(SvnRepoCommandType::SetUpstream) {}

  ~SvnRepoSetUpstreamCommand() override = default;

  QString remoteName;
  QString remoteBranchName;
  QString localBranchName;
};

struct SvnRepoIgnoreFilesCommand final : SvnRepoCommand {
  SvnRepoIgnoreFilesCommand()
      : SvnRepoCommand(SvnRepoCommandType::IgnoreFiles) {}

  ~SvnRepoIgnoreFilesCommand() override = default;

  QStringList files;
  bool recursive = false;
};

struct SvnRepoStageFilesCommand final : SvnRepoCommand {
  SvnRepoStageFilesCommand() : SvnRepoCommand(SvnRepoCommandType::StageFiles) {}

  ~SvnRepoStageFilesCommand() override = default;

  QStringList files;
};

struct SvnRepoStashCommand final : SvnRepoCommand {
  SvnRepoStashCommand() : SvnRepoCommand(SvnRepoCommandType::Stash) {}

  ~SvnRepoStashCommand() override = default;

  QString note;
};

struct SvnRepoStatusCommand final : SvnRepoCommand {
  SvnRepoStatusCommand() : SvnRepoCommand(SvnRepoCommandType::Status) {}

  ~SvnRepoStatusCommand() override = default;
};

struct SvnRepoSubmoduleUpdateCommand final : SvnRepoCommand {
  SvnRepoSubmoduleUpdateCommand()
      : SvnRepoCommand(SvnRepoCommandType::SubmoduleUpdate) {}

  ~SvnRepoSubmoduleUpdateCommand() override = default;

  QString submoduleName;
};

struct SvnRepoUnstageFilesCommand final : SvnRepoCommand {
  SvnRepoUnstageFilesCommand()
      : SvnRepoCommand(SvnRepoCommandType::UnstageFiles) {}

  ~SvnRepoUnstageFilesCommand() override = default;

  QStringList files;
};

struct SvnRepoUpdateRemoteCommand final : SvnRepoCommand {
  SvnRepoUpdateRemoteCommand()
      : SvnRepoCommand(SvnRepoCommandType::UpdateRemote) {}

  ~SvnRepoUpdateRemoteCommand() override = default;

  QString remoteName;
  QString newRemoteName;
  QString fetchUrl;
  QString pushUrl;
};

struct SvnRepoQuitCommand final : SvnRepoCommand {
  SvnRepoQuitCommand() : SvnRepoCommand(SvnRepoCommandType::Quit) {}

  ~SvnRepoQuitCommand() override = default;
};

struct SvnRepoError {
  QString uuid;
  QString message;
  int error = -1;
  SvnRepo::SvnError gitError = SvnRepo::SvnError::Unknown;
};

int decodeSvnError(int errorCode);

/**
 * @brief Translates a libgit2 error code into a user-friendly error message.
 *
 * This function takes the combined error code (containing both the error class
 * and the libgit2 error code) and returns a localized, user-friendly
 * description of the error.
 *
 * @param errorCode The combined error code from SvnRepoError::error
 * @return QString A translated, user-friendly error message
 */
QString translateSvnError(int errorCode);

struct SvnRepoResult {
  QString uuid;
  SvnRepoCommandType type = SvnRepoCommandType::Unknown;
  QVariant value;
};

struct SvnRepoContext {
  QString uuid;
  // Sync uuid of the request that made the context (open, clone or init) and
  // of its close. Only their answers end the context; a refused request is
  // answered with the same event types.
  QString ownerSyncUuid;
  QString closeSyncUuid;
  BlockingQueue<SvnRepoCommand *> commandsQueue;
  QThread *workersThread = nullptr;
  QString SvnRepo;
  bool quit = false;
};

} // namespace SvnRepoManager::Structs

Q_DECLARE_METATYPE(SvnRepo::SvnError)
Q_DECLARE_METATYPE(SvnRepo::BlameReport)
Q_DECLARE_METATYPE(SvnRepoManager::Structs::SvnRepoBlameCommand)
Q_DECLARE_METATYPE(SvnRepoManager::Structs::SvnRepoBlameCommandResult)
Q_DECLARE_METATYPE(SvnRepo::SvnStatus)
Q_DECLARE_METATYPE(SvnRepo::SvnHistory)
Q_DECLARE_METATYPE(SvnRepo::CommitStatus)
Q_DECLARE_METATYPE(SvnRepo::FilesDiff)
Q_DECLARE_METATYPE(SvnRepo::BranchStatusType)
Q_DECLARE_METATYPE(SvnRepo::BranchStatus)
Q_DECLARE_METATYPE(SvnRepo::BranchInfo)
Q_DECLARE_METATYPE(QVector<SvnRepo::BranchInfo>)
Q_DECLARE_METATYPE(SvnRepo::SubmoduleStatusType)
Q_DECLARE_METATYPE(SvnRepo::SubmoduleStatus)
Q_DECLARE_METATYPE(SvnRepo::SubmoduleInfo)
Q_DECLARE_METATYPE(QVector<SvnRepo::SubmoduleInfo>)
Q_DECLARE_METATYPE(SvnRepo::RemoteTarget)
Q_DECLARE_METATYPE(QVector<SvnRepo::RemoteTarget>)
Q_DECLARE_METATYPE(SvnRepo::RemoteInfo)
Q_DECLARE_METATYPE(QVector<SvnRepo::RemoteInfo>)
