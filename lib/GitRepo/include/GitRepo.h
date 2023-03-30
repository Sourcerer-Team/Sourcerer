#pragma once

namespace GitRepo {

class GitRepo {
    static void initForCurrentThread();
    static void shutdownForCurrentThread();

private:
  GitRepo() = delete;
};

} // namespace GitRepo
