#pragma once

#include <BlockingQueue.h>

#include <QString>

#include <git2/repository.h>

#include <thread>

namespace GitRepoManager {

namespace Structs {

enum class GitRepoCommandType : int {
  Unknown = 0,
  Clone,
  Close,
  GetActiveBranch,
  GetCommitBranches,
  GetCommitHeads,
  GetCommitIsHead,
  GetHistory,
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
  GitRepoCommand(GitRepoCommandType commandType) : type(commandType) {}
  virtual ~GitRepoCommand() {}
  GitRepoCommandType type;
  QString uuid;
};

struct GitRepoCloneCommand : GitRepoCommand {
  GitRepoCloneCommand() : GitRepoCommand(GitRepoCommandType::Clone) {}
  virtual ~GitRepoCloneCommand() {}

  QString url;
  QString destinationDir;
};

struct GitRepoCloseCommand : GitRepoCommand {
  GitRepoCloseCommand() : GitRepoCommand(GitRepoCommandType::Close) {}
  virtual ~GitRepoCloseCommand() {}
};

struct GitRepoGetActiveBranchCommand : GitRepoCommand {
  GitRepoGetActiveBranchCommand()
      : GitRepoCommand(GitRepoCommandType::GetActiveBranch) {}
  virtual ~GitRepoGetActiveBranchCommand() {}
};

struct GitRepoGetCommitBranchesCommand : GitRepoCommand {
  GitRepoGetCommitBranchesCommand()
      : GitRepoCommand(GitRepoCommandType::GetCommitBranches) {}
  virtual ~GitRepoGetCommitBranchesCommand() {}

  QString commitOid;
};

struct GitRepoGetCommitHeadsCommand : GitRepoCommand {
  GitRepoGetCommitHeadsCommand()
      : GitRepoCommand(GitRepoCommandType::GetCommitHeads) {}
  virtual ~GitRepoGetCommitHeadsCommand() {}

  QString commitOid;
};

struct GitRepoGetCommitIsHeadCommand : GitRepoCommand {
  GitRepoGetCommitIsHeadCommand()
      : GitRepoCommand(GitRepoCommandType::GetCommitIsHead) {}
  virtual ~GitRepoGetCommitIsHeadCommand() {}

  QString commitOid;
};

struct GitRepoGetHistoryCommand : GitRepoCommand {
  GitRepoGetHistoryCommand() : GitRepoCommand(GitRepoCommandType::GetHistory) {}
  virtual ~GitRepoGetHistoryCommand() {}
};

struct GitRepoInitRepoCommand : GitRepoCommand {
  GitRepoInitRepoCommand() : GitRepoCommand(GitRepoCommandType::Init) {}
  virtual ~GitRepoInitRepoCommand() {}

  QString repoName;
  QString workingDir;
  QString defaultBranch;
};

struct GitRepoListLocalBranchesCommand : GitRepoCommand {
  GitRepoListLocalBranchesCommand()
      : GitRepoCommand(GitRepoCommandType::ListLocalBranches) {}
  virtual ~GitRepoListLocalBranchesCommand() {}
};

struct GitRepoListRemoteBranchesCommand : GitRepoCommand {
  GitRepoListRemoteBranchesCommand()
      : GitRepoCommand(GitRepoCommandType::ListRemoteBranches) {}
  virtual ~GitRepoListRemoteBranchesCommand() {}
};

struct GitRepoListSubmodulesCommand : GitRepoCommand {
  GitRepoListSubmodulesCommand()
      : GitRepoCommand(GitRepoCommandType::ListSubmodules) {}
  virtual ~GitRepoListSubmodulesCommand() {}
};

struct GitRepoListTagsCommand : GitRepoCommand {
  GitRepoListTagsCommand() : GitRepoCommand(GitRepoCommandType::ListTags) {}
  virtual ~GitRepoListTagsCommand() {}
};

struct GitRepoOpenCommand : GitRepoCommand {
  GitRepoOpenCommand() : GitRepoCommand(GitRepoCommandType::Open) {}
  virtual ~GitRepoOpenCommand() {}
  QString path;
};

struct GitRepoQuitCommand : GitRepoCommand {
  GitRepoQuitCommand() : GitRepoCommand(GitRepoCommandType::Quit) {}
  virtual ~GitRepoQuitCommand() {}
};

struct GitRepoError {
  QString uuid;
  QString message;
  GitRepoErrorType error = GitRepoErrorType::Unknown;
};

struct GitRepoContext {
  QString uuid;
  BlockingQueue<GitRepoCommand *> commandsQueue;
  std::thread workersThread;
  git_repository *gitRepo = nullptr;
};

} // namespace Structs
} // namespace GitRepoManager
