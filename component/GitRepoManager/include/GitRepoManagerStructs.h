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
  DeleteStash,
  DiffFiles,
  DiscardFiles,
  GetActiveBranch,
  GetBranchHistory,
  GetCommitBranches,
  GetCommitHeads,
  GetCommitIsHead,
  GetCommitStatus,
  GetUpstream,
  Init,
  ListLocalBranches,
  ListRemoteBranches,
  ListStashes,
  ListSubmodules,
  ListTags,
  Open,
  Pull,
  Push,
  RenameBranch,
  RenameTag,
  Reset,
  Revert,
  SetUpstream,
  StageFiles,
  Stash,
  Status,
  UnstageFiles,
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

struct GitRepoDeleteStashCommand final : GitRepoCommand {
  GitRepoDeleteStashCommand()
      : GitRepoCommand(GitRepoCommandType::DeleteStash) {}
  ~GitRepoDeleteStashCommand() override = default;

  int index = -1;
};

struct GitRepoDiffFilesCommand final : GitRepoCommand {
  GitRepoDiffFilesCommand() : GitRepoCommand(GitRepoCommandType::DiffFiles) {}
  ~GitRepoDiffFilesCommand() override = default;

  QStringList files;
};

struct GitRepoDiscardFilesCommand final : GitRepoCommand {
  GitRepoDiscardFilesCommand()
      : GitRepoCommand(GitRepoCommandType::DiscardFiles) {}
  ~GitRepoDiscardFilesCommand() override = default;

  QStringList files;
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

struct GitRepoListTagsCommand final : GitRepoCommand {
  GitRepoListTagsCommand() : GitRepoCommand(GitRepoCommandType::ListTags) {}
  ~GitRepoListTagsCommand() override = default;
};

struct GitRepoOpenCommand final : GitRepoCommand {
  GitRepoOpenCommand() : GitRepoCommand(GitRepoCommandType::Open) {}
  ~GitRepoOpenCommand() override = default;
  QString path;
};

struct GitRepoPullCommand final : GitRepoCommand {
  GitRepoPullCommand() : GitRepoCommand(GitRepoCommandType::Pull) {}
  ~GitRepoPullCommand() override = default;
};

struct GitRepoPushCommand final : GitRepoCommand {
  GitRepoPushCommand() : GitRepoCommand(GitRepoCommandType::Push) {}
  ~GitRepoPushCommand() override = default;
};

struct GitRepoRenameBranchCommand final : GitRepoCommand {
  GitRepoRenameBranchCommand() : GitRepoCommand(GitRepoCommandType::RenameBranch) {}
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

struct GitRepoUnstageFilesCommand final : GitRepoCommand {
  GitRepoUnstageFilesCommand()
      : GitRepoCommand(GitRepoCommandType::UnstageFiles) {}
  ~GitRepoUnstageFilesCommand() override = default;

  QStringList files;
};

struct GitRepoQuitCommand final : GitRepoCommand {
  GitRepoQuitCommand() : GitRepoCommand(GitRepoCommandType::Quit) {}
  ~GitRepoQuitCommand() override = default;
};

struct GitRepoError {
  QString uuid;
  QString message;
  int error = -1;
};

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
