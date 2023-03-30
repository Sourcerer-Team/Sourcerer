
#include "GitRepo.h"

#include <Log.h>

#define THREAD_ID "GitRepo"

namespace GitRepo {

void initForCurrentThread() {
  LOG_PERF(0, "Enter");

  const int numberOfCalls = git_libgit2_init();

  LOG_DEBUG("Number of initializations is {}", numberOfCalls);

  LOG_PERF(0, "Exit");
}

void shutdownForCurrentThread() {
  LOG_PERF(0, "Enter");

  const int numberofRemaining = git_libgit2_shutdown();

  LOG_DEBUG("Number of remaining initializations is {}", numberofRemaining);

  LOG_PERF(0, "Exit");
}

git_repository *openRepository(const QString &path) {
  LOG_PERF(0, "Enter");

  git_repository *repo = nullptr;
  const int error = git_repository_open(&repo, path.toStdString().c_str());

  if (error) {
    const git_error *gitError = git_error_last();
    LOG_ERROR(Log::ErrorId::Git,
              "Error is {}, error klass is {}, error message is '{}'", error,
              gitError->klass, gitError->message);
  }

  LOG_PERF(0, "Exit");
  return repo;
}

void closeRepository(git_repository *repo) {
  LOG_PERF(0, "Enter");

  if (!repo) {
    LOG_PERF(0, "Exit");
    return;
  }

  LOG_PERF(0, "Exit");
}

} // namespace GitRepo
