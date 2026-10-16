#pragma once

#include "GitRepo.h"

#include <git2/repository.h>

#include <QByteArray>
#include <QString>

namespace GitRepo::Lfs {

struct Pointer {
  QString oid;
  qint64 size = 0;
  bool valid = false;
  QString errorMessage;
};

bool initialize();
void shutdown();

Pointer parsePointer(const QByteArray &data);
QByteArray makePointer(const QString &oid, qint64 size);

QString repositoryRootPath(git_repository *repo);
QString repositoryRootPath(const QString &repoPath);
QString objectsRootPath(git_repository *repo);
QString objectsRootPath(const QString &repoPath);
QString objectPath(git_repository *repo, const QString &oid);

StorageStatus getStorageStatus(git_repository *repo);

bool objectExists(git_repository *repo, const QString &oid);
bool readObject(git_repository *repo, const QString &oid, QByteArray &data);
bool storeObject(git_repository *repo, const QByteArray &data, QString &oid);

bool downloadObject(git_repository *repo, const QString &oid, qint64 size,
                    QByteArray &data);
bool uploadLocalObjects(git_repository *repo, const QString &repoPath,
                        const QString &remoteName,
                        const QString &remoteUrlHint = QString(),
                        const QString &refName = QString());

} // namespace GitRepo::Lfs
