#pragma once

#include <QDir>
#include <QString>

namespace GuiManager {

enum class RepositoryType {
  Git = 0,
  Subversion,
};

// Detects the repository type from its administration directory, Git is the
// default when neither is found.
inline RepositoryType repositoryTypeOf(const QString &path) {
  const QDir repoDir(path);
  if (repoDir.exists(QStringLiteral(".git"))) {
    return RepositoryType::Git;
  }
  if (repoDir.exists(QStringLiteral(".svn"))) {
    return RepositoryType::Subversion;
  }
  return RepositoryType::Git;
}

// Key used to store the repository type together with a path.
inline QString repositoryTypeKey(const RepositoryType type) {
  return type == RepositoryType::Subversion ? QStringLiteral("svn")
                                            : QStringLiteral("git");
}

inline bool repositoryTypeFromKey(const QString &key, RepositoryType *type) {
  if (key == QStringLiteral("git")) {
    *type = RepositoryType::Git;
    return true;
  }
  if (key == QStringLiteral("svn")) {
    *type = RepositoryType::Subversion;
    return true;
  }
  return false;
}

} // namespace GuiManager
