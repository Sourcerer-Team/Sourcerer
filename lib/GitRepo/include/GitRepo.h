#pragma once

#include <QString>
#include <QStringList>

#include <git2/errors.h>
#include <git2/global.h>
#include <git2/repository.h>

namespace GitRepo {

struct CloneProgress
{
    uint hash;
    size_t completedSteps = 0;
    size_t totalSteps = 0;
    size_t lastStepPercentage = 0;
    QString path;
};

struct GitCredentialsRequest
{
    QString uuid;
    QString url;
    QString username;
    int allowedTypes;
};

struct GitCredentialsResponse
{
    QString uuid;
    QString username;
    QString password;
};

struct GitCredentialsErrorResponse
{
    QString uuid;
};

void initForCurrentThread();
void shutdownForCurrentThread();
git_repository *cloneRepository(const QString &url,
                                const QString &destinationDir);
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
