// MIT License
// Copyright (c) 2026 Sourcerer

// Worktrees, built on the share extension, with a registry kept by Sourcerer
// in the main repository.

#include "HgRepo.h"
#include "HgRepoPrivate.h"

#include <Log.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>

#define THREAD_ID "HgRepo"

namespace HgRepo {

using namespace Private;

namespace {

struct ShareEntry {
  QString name;
  QString path;
  bool locked = false;
  QString reason;
};

// Root of the repository whose store repo uses: repo itself, or the source
// of a share (.hg/sharedpath names the source's .hg directory).
QString mainRoot(const Repository *repo) {
  QFile sharedPath(hgFile(repo, QStringLiteral("sharedpath")));
  if (!sharedPath.open(QIODevice::ReadOnly)) {
    return repo->root;
  }
  const QString hgDir =
      normalizedPath(QString::fromUtf8(sharedPath.readAll()).trimmed());
  return normalizedPath(QFileInfo(hgDir).absolutePath());
}

QString registryFile(const QString &root) {
  return root + QStringLiteral("/.hg/sourcerer/shares.json");
}

QVector<ShareEntry> readRegistry(const QString &root) {
  QFile file(registryFile(root));
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }

  QVector<ShareEntry> entries;
  for (const QJsonValue &value :
       QJsonDocument::fromJson(file.readAll()).array()) {
    const QJsonObject object = value.toObject();
    ShareEntry entry;
    entry.name = object.value("name").toString();
    entry.path = object.value("path").toString();
    entry.locked = object.value("locked").toBool();
    entry.reason = object.value("reason").toString();
    if (!entry.name.isEmpty()) {
      entries.append(entry);
    }
  }
  return entries;
}

bool writeRegistry(const QString &root, const QVector<ShareEntry> &entries) {
  QJsonArray array;
  for (const ShareEntry &entry : entries) {
    array.append(QJsonObject{{"name", entry.name},
                             {"path", entry.path},
                             {"locked", entry.locked},
                             {"reason", entry.reason}});
  }

  const QString fileName = registryFile(root);
  QDir().mkpath(QFileInfo(fileName).absolutePath());
  QSaveFile file(fileName);
  if (!file.open(QIODevice::WriteOnly) ||
      file.write(QJsonDocument(array).toJson()) < 0 || !file.commit()) {
    setLastError(HgError::WorktreeFailed, -1,
                 QStringLiteral("Failed to write %1: %2")
                     .arg(fileName, file.errorString()));
    return false;
  }
  return true;
}

int findEntry(const QVector<ShareEntry> &entries, const QString &name) {
  for (int i = 0; i < entries.size(); ++i) {
    if (entries[i].name == name) {
      return i;
    }
  }
  return -1;
}

// The share's .hg/sharedpath points back at the main repository's store.
bool isValidShare(const QString &path, const QString &root) {
  QFile sharedPath(path + QStringLiteral("/.hg/sharedpath"));
  if (!sharedPath.open(QIODevice::ReadOnly)) {
    return false;
  }
  return normalizedPath(QString::fromUtf8(sharedPath.readAll()).trimmed()) ==
         normalizedPath(root + QStringLiteral("/.hg"));
}

// Branch ("name" or "@bookmark") and short node of a working directory.
void readCheckout(const QString &path, QString *branch, QString *head) {
  HgCommandResult result;
  if (!runOneShot(
          HgError::WorktreeFailed,
          {QStringLiteral("log"), QStringLiteral("--rev"), QStringLiteral("."),
           QStringLiteral("--template"),
           QStringLiteral("{node|short}\\n{branch}\\n{activebookmark}")},
          path, &result)) {
    return;
  }
  const QStringList lines = QString::fromUtf8(result.out).split('\n');
  const QString node = lines.value(0).trimmed();
  *head = node == QStringLiteral("000000000000") ? QString() : node.left(7);
  const QString bookmark = lines.value(2).trimmed();
  *branch = bookmark.isEmpty() ? lines.value(1).trimmed() : '@' + bookmark;
}

bool runIn(const QString &path, HgError kind, const QStringList &args) {
  HgCommandResult result;
  return runOneShot(kind, args, path, &result);
}

} // namespace

