#pragma once

#include <BlockingQueue.h>

#include <QString>

#include <git2/repository.h>

#include <thread>

namespace GitRepoManager {

namespace Structs {

enum class GitRepoCommandType : int { Unknown = 0, Open, Quit, Last };

enum class GitRepoErrorType : int {
  Unknown = 0,
  AlreadyExist,
  ErrorWhileOpening,
  InternalError,
  Last
};

struct GitRepoCommand {
  GitRepoCommand(GitRepoCommandType commandType) : type(commandType) {}
  virtual ~GitRepoCommand() {}
  GitRepoCommandType type;
};

struct GitRepoQuitCommand : GitRepoCommand {
  GitRepoQuitCommand() : GitRepoCommand(GitRepoCommandType::Quit) {}
  virtual ~GitRepoQuitCommand() {}
};

struct GitRepoOpenCommand : GitRepoCommand {
  GitRepoOpenCommand() : GitRepoCommand(GitRepoCommandType::Open) {}
  virtual ~GitRepoOpenCommand() {}
  QString uuid;
  QString path;
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
