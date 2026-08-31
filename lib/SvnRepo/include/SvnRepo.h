#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

#include <svn_client.h>

#include <tuple>

namespace SvnRepo {

struct git_repository;

enum class BranchType : int {
  Unknown = 0,

  All,
  Local,
  Remote,
  Tag,

  last
};

enum class ResetType : int {
  Unknown = 0,

  Soft,
  Mixed,
  Hard,

  last
};

enum class SvnError : int {
  Unknown = 0,
  InitializationFailed,
  CreateClientContextFailed,
  CheckoutFailed,
  UpdateFailed,
  StatusFailed,
  AddFailed,
  DeleteFailed,
  RevertFailed,
  CommitFailed,
  HistoryFailed,
  ErrorWhileOpening,
  Last
};

using GitError = SvnError;

enum class BranchStatusType : int {
  Unknown = 0,
  UpToDate,
  Ahead,
  Behind,
  Diverged,
  NoUpstream,
  last
};

struct BranchStatus {
  BranchStatusType status = BranchStatusType::Unknown;
  size_t aheadCount = 0;
  size_t behindCount = 0;
};

struct BranchInfo {
  QString name;
  BranchStatus statusInfo;
};

enum class SubmoduleStatusType : int {
  Unknown = 0,
  UpToDate,
  Outdated,
  Error,
  last
};

struct SubmoduleStatus {
  SubmoduleStatusType status = SubmoduleStatusType::Unknown;
};

struct SubmoduleInfo {
  QString name;
  QString commit;
  SubmoduleStatus statusInfo;
};

struct RemoteTarget {
  QString name;
  QString fetchUrl;
  QString pushUrl;
};

struct RemoteInfo {
  QString name;
  QString fetchUrl;
  QString pushUrl;
  bool isDefault = false;
  QVector<RemoteTarget> targets;
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
  bool valid = false;
  QString diff;
  QMap<QString, QString> filesContent;
  QMap<QString, QString> repoContent;
  QVector<FilesDiffDelta> deltas;
};

struct CommitStatus {
  QString commit;
  QString author;
  QString message;
  qint64 timeStamp = -1;
};

struct SvnErrorContext {
  SvnError error = SvnError::Unknown;
  int errorCode = -1;
  QString errorMessage;
};

struct SvnStatus {
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
  QStringList untrackedFiles;

  bool hasChanges() const;

  bool operator==(const SvnStatus &other) const;
};

struct SvnCommitInfo {
  qint64 revision = -1;
  QString message;
  QString summary;
  QString authorName;
  QString authorEmail;
  qint64 timeStamp = -1;
  bool signedCommit = false;
  QString signatureType;
  QString signature;
  QVector<QString> branches;
  QVector<QString> parents;
  QVector<QString> children;