QVector<GitRepo::WorktreeInfo> listWorktrees(Repository *repo) {
  if (!repo) {
    return {};
  }

  const QString root = mainRoot(repo);
  QVector<GitRepo::WorktreeInfo> worktrees;

  GitRepo::WorktreeInfo main;
  main.name = QFileInfo(root).fileName();
  main.path = root;
  main.isMain = true;
  main.isCurrent = root == repo->root;
  if (main.isCurrent) {
    const QString bookmark = activeBookmark(repo);
    main.branch = bookmark.isEmpty()
                      ? getActiveBranch(repo, GitRepo::BranchType::Local, false)
                      : '@' + bookmark;
    main.head = workingDirectoryNode(repo).left(7);
  } else {
    readCheckout(root, &main.branch, &main.head);
  }
  worktrees.append(main);

  QVector<ShareEntry> entries = readRegistry(root);
  std::sort(entries.begin(), entries.end(),
            [](const ShareEntry &a, const ShareEntry &b) {
              return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
            });

  for (const ShareEntry &entry : entries) {
    GitRepo::WorktreeInfo worktree;
    worktree.name = entry.name;
    worktree.path = normalizedPath(entry.path);
    worktree.locked = entry.locked;
    worktree.valid = isValidShare(worktree.path, root);
    worktree.isCurrent = worktree.path == repo->root;
    if (worktree.valid) {
      readCheckout(worktree.path, &worktree.branch, &worktree.head);
    }
    worktrees.append(worktree);
  }
  return worktrees;
}

bool addWorktree(Repository *repo, const QString &name, const QString &path,
                 const QString &branch, bool createBranch,
                 const QString &baseBranch, HgBranchKind kind) {
  if (!repo || name.isEmpty() || path.isEmpty() || branch.isEmpty()) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("A name, a path and a branch are required."));
    return false;
  }

  const QString root = mainRoot(repo);
  QVector<ShareEntry> entries = readRegistry(root);
  if (findEntry(entries, name) >= 0) {
    setLastError(HgError::WorktreeFailed, -1,
                 QStringLiteral("Worktree %1 already exists.").arg(name));
    return false;
  }

  const QString sharePath = normalizedPath(path);
  if (QFileInfo::exists(sharePath) &&
      !QDir(sharePath).isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot |
                               QDir::Hidden)) {
    setLastError(HgError::WorktreeFailed, -1,
                 QStringLiteral("%1 is not empty.").arg(sharePath));
    return false;
  }

  const QString bareName = branch.startsWith('@') ? branch.mid(1) : branch;
  const bool bookmark =
      createBranch ? kind == HgBranchKind::Bookmark : branch.startsWith('@');

  // A new bookmark is shared, so it can be created in the main repository.
  if (createBranch && bookmark &&
      !createBranchFromCommit(
          repo,
          baseBranch.isEmpty()
              ? QStringLiteral(".")
              : branchRevset(baseBranch, GitRepo::BranchType::Local),
          bareName)) {
    return false;
  }

  QDir().mkpath(QFileInfo(sharePath).absolutePath());
  if (!runIn(QFileInfo(sharePath).absolutePath(), HgError::WorktreeFailed,
             {QStringLiteral("share"), QStringLiteral("--bookmarks"),
              QStringLiteral("--noupdate"), QDir::toNativeSeparators(root),
              QDir::toNativeSeparators(sharePath)})) {
    return false;
  }

  bool updated = false;
  if (createBranch && !bookmark) {
    // A named branch exists with its first commit, so it is set in the share.
    const QString base =
        baseBranch.isEmpty()
            ? workingDirectoryNode(repo)
            : branchRevset(baseBranch, GitRepo::BranchType::Local);
    updated = (base.isEmpty() || runIn(sharePath, HgError::WorktreeFailed,
                                       {QStringLiteral("update"),
                                        QStringLiteral("--rev"), base})) &&
              runIn(sharePath, HgError::WorktreeFailed,
                    {QStringLiteral("branch"), bareName});
  } else {
    updated =
        runIn(sharePath, HgError::WorktreeFailed,
              {QStringLiteral("update"), QStringLiteral("--rev"),
               bookmark ? bareName
                        : branchRevset(branch, GitRepo::BranchType::Local)});
  }

  if (!updated) {
    const HgErrorContext error = getLastError();
    QDir(sharePath).removeRecursively();
    setLastError(error.error, error.errorCode, error.errorMessage);
    return false;
  }

  entries.append(ShareEntry{name, sharePath, false, QString()});
  return writeRegistry(root, entries);
}

