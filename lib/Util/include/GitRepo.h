#pragma once

#include <QString>

namespace Util {

class GitRepo {
public:

  static QString parseRepoName(const QString& url);

private:
  GitRepo() = delete;
};

} // namespace Util
