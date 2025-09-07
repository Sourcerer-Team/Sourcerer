#pragma once

#include "GitRepo.h"

#include <BlockingQueue.h>

#include <QString>
#include <QVariant>

#include <git2/repository.h>

#include <thread>

namespace GitRepoManager::Structs {

enum class GitRepoCommandType : int {
  Unknown = 0,
  Clone,
  Close,
  GetActiveBranch,
  GetBranchHistory,
  GetCommitBranches,
  GetCommitHeads,
  GetCommitIsHead,
  Init,
  ListLocalBranches,
  ListRemoteBranches,
  ListSubmodules,
  ListTags,
  Open,
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

struct GitRepoGetBranchHistoryCommand final : GitRepoCommand {
  GitRepoGetBranchHistoryCommand()
      : GitRepoCommand(GitRepoCommandType::GetBranchHistory) {}
  ~GitRepoGetBranchHistoryCommand() override = default;

  QString branchName;
  GitRepo::BranchType branchType = GitRepo::BranchType::Unknown;
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

struct GitRepoQuitCommand final : GitRepoCommand {
  GitRepoQuitCommand() : GitRepoCommand(GitRepoCommandType::Quit) {}
  ~GitRepoQuitCommand() override = default;
};

struct GitRepoError {
  QString uuid;
  QString message;
  GitRepoErrorType error = GitRepoErrorType::Unknown;
};

struct GitRepoResult {
  QString uuid;
  GitRepoCommandType type = GitRepoCommandType::Unknown;
  QVariant value;
};


struct GitRepoContext {
  QString uuid;
  BlockingQueue<GitRepoCommand *> commandsQueue;
  std::thread workersThread;
  git_repository *gitRepo = nullptr;
  bool quit = false;
};

} // namespace GitRepoManager::Structs