#pragma once

namespace GitRepoManager {

class GitRepoManager {
public:
  GitRepoManager();
  virtual ~GitRepoManager();

  void start();
  void stop();
};

}
