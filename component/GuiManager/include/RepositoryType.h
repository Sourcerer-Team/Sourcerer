#pragma once

#include <QDir>
#include <QString>

namespace GuiManager {

enum class RepositoryType {
  Git = 0,
  Subversion,
  Mercurial,
};

// Detects the repository type from its administration directory, Git is the
// default when none is found.
inline RepositoryType repositoryTypeOf(const QString &path) {
  const QDir repoDir(path);
  if (repoDir.exists(QStringLiteral(".git"))) {
    return RepositoryType::Git;
  }
  if (repoDir.exists(QStringLiteral(".hg"))) {
    return RepositoryType::Mercurial;
  }
  if (repoDir.exists(QStringLiteral(".svn"))) {
    return RepositoryType::Subversion;
  }
  return RepositoryType::Git;
}

// Key used to store the repository type together with a path.
inline QString repositoryTypeKey(const RepositoryType type) {
  switch (type) {
  case RepositoryType::Subversion:
    return QStringLiteral("svn");
  case RepositoryType::Mercurial:
    return QStringLiteral("hg");
  case RepositoryType::Git:
    break;
  }
  return QStringLiteral("git");
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
  if (key == QStringLiteral("hg")) {
    *type = RepositoryType::Mercurial;
    return true;
  }
  return false;
}

} // namespace GuiManager
