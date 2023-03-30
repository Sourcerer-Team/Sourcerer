
#include "GitRepo.h"

#include <Log.h>

#include <git2/global.h>

#define THREAD_ID "GitRepo"

namespace GitRepo {

void initForCurrentThread() {
  LOG_PERF(0, "Enter");

  const int numberOfCalls = git_libgit2_init();

  LOG_DEBUG("Number of initializations is %1", numberOfCalls);

  LOG_PERF(0, "Exit");
}

void shutdownForCurrentThread() {
  LOG_PERF(0, "Enter");

  const int numberofRemaining = git_libgit2_shutdown();

  LOG_DEBUG("Number of remaining initializations is %1", numberofRemaining);

  LOG_PERF(0, "Exit");
}

} // namespace GitRepo
