#pragma once

#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <git2/errors.h>
#include <git2/global.h>
#include <git2/repository.h>

namespace GitRepo {

enum class CloneProgressStageType : int {
  Unknown = 0,
  Fetch,
  Checkout,

  last
};

struct CloneProgress {
  QString uuid;
  size_t completedSteps = 0;
  size_t totalSteps = 0;
  int lastStepPercentage = -1;
  bool started = false;
  bool finished = false;
  CloneProgressStageType stage = CloneProgressStageType::Unknown;
};

struct GitCredentialsRequest {
  QString uuid;
  QString url;
  QString username;
  int allowedTypes;
};

struct GitCredentialsResponse {
  QString uuid;
  QString username;
  QString password;
};

struct GitCredentialsErrorResponse {
  QString uuid;
};

struct GitCommitInfo {
  git_oid oid;
  QString message;
  QString summary;
  QString authorName;
  QString authorEmail;
  qint64 timeStamp = -1;
  QVector<QString> branches;
  QVector<QString> parents;
  QVector<QString> children;
};

struct GitHistory {
  bool valid = false;
  QVector<QString> commits;
  QMap<QString, GitCommitInfo> commitInfos;
};

enum class BranchType {
  Unknown = 0,

  All,
  Local,
  Remote,
  Tag,

  last
};

void initForCurrentThread();
void shutdownForCurrentThread();
bool cloneRepository(const QString &url, const QString &destinationDir,
                     const QString &uuid);
void closeRepository(git_repository *repo);
QString getActiveBranch(git_repository *repo, const BranchType type,
                        const bool &quitFlag);
bool initRepository(const QString &repoName, const QString &workingDir,
                    const QString &defaultBranch);
QStringList listLocalBranches(git_repository *repo, const bool &quitFlag);
QStringList listRemoteBranches(git_repository *repo, const bool &quitFlag);
QStringList listSubmodules(git_repository *repo);
QStringList listTags(git_repository *repo);
git_repository *openRepository(const QString &path);
GitHistory getBranchHistory(git_repository *repo, const QString &branchName,
                            BranchType type, const bool &quitFlag);
QStringList getCommitBranches(git_repository *repo, const QString &commitOid,
                              const bool &quitFlag);
QStringList getCommitHeads(git_repository *repo, const QString &commitOid,
                           const bool &quitFlag);
bool getCommitIsHead(git_repository *repo, const QString &commitOid,
                     const bool &quitFlag);

} // namespace GitRepo
