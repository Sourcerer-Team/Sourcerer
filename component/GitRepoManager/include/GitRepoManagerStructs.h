#pragma once

#include <BlockingQueue.h>

#include <QString>

#include <git2/repository.h>

#include <thread>

namespace GitRepoManager {

namespace Structs {

enum class GitRepoCommandType : int {
  Unknown = 0,
  Close,
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
  NotOpened,
  Last
};

struct GitRepoCommand {
  GitRepoCommand(GitRepoCommandType commandType) : type(commandType) {}
  virtual ~GitRepoCommand() {}
  GitRepoCommandType type;
  QString uuid;
};

struct GitRepoCloseCommand : GitRepoCommand {
  GitRepoCloseCommand() : GitRepoCommand(GitRepoCommandType::Close) {}
  virtual ~GitRepoCloseCommand() {}
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