  bool operator==(const SvnCommitInfo &other) const {
    return revision == other.revision && message == other.message &&
           summary == other.summary && authorName == other.authorName &&
           authorEmail == other.authorEmail && timeStamp == other.timeStamp &&
           signedCommit == other.signedCommit &&
           signatureType == other.signatureType &&
           signature == other.signature && branches == other.branches &&
           parents == other.parents && children == other.children;
  }
};

struct SvnHistory {
  bool valid = false;
  QVector<QString> commits;
  QMap<QString, SvnCommitInfo> commitInfos;
};

SvnErrorContext getLastError();

void initForCurrentThread();
void shutdownForCurrentThread();

bool init();

bool abortMerge(const QString &path);
bool addSubmodule(const QString &path, const QString &url, const QString &name,
                  const QString &branch, const QString &uuid);
bool amend(const QString &path, const QString &summary, const QString &message,
           const QString &author, const QString &email, bool signoff,
           bool signCommit);
bool applyPatch(const QString &path, const QString &patchContent);
bool applyStash(const QString &path, int index);
QString listConflictsForStashFile(const QString &path, int index,
                                  const QString &filePath);
bool checkoutBranch(const QString &path, const QString &branchOrTagName,
                    BranchType type);
bool checkoutCommit(const QString &path, const QString &commit);

QString openRepository(const QString &path);
void closeRepository(const QString &repo);

bool cloneRepository(const QString &url, const QString &destinationDir,
                     const QString &uuid = QString());
bool commitStaged(const QString &path, const QString &author,
                  const QString &email, const QString &summary,
                  const QString &message, bool cleanup, bool signoff,
                  bool signCommit);
bool createBranchFromCommit(const QString &path, const QString &commit,
                            const QString &branchName);
QString createPatchFromCommit(const QString &path, const QString &commit);
bool createTagFromCommit(const QString &path, const QString &commit,
                         const QString &tagName, const QString &author,
                         const QString &email, const QString &message);
bool addTarget(const QString &path, const QString &remoteName,
               const QString &targetName, const QString &fetchUrl,
               const QString &pushUrl);
bool editTarget(const QString &path, const QString &remoteName,
                const QString &targetName, const QString &newFetchUrl,
                const QString &newPushUrl);
bool deleteTarget(const QString &path, const QString &remoteName,
                  const QString &targetName);
bool setDefaultTarget(const QString &path, const QString &remoteName,
                      const QString &targetName);
bool deleteBranch(const QString &path, const QString &branchName);
bool deleteRemoteBranch(const QString &path, const QString &branchName);
bool deleteStash(const QString &path, int index);
bool deleteAllStashes(const QString &path);
bool deleteSubmodule(const QString &path, const QString &submoduleName,
                     const QString &submodulePath);
bool deleteTag(const QString &path, const QString &tagName);
bool deleteRemoteTag(const QString &path, const QString &tagName);

FilesDiff diffFiles(const QString &path, const QStringList &files);
FilesDiff diffFilesFromCommit(const QString &path, const QStringList &files,
                              const QString &commit);
bool discardFiles(const QString &path, const QStringList &files);

bool fetchAll(const QString &path);
bool fetchRemote(const QString &path, const QString &remoteName);
bool rebaseBranches(const QString &path, const QString &sourceBranch,
                    const QString &destinationBranch, const QString &author,
                    const QString &email);

qint64 getFileSizeFromCommit(const QString &path, const QString &filePath,
                             const QString &commit);
QString getActiveBranch(const QString &path, BranchType type, bool quitFlag);
QStringList getCommitBranches(const QString &path, const QString &commitOid,
                              bool quitFlag);
QStringList getCommitTags(const QString &path, const QString &commitOid,
                          bool quitFlag);
QStringList getCommitHeads(const QString &path, const QString &commitOid,
                           bool quitFlag);
bool getCommitIsHead(const QString &path, const QString &commitOid,
                     bool quitFlag);
CommitStatus getCommitStatus(const QString &path, const QString &commitOid);
CommitStatus getStashStatus(const QString &path, int stashIndex);
bool getIsMergeInProgress(const QString &path);
SvnHistory getBranchHistory(const QString &path, const QString &branchName,
                            BranchType branchType, bool quitFlag);
QString getSubmodulePath(const QString &path, const QString &submoduleName);
std::tuple<bool, QString, QString> getUpstream(const QString &path,
                                               const QString &localBranchName);
bool initRepository(const QString &repoName, const QString &workingDir,
                    const QString &defaultBranch);
QString listConflictsForAFile(const QString &path, const QString &filePath);
QStringList listLocalBranches(const QString &path, const bool &quitFlag);
QStringList listRemoteBranches(const QString &path, const bool &quitFlag);
QVector<BranchInfo> listRemoteBranchesWithStatus(const QString &path,
                                                 const bool &quitFlag);
QStringList listStashes(const QString &path, const bool &quitFlag);
QStringList listSubmodules(const QString &path);
QVector<SubmoduleInfo> listSubmodulesWithStatus(const QString &path,
                                                const bool &quitFlag);
QStringList listTags(const QString &path);
bool merge(const QString &path, const QString &sourceBranch,
           const QString &destinationBranch, const QString &author,
           const QString &email);
bool pull(const QString &path, const QString &activeBranchName, bool rebase);
bool push(const QString &path, const QString &activeBranchName, bool force);
bool pushTag(const QString &path, const QString &tagName);
bool pushAllTags(const QString &path);
bool removeRemote(const QString &path, const QString &remoteName);
bool renameBranch(const QString &path, const QString &oldBranchName,
                  const QString &newBranchName);
bool renameTag(const QString &path, const QString &oldTagName,
               const QString &newTagName);
bool resetToCommit(const QString &path, const QString &commit, ResetType type);
bool revertCommit(const QString &path, const QString &commit,
                  const QString &author, const QString &email,
                  const QString &message);
bool cherryPickCommit(const QString &path, const QString &commit,
                      const QString &author, const QString &email);
bool setUpstream(const QString &path, const QString &remoteName,
                 const QString &remoteBranchName,
                 const QString &localBranchName);
bool stageFiles(const QString &path, const QStringList &files);
bool stash(const QString &path);
SvnStatus getStatus(const QString &path, const bool &quitFlag);
bool updateSubmodule(const QString &path, const QString &submoduleName);
bool unstageFiles(const QString &path, const QStringList &files);
QVector<RemoteInfo> listRemotes(const QString &path);
bool addRemote(const QString &path, const QString &remoteName,
               const QString &fetchUrl, const QString &pushUrl);
bool updateRemote(const QString &path, const QString &remoteName,
                  const QString &newRemoteName, const QString &fetchUrl,
                  const QString &pushUrl);
bool setDefaultRemote(const QString &path, const QString &remoteName);
QString getDefaultRemote(const QString &path);

SvnHistory getHistory(const QString &path, int limit = 100);

} // namespace SvnRepo

bool svn_init();
