#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

#include <git2/repository.h>
#include <git2/status.h>

namespace GitRepo {

struct GitError {
  int errorCode = -1;
  QString errorMessage;
};

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
  bool error = false;
  CloneProgressStageType stage = CloneProgressStageType::Unknown;
};

struct GitCredentialsRequest {
  size_t id = -1;
  QString url;
  QString username;
  int allowedTypes;
};

struct GitCredentialsResponse {
  size_t id = -1;
  QString username;
  QString password;
};

struct GitCredentialsErrorResponse {
  size_t id = -1;
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
           !renamedStagedFiles.isEmpty() /*|| !ignoredFiles.isEmpty()*/ ||
           !conflictedFiles.isEmpty();
  }
};

struct GitCommitStatus {
  QString commit;
  QStringList newFiles;
  QStringList modifiedFiles;
  QStringList deletedFiles;
  QStringList typeChangedFiles;
  QStringList renamedFiles;
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

enum class ResetType {
  Unknown = 0,

  Soft,
  Mixed,
  Hard,

  last
};

struct FilesDiffLine {
  QChar origin;
  QString content;
};

struct FilesDiffHunk {
  int oldStart = -1;
  int newStart = -1;
  int oldLines = -1;
  int newLines = -1;
  QVector<FilesDiffLine> lines;
};

struct FilesDiffDelta {
  QString oldFilePath;
  QString newFilePath;
  QVector<FilesDiffHunk> hunks;
};

struct FilesDiff {
  QString diff;
  QMap<QString, QString> filesContent;
  QMap<QString, QString> repoContent;
  QVector<FilesDiffDelta> deltas;
  bool valid = false;
};

GitError getLastError();

void initForCurrentThread();

void shutdownForCurrentThread();

bool applyStash(git_repository *repo, int index);

bool checkoutBranch(git_repository *repo, const QString &branchOrTagName,
                    BranchType type);

bool checkoutCommit(git_repository *repo, const QString &commit);

bool cloneRepository(const QString &url, const QString &destinationDir,
                     const QString &uuid);

void closeRepository(git_repository *repo);

bool createBranchFromCommit(git_repository *repo, const QString &commit,
                            const QString &branchName);

bool deleteStash(git_repository *repo, int stashIndex);

QString getActiveBranch(git_repository *repo, BranchType type,
                        const bool &quitFlag);

bool initRepository(const QString &repoName, const QString &workingDir,
                    const QString &defaultBranch);

QStringList listLocalBranches(git_repository *repo, const bool &quitFlag);

QStringList listRemoteBranches(git_repository *repo, const bool &quitFlag);

QStringList listStashes(git_repository *repo, const bool &quitFlag);

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

GitCommitStatus getCommitStatus(git_repository *repo, const QString &commitOid);

bool stageFiles(git_repository *repo, const QStringList &files);

bool unstageFiles(git_repository *repo, const QStringList &files);

bool commitStaged(git_repository *repo, const QString &summary,
                  const QString &message, const QString &author,
                  const QString &email);

FilesDiff diffFiles(git_repository *repo, const QStringList &files);

FilesDiff diffFilesFromCommit(git_repository *repo, const QStringList &files,
                            const QString &commit);

bool discardFiles(git_repository *repo, const QStringList &files);

bool pull(git_repository *rep);

bool push(git_repository *rep);

bool stash(git_repository *repo);

bool setUpstream(git_repository *repo, const QString &remoteName,
                 const QString &remoteBranchName,
                 const QString &localBranchName);

QPair<QString, QString> getUpstream(git_repository *repo,
                                    const QString &localBranchName);

bool resetToCommit(git_repository *repo, const QString &commit, ResetType type);

bool revertCommit(git_repository *repo, const QString &commit,
                  const QString &author, const QString &email,
                  const QString &message);

bool applyPatch(git_repository *repo, const QString &patchContent);

bool renameBranch(git_repository *repo, const QString &oldName,
                  const QString &newName);

bool deleteBranch(git_repository *repo, const QString &branchName);

bool createTagFromCommit(git_repository *repo, const QString &commit,
                         const QString &tagName, const QString &author,
                         const QString &email, const QString &message);

QString createPatchFromCommit(git_repository *repo, const QString &commit);

bool renameTag(git_repository *repo, const QString &oldName,
               const QString &newName);

bool deleteTag(git_repository *repo, const QString &tagName);

QString getSubmodulePath(git_repository *repo, const QString &submoduleName);

bool deleteSubmodule(git_repository *repo, const QString &submoduleName,
                     const QString &submodulePath);

bool updateSubmodule(git_repository *repo, const QString &submoduleName);

bool addSubmodule(git_repository *repo, const QString &url, const QString &name,
                  const QString &uuid);

} // namespace GitRepo