bool removeWorktree(Repository *repo, const QString &name, bool force) {
  if (!repo) {
    return false;
  }

  const QString root = mainRoot(repo);
  QVector<ShareEntry> entries = readRegistry(root);
  const int index = findEntry(entries, name);
  if (index < 0) {
    setLastError(HgError::WorktreeFailed, -1,
                 name == QFileInfo(root).fileName()
                     ? QStringLiteral("The main working directory can't be "
                                      "removed.")
                     : QStringLiteral("Worktree %1 does not exist.").arg(name));
    return false;
  }

  const ShareEntry entry = entries[index];
  if (entry.locked) {
    setLastError(HgError::WorktreeFailed, -1,
                 QStringLiteral("Worktree %1 is locked.").arg(name));
    return false;
  }
  if (normalizedPath(entry.path) == repo->root) {
    setLastError(HgError::WorktreeFailed, -1,
                 QStringLiteral("The open worktree can't be removed."));
    return false;
  }

  if (!force && isValidShare(entry.path, root)) {
    HgCommandResult result;
    if (!runOneShot(HgError::WorktreeFailed,
                    {QStringLiteral("status"), QStringLiteral("-mardu")},
                    entry.path, &result)) {
      return false;
    }
    if (!result.out.trimmed().isEmpty()) {
      setLastError(HgError::UncommittedChanges, -1,
                   QStringLiteral("Worktree %1 has changes.").arg(name));
      return false;
    }
  }

  if (QFileInfo(entry.path).isDir() && !QDir(entry.path).removeRecursively()) {
    setLastError(HgError::WorktreeFailed, -1,
                 QStringLiteral("Failed to delete %1.").arg(entry.path));
    return false;
  }

  entries.removeAt(index);
  return writeRegistry(root, entries);
}

bool lockWorktree(Repository *repo, const QString &name,
                  const QString &reason) {
  if (!repo) {
    return false;
  }
  const QString root = mainRoot(repo);
  QVector<ShareEntry> entries = readRegistry(root);
  const int index = findEntry(entries, name);
  if (index < 0) {
    setLastError(HgError::WorktreeFailed, -1,
                 QStringLiteral("Worktree %1 does not exist.").arg(name));
    return false;
  }
  entries[index].locked = true;
  entries[index].reason = reason;
  return writeRegistry(root, entries);
}

bool unlockWorktree(Repository *repo, const QString &name) {
  if (!repo) {
    return false;
  }
  const QString root = mainRoot(repo);
  QVector<ShareEntry> entries = readRegistry(root);
  const int index = findEntry(entries, name);
  if (index < 0) {
    setLastError(HgError::WorktreeFailed, -1,
                 QStringLiteral("Worktree %1 does not exist.").arg(name));
    return false;
  }
  entries[index].locked = false;
  entries[index].reason.clear();
  return writeRegistry(root, entries);
}

} // namespace HgRepo
