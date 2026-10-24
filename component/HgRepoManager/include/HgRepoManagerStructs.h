#pragma once

#include <Events.h>
#include <HgRepo.h>

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

#include <memory>

// Requests of HgRepoManager and their replies. A request is sent as the event
// HgRepoManager<Name> with an HgRepo<Name>Command, and answered with
// HgRepoManager<Name>Result carrying an HgRepoResult, or
// HgRepoManager<Name>Error carrying an HgRepoError. Both replies have the
// command's syncUuid as event uuid. uuid identifies the repository: the
// RepoContainer that opened it.
namespace HgRepoManager::Structs {

enum class HgRepoCommandType : int {
  Unknown = 0,
#define HG_REPO_COMMAND_TYPE(X, name) name,
  HG_REPO_MANAGER_COMMAND_LIST(HG_REPO_COMMAND_TYPE, unused)
#undef HG_REPO_COMMAND_TYPE
  // Internal: ends a worker thread.
  Quit,
  Last
};

struct HgRepoCommand {
  explicit HgRepoCommand(const HgRepoCommandType commandType)
      : type(commandType) {}
  virtual ~HgRepoCommand() = default;

  HgRepoCommandType type;
  QString uuid;
  QString syncUuid;
};

using HgRepoCommandPtr = std::shared_ptr<HgRepoCommand>;

template <HgRepoCommandType Type> struct HgRepoCommandOf : HgRepoCommand {
  HgRepoCommandOf() : HgRepoCommand(Type) {}
};

using Type = HgRepoCommandType;

// Commands without parameters.
struct HgRepoAbortMergeCommand final : HgRepoCommandOf<Type::AbortMerge> {};
struct HgRepoAbortRebaseCommand final : HgRepoCommandOf<Type::AbortRebase> {};
struct HgRepoCloseRepoCommand final : HgRepoCommandOf<Type::CloseRepo> {};
struct HgRepoDeleteAllStashesCommand final
    : HgRepoCommandOf<Type::DeleteAllStashes> {};
struct HgRepoEnableLfsCommand final : HgRepoCommandOf<Type::EnableLfs> {};
struct HgRepoFetchAllCommand final : HgRepoCommandOf<Type::FetchAll> {};
struct HgRepoGetIsMergeInProgressCommand final
    : HgRepoCommandOf<Type::GetIsMergeInProgress> {};
struct HgRepoGetIsRebaseInProgressCommand final
    : HgRepoCommandOf<Type::GetIsRebaseInProgress> {};
struct HgRepoGetLfsStorageStatusCommand final
    : HgRepoCommandOf<Type::GetLfsStorageStatus> {};
struct HgRepoListLfsFilesCommand final : HgRepoCommandOf<Type::ListLfsFiles> {};
struct HgRepoListRemoteBranchesCommand final
    : HgRepoCommandOf<Type::ListRemoteBranches> {};
struct HgRepoListRemoteBranchesWithStatusCommand final
    : HgRepoCommandOf<Type::ListRemoteBranchesWithStatus> {};
struct HgRepoListRemotesCommand final : HgRepoCommandOf<Type::ListRemotes> {};
struct HgRepoListStashesCommand final : HgRepoCommandOf<Type::ListStashes> {};
struct HgRepoListSubmodulesCommand final
    : HgRepoCommandOf<Type::ListSubmodules> {};
struct HgRepoListSubmodulesWithStatusCommand final
    : HgRepoCommandOf<Type::ListSubmodulesWithStatus> {};
struct HgRepoListTagsCommand final : HgRepoCommandOf<Type::ListTags> {};
struct HgRepoListWorktreesCommand final : HgRepoCommandOf<Type::ListWorktrees> {
};
struct HgRepoPushAllTagsCommand final : HgRepoCommandOf<Type::PushAllTags> {};
struct HgRepoStatusCommand final : HgRepoCommandOf<Type::Status> {};
struct HgRepoQuitCommand final : HgRepoCommandOf<Type::Quit> {};

// Repository.
struct HgRepoOpenRepoCommand final : HgRepoCommandOf<Type::OpenRepo> {
  QString path;
  QString author;
  QString email;
};

struct HgRepoCloneRepoCommand final : HgRepoCommandOf<Type::CloneRepo> {
  QString url;
  QString destinationDir;
};

struct HgRepoInitRepoCommand final : HgRepoCommandOf<Type::InitRepo> {
  QString repoName;
  QString workingDir;
  QString defaultBranch;
  QString author;
  QString email;
};

// Working directory.
struct HgRepoStageFilesCommand final : HgRepoCommandOf<Type::StageFiles> {
  QStringList files;
};

struct HgRepoUnstageFilesCommand final : HgRepoCommandOf<Type::UnstageFiles> {
  QStringList files;
};

struct HgRepoDiscardFilesCommand final : HgRepoCommandOf<Type::DiscardFiles> {
  QStringList files;
};

struct HgRepoMarkResolvedCommand final : HgRepoCommandOf<Type::MarkResolved> {
  QStringList files;
};

struct HgRepoCommitStagedCommand final : HgRepoCommandOf<Type::CommitStaged> {
  QString author;
  QString email;
  QString summary;
  QString message;
  bool cleanup = false;
  bool signoff = false;
  bool signCommit = false;
};

struct HgRepoAmendCommand final : HgRepoCommandOf<Type::Amend> {
  QString author;
  QString email;
  QString summary;
  QString message;
  bool signoff = false;
  bool signCommit = false;
};

struct HgRepoApplyPatchCommand final : HgRepoCommandOf<Type::ApplyPatch> {
  QString patchContent;
};

struct HgRepoListConflictsCommand final : HgRepoCommandOf<Type::ListConflicts> {
  QString filePath;
};

// Diffs and blame.
struct HgRepoDiffFilesCommand final : HgRepoCommandOf<Type::DiffFiles> {
  QStringList files;
};

struct HgRepoDiffFilesFromCommitCommand final
    : HgRepoCommandOf<Type::DiffFilesFromCommit> {
  QStringList files;
  QString commit;
};

struct HgRepoBlameCommand final : HgRepoCommandOf<Type::Blame> {
  QStringList files;
};

struct HgRepoGetFileSizeFromCommitCommand final
    : HgRepoCommandOf<Type::GetFileSizeFromCommit> {
  QString filePath;
  QString commit;
};

struct HgRepoCreatePatchFromCommitCommand final
    : HgRepoCommandOf<Type::CreatePatchFromCommit> {
  QString commit;
};

// History.
struct HgRepoGetBranchHistoryCommand final
    : HgRepoCommandOf<Type::GetBranchHistory> {
  QString branchName;
  GitRepo::BranchType branchType = GitRepo::BranchType::Unknown;
  int page = 0;
};

struct HgRepoGetCommitStatusCommand final
    : HgRepoCommandOf<Type::GetCommitStatus> {
  QString commitOid;
};

struct HgRepoGetCommitBranchesCommand final
    : HgRepoCommandOf<Type::GetCommitBranches> {
  QString commitOid;
};

struct HgRepoGetCommitTagsCommand final : HgRepoCommandOf<Type::GetCommitTags> {
  QString commitOid;
};

struct HgRepoGetCommitHeadsCommand final
    : HgRepoCommandOf<Type::GetCommitHeads> {
  QString commitOid;
};

struct HgRepoGetCommitIsHeadCommand final
    : HgRepoCommandOf<Type::GetCommitIsHead> {
  QString commitOid;
};

// Branches and bookmarks.
struct HgRepoGetActiveBranchCommand final
    : HgRepoCommandOf<Type::GetActiveBranch> {
  GitRepo::BranchType branchType = GitRepo::BranchType::Local;
};

struct HgRepoListLocalBranchesCommand final
    : HgRepoCommandOf<Type::ListLocalBranches> {
  bool includeClosed = false;
};

struct HgRepoCheckoutBranchCommand final
    : HgRepoCommandOf<Type::CheckoutBranch> {
  QString branchName;
  GitRepo::BranchType branchType = GitRepo::BranchType::Unknown;
};

struct HgRepoCheckoutCommitCommand final
    : HgRepoCommandOf<Type::CheckoutCommit> {
  QString commit;
};

struct HgRepoCreateBranchFromCommitCommand final
    : HgRepoCommandOf<Type::CreateBranchFromCommit> {
  QString branchName;
  QString commit;
  HgRepo::HgBranchKind kind = HgRepo::HgBranchKind::Bookmark;
};

struct HgRepoCloseNamedBranchCommand final
    : HgRepoCommandOf<Type::CloseNamedBranch> {
  QString branchName;
  QString author;
  QString email;
};

struct HgRepoRenameBranchCommand final : HgRepoCommandOf<Type::RenameBranch> {
  QString oldBranchName;
  QString newBranchName;
};

struct HgRepoDeleteBranchCommand final : HgRepoCommandOf<Type::DeleteBranch> {
  QString branchName;
};

struct HgRepoDeleteRemoteBranchCommand final
    : HgRepoCommandOf<Type::DeleteRemoteBranch> {
  QString branchName;
};

// Tags.
struct HgRepoCreateTagFromCommitCommand final
    : HgRepoCommandOf<Type::CreateTagFromCommit> {
  QString tagName;
  QString commit;
  QString author;
  QString email;
  QString message;
  bool local = false;
};

struct HgRepoDeleteTagCommand final : HgRepoCommandOf<Type::DeleteTag> {
  QString tagName;
};

struct HgRepoRenameTagCommand final : HgRepoCommandOf<Type::RenameTag> {
  QString oldTagName;
  QString newTagName;
};

struct HgRepoPushTagCommand final : HgRepoCommandOf<Type::PushTag> {
  QString tagName;
};

struct HgRepoDeleteRemoteTagCommand final
    : HgRepoCommandOf<Type::DeleteRemoteTag> {
  QString tagName;
};

// Merging and history editing.
struct HgRepoMergeCommand final : HgRepoCommandOf<Type::Merge> {
  QString sourceBranch;
  QString destinationBranch;
  QString author;
  QString email;
};

struct HgRepoRebaseCommand final : HgRepoCommandOf<Type::Rebase> {
  QString sourceBranch;
  QString destinationBranch;
  QString author;
  QString email;
  bool signCommit = false;
};

struct HgRepoSquashCommand final : HgRepoCommandOf<Type::Squash> {
  QString sourceBranch;
  QString destinationBranch;
  QString author;
  QString email;
  bool signCommit = false;
};

struct HgRepoCommitRebaseCommand final : HgRepoCommandOf<Type::CommitRebase> {
  QString author;
  QString email;
  QString summary;
  QString message;
  bool cleanup = false;
  bool signoff = false;
  bool signCommit = false;
};

struct HgRepoCherryPickCommand final : HgRepoCommandOf<Type::CherryPick> {
  QString commit;
  QString author;
  QString email;
};

struct HgRepoRevertCommand final : HgRepoCommandOf<Type::Revert> {
  QString commit;
  QString author;
  QString email;
  QString message;
};

struct HgRepoStripCommand final : HgRepoCommandOf<Type::Strip> {
  QString commit;
  bool keepWorkingCopy = false;
  bool backup = true;
};

struct HgRepoResetCommand final : HgRepoCommandOf<Type::Reset> {
  QString commit;
  GitRepo::ResetType resetType = GitRepo::ResetType::Unknown;
};

struct HgRepoBisectCommand final : HgRepoCommandOf<Type::Bisect> {
  GitRepo::BisectAction action = GitRepo::BisectAction::Reset;
  QString commit;
};

// Shelves.
struct HgRepoStashCommand final : HgRepoCommandOf<Type::Stash> {
  QString note;
};

struct HgRepoApplyStashCommand final : HgRepoCommandOf<Type::ApplyStash> {
  int index = -1;
};

struct HgRepoDeleteStashCommand final : HgRepoCommandOf<Type::DeleteStash> {
  int index = -1;
};

struct HgRepoGetStashStatusCommand final
    : HgRepoCommandOf<Type::GetStashStatus> {
  int index = -1;
};

// Remotes and synchronization.
struct HgRepoAddRemoteCommand final : HgRepoCommandOf<Type::AddRemote> {
  QString remoteName;
  QString fetchUrl;
  QString pushUrl;
};

struct HgRepoRemoveRemoteCommand final : HgRepoCommandOf<Type::RemoveRemote> {
  QString remoteName;
};

struct HgRepoUpdateRemoteCommand final : HgRepoCommandOf<Type::UpdateRemote> {
  QString remoteName;
  QString newRemoteName;
  QString fetchUrl;
  QString pushUrl;
};

struct HgRepoSetDefaultRemoteCommand final
    : HgRepoCommandOf<Type::SetDefaultRemote> {
  QString remoteName;
};

struct HgRepoAddTargetCommand final : HgRepoCommandOf<Type::AddTarget> {
  QString remoteName;
  QString targetName;
  QString fetchUrl;
  QString pushUrl;
};

struct HgRepoEditTargetCommand final : HgRepoCommandOf<Type::EditTarget> {
  QString remoteName;
  QString targetName;
  QString newFetchUrl;
  QString newPushUrl;
};

struct HgRepoDeleteTargetCommand final : HgRepoCommandOf<Type::DeleteTarget> {
  QString remoteName;
  QString targetName;
};

struct HgRepoSetDefaultTargetCommand final
    : HgRepoCommandOf<Type::SetDefaultTarget> {
  QString remoteName;
  QString targetName;
};

struct HgRepoPullCommand final : HgRepoCommandOf<Type::Pull> {
  // The active branch when empty.
  QString branchName;
  bool rebase = false;
};

struct HgRepoPushCommand final : HgRepoCommandOf<Type::Push> {
  // The active branch when empty.
  QString branchName;
  bool force = false;
};

struct HgRepoFetchRemoteCommand final : HgRepoCommandOf<Type::FetchRemote> {
  QString remoteName;
};

struct HgRepoSetUpstreamCommand final : HgRepoCommandOf<Type::SetUpstream> {
  QString remoteName;
  QString remoteBranchName;
  QString localBranchName;
};

struct HgRepoGetUpstreamCommand final : HgRepoCommandOf<Type::GetUpstream> {
  QString localBranchName;
};

// Subrepos.
struct HgRepoAddSubmoduleCommand final : HgRepoCommandOf<Type::AddSubmodule> {
  QString url;
  // Path of the subrepo, relative to the repository root.
  QString name;
  // Branch to check out; for Subversion a path below url, e.g. "trunk".
  QString branch;
  HgRepo::HgSubrepoKind kind = HgRepo::HgSubrepoKind::Mercurial;
};

struct HgRepoDeleteSubmoduleCommand final
    : HgRepoCommandOf<Type::DeleteSubmodule> {
  QString submoduleName;
  QString submodulePath;
};

struct HgRepoSubmoduleUpdateCommand final
    : HgRepoCommandOf<Type::SubmoduleUpdate> {
  QString submoduleName;
};

struct HgRepoGetSubmodulePathCommand final
    : HgRepoCommandOf<Type::GetSubmodulePath> {
  QString submoduleName;
};

// Worktrees.
struct HgRepoAddWorktreeCommand final : HgRepoCommandOf<Type::AddWorktree> {
  QString name;
  QString path;
  QString branch;
  bool createBranch = false;
  QString baseBranch;
  HgRepo::HgBranchKind kind = HgRepo::HgBranchKind::Bookmark;
};

struct HgRepoRemoveWorktreeCommand final
    : HgRepoCommandOf<Type::RemoveWorktree> {
  QString name;
  bool force = false;
};

struct HgRepoLockWorktreeCommand final : HgRepoCommandOf<Type::LockWorktree> {
  QString name;
  QString reason;
};

struct HgRepoUnlockWorktreeCommand final
    : HgRepoCommandOf<Type::UnlockWorktree> {
  QString name;
};

// LFS.
struct HgRepoAddFilesToLfsCommand final : HgRepoCommandOf<Type::AddFilesToLfs> {
  QStringList files;
};

struct HgRepoRemoveFilesFromLfsCommand final
    : HgRepoCommandOf<Type::RemoveFilesFromLfs> {
  QStringList files;
};

// Workflows.
struct HgRepoGitFlowCommand final : HgRepoCommandOf<Type::GitFlow> {
  GitRepo::WorkflowType workflow = GitRepo::WorkflowType::GitFlow;
  // Classic Git Flow.
  GitRepo::GitFlowBranchType branchType = GitRepo::GitFlowBranchType::Unknown;
  bool start = true;
  QString developmentBranchName;
  // GitLab Flow, GitHub Flow and git-flow-next.
  GitRepo::GitLabFlowAction gitLabFlowAction =
      GitRepo::GitLabFlowAction::Unknown;
  GitRepo::GitHubFlowAction gitHubFlowAction =
      GitRepo::GitHubFlowAction::Unknown;
  GitRepo::GitFlowNextAction gitFlowNextAction =
      GitRepo::GitFlowNextAction::Unknown;
  // The topic, release or environment branch, depending on the action.
  QString branchName;
  QString mainBranchName;
  QString topicType;
  QStringList environmentChain;
  QString commit;
  GitRepo::GitFlowNextWorkflow customWorkflow;
  QString author;
  QString email;
};

struct HgRepoWorkflowCommand final : HgRepoCommandOf<Type::Workflow> {
  // Writes workflow (and flowBranchKind with writeFlowBranchKind) before
  // reading the state.
  bool write = false;
  GitRepo::WorkflowType workflow = GitRepo::WorkflowType::GitFlow;
  bool writeFlowBranchKind = false;
  HgRepo::HgBranchKind flowBranchKind = HgRepo::HgBranchKind::NamedBranch;
};

// Result values, in HgRepoResult::value, for the commands whose HgRepo call
// returns more than one value.
struct HgRepoUpstreamResult {
  bool configured = false;
  QString remoteName;
  QString remoteBranchName;
};

struct HgRepoTagsResult {
  QStringList tags;
  // The tags of tags that are local tags.
  QStringList localTags;
};

struct HgRepoSubmodulesResult {
  QVector<GitRepo::SubmoduleInfo> infos;
  QVector<HgRepo::HgSubrepo> subrepos;
};

struct HgRepoWorkflowResult {
  GitRepo::WorkflowState state;
  HgRepo::HgBranchKind flowBranchKind = HgRepo::HgBranchKind::NamedBranch;
};

struct HgRepoGitFlowResult {
  // A GitLab Flow cherry-pick to a release left conflicts to resolve.
  bool conflicted = false;
};

struct HgRepoError {
  QString uuid;
  QString message;
  int error = -1;
  HgRepo::HgError hgError = HgRepo::HgError::Unknown;
};

struct HgRepoResult {
  QString uuid;
  HgRepoCommandType type = HgRepoCommandType::Unknown;
  QVariant value;
};

} // namespace HgRepoManager::Structs
