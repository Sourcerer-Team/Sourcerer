// MIT License
// Copyright (c) 2026 Sourcerer

// LFS with the lfs extension: tracked patterns in .hglfs, objects in the
// store.

#include "HgRc.h"
#include "HgRepo.h"
#include "HgRepoPrivate.h"

#include <Log.h>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

#define THREAD_ID "HgRepo"

namespace HgRepo {

using namespace Private;

namespace {

const QString kHgLfs = QStringLiteral(".hglfs");
const QString kTrack = QStringLiteral("track");

QString trackKey(const QString &path) {
  return QStringLiteral("path:") + path;
}

// The store of a share is the one of its main repository.
QString storePath(const Repository *repo) {
  QFile sharedPath(hgFile(repo, QStringLiteral("sharedpath")));
  if (sharedPath.open(QIODevice::ReadOnly)) {
    return normalizedPath(QString::fromUtf8(sharedPath.readAll()).trimmed()) +
           QStringLiteral("/store");
  }
  return hgFile(repo, QStringLiteral("store"));
}

bool loadLfs(const Repository *repo, HgRc *rc, bool *existed) {
  *existed = QFile::exists(repo->root + '/' + kHgLfs);
  if (!rc->load()) {
    setLastError(HgError::LfsFailed, -1, rc->errorMessage());
    return false;
  }
  return true;
}

bool saveAndStage(Repository *repo, HgRc *rc) {
  if (!rc->save()) {
    setLastError(HgError::LfsFailed, -1, rc->errorMessage());
    return false;
  }
  // Unknown .hglfs is added by staging.
  return stageFiles(repo, {kHgLfs});
}

} // namespace

bool isLfsEnabled(Repository *repo) {
  if (!repo) {
    return false;
  }

  if (hasRequirement(repo->root, "lfs")) {
    return true;
  }

  HgRc rc(hgFile(repo, QStringLiteral("hgrc")));
  if (!rc.load() ||
      !rc.contains(QStringLiteral("extensions"), QStringLiteral("lfs"))) {
    return false;
  }
  // "lfs = !" disables it.
  return !rc.value(QStringLiteral("extensions"), QStringLiteral("lfs"))
              .startsWith('!');
}

bool enableLfs(Repository *repo) {
  if (!repo) {
    return false;
  }
  if (isLfsEnabled(repo)) {
    return true;
  }

  HgRc rc(hgFile(repo, QStringLiteral("hgrc")));
  if (!rc.load()) {
    setLastError(HgError::LfsFailed, -1, rc.errorMessage());
    return false;
  }
  rc.setValue(QStringLiteral("extensions"), QStringLiteral("lfs"), QString());
  if (!rc.save()) {
    setLastError(HgError::LfsFailed, -1, rc.errorMessage());
    return false;
  }
  // The command server restarts with the extension at its next command, as
  // .hg/hgrc changed.
  return true;
}

bool addFilesToLfs(Repository *repo, const QStringList &files) {
  if (!repo) {
    return false;
  }
  if (!isLfsEnabled(repo)) {
    setLastError(HgError::LfsNotEnabled, -1,
                 QStringLiteral("The lfs extension is not enabled for this "
                                "repository."));
    return false;
  }

  HgRc rc(repo->root + '/' + kHgLfs);
  bool existed = false;
  if (!loadLfs(repo, &rc, &existed)) {
    return false;
  }

  for (const QString &path : relativeFilePaths(repo->root, files)) {
    rc.setValue(kTrack, trackKey(path), QStringLiteral("all()"));
  }
  return saveAndStage(repo, &rc);
}

bool removeFilesFromLfs(Repository *repo, const QStringList &files) {
  if (!repo) {
    return false;
  }

  HgRc rc(repo->root + '/' + kHgLfs);
  bool existed = false;
  if (!loadLfs(repo, &rc, &existed)) {
    return false;
  }
  if (!existed) {
    return true;
  }

  for (const QString &path : relativeFilePaths(repo->root, files)) {
    rc.remove(kTrack, trackKey(path));
  }
  return saveAndStage(repo, &rc);
}

QStringList listLfsFiles(Repository *repo) {
  QStringList paths;
  if (!repo) {
    return paths;
  }

  HgRc rc(repo->root + '/' + kHgLfs);
  if (!rc.load()) {
    return paths;
  }
  for (const auto &[key, value] : rc.entries(kTrack)) {
    Q_UNUSED(value);
    if (key.startsWith(QStringLiteral("path:"))) {
      paths << key.mid(5);
    }
  }
  return paths;
}

GitRepo::Lfs::StorageStatus getLfsStorageStatus(Repository *repo) {
  GitRepo::Lfs::StorageStatus status;
  if (!repo) {
    return status;
  }
  status.repositoryPath = repo->root;

  QDirIterator it(storePath(repo) + QStringLiteral("/lfs/objects"),
                  QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
  while (it.hasNext()) {
    it.next();
    ++status.objectCount;
    status.totalBytes += it.fileInfo().size();
  }
  return status;
}

} // namespace HgRepo
