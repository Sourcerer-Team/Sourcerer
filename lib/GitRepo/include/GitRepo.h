#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

#include <git2/repository.h>
#include <git2/status.h>

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

struct GitStatus {
  QString path;
  QStringList newFiles;
  QStringList modifiedFiles;
  QStringList deletedFiles;
  QStringList typeChangedFiles;
  QStringList renamedFiles;
  QStringList unreadableFiles;
  QStringList newStagedFiles;
  QStringList modifiedStagedFiles;
  QStringList deletedStagedFiles;
  QStringList typeChangedStagedFiles;
  QStringList renamedStagedFiles;
  QStringList ignoredFiles;
  QStringList conflictedFiles;

  bool hasChanges() const {
    return !newFiles.isEmpty() || !modifiedFiles.isEmpty() ||
           !deletedFiles.isEmpty() || !typeChangedFiles.isEmpty() ||
           !renamedFiles.isEmpty() || !unreadableFiles.isEmpty() ||
           !newStagedFiles.isEmpty() || !modifiedStagedFiles.isEmpty() ||
           !deletedStagedFiles.isEmpty() || !typeChangedStagedFiles.isEmpty() ||
           !renamedStagedFiles.isEmpty() || !ignoredFiles.isEmpty() ||
           !conflictedFiles.isEmpty();
  }
};

struct GitHistoryWithStatus {
  ;
  GitHistory history;
  GitStatus status;
};

enum class BranchType {
  Unknown = 0,

  All,
  Local,
  Remote,
  Tag,

  last
};

int getLastError();
void initForCurrentThread();
void shutdownForCurrentThread();
bool checkoutBranch(git_repository *repo, const QString &branchOrTagName,
                    BranchType type);
bool cloneRepository(const QString &url, const QString &destinationDir,
                     const QString &uuid);
void closeRepository(git_repository *repo);
QString getActiveBranch(git_repository *repo, BranchType type,
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
GitStatus getStatus(git_repository *repo);

} // namespace GitRepo
