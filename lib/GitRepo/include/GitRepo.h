#pragma once

#include <QString>
#include <QStringList>

#include <git2/errors.h>
#include <git2/global.h>
#include <git2/repository.h>

namespace GitRepo {

void initForCurrentThread();
void shutdownForCurrentThread();
void closeRepository(git_repository *repo);
QString getActiveBranch(git_repository *repo);
git_repository *initRepository(const QString &repoName,
                               const QString &workingDir,
                               const QString &defaultBranch);
QStringList listLocalBranches(git_repository *repo);
QStringList listRemoteBranches(git_repository *repo);
QStringList listSubmodules(git_repository *repo);
QStringList listTags(git_repository *repo);
git_repository *openRepository(const QString &path);

} // namespace GitRepo
