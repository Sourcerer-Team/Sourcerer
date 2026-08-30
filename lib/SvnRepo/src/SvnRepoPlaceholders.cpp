#include "../include/SvnRepo.h"

namespace SvnRepo {

namespace {

template <typename T> T defaultValue() {
  return T{};
}

} // namespace

bool abortMerge(const QString &) {
  return false;
}

bool addSubmodule(const QString &, const QString &, const QString &,
                  const QString &, const QString &) {
  return false;
}

bool amend(const QString &, const QString &, const QString &, const QString &,
           const QString &, bool, bool) {
  return false;
}

bool applyPatch(const QString &, const QString &) {
  return false;
}

bool applyStash(const QString &, int) {
  return false;
}

QString listConflictsForStashFile(const QString &, int, const QString &) {
  return {};
}

bool checkoutBranch(const QString &, const QString &, BranchType) {
  return false;
}

bool checkoutCommit(const QString &, const QString &) {
  return false;
}

QString openRepository(const QString &) {
  return QString();
}

void closeRepository(const QString &) {}

bool cloneRepository(const QString &, const QString &, const QString &) {
  return false;
}

bool commitStaged(const QString &, const QString &, const QString &,
                  const QString &, const QString &, bool, bool, bool) {
  return false;
}

bool createBranchFromCommit(const QString &, const QString &, const QString &) {
  return false;
}

QString createPatchFromCommit(const QString &, const QString &) {
  return QString();
}

bool createTagFromCommit(const QString &, const QString &, const QString &,
                         const QString &, const QString &, const QString &) {
  return false;
}

bool addTarget(const QString &, const QString &, const QString &,
               const QString &, const QString &) {
  return false;
}

bool editTarget(const QString &, const QString &, const QString &,
                const QString &, const QString &) {
  return false;
}

bool deleteTarget(const QString &, const QString &, const QString &) {
  return false;
}

bool setDefaultTarget(const QString &, const QString &, const QString &) {
  return false;
}

bool deleteBranch(const QString &, const QString &) {
  return false;
}

bool deleteRemoteBranch(const QString &, const QString &) {
  return false;
}

bool deleteStash(const QString &, int) {
  return false;
}

bool deleteAllStashes(const QString &) {
  return false;
}

bool deleteSubmodule(const QString &, const QString &, const QString &) {
  return false;
}

bool deleteTag(const QString &, const QString &) {
  return false;
}

bool deleteRemoteTag(const QString &, const QString &) {
  return false;
}

FilesDiff diffFiles(const QString &, const QStringList &) {
  return {};
}

FilesDiff diffFilesFromCommit(const QString &, const QStringList &,
                              const QString &) {
  return {};
}

bool discardFiles(const QString &, const QStringList &) {
  return false;
}

bool fetchAll(const QString &) {
  return false;
}

bool fetchRemote(const QString &, const QString &) {
  return false;
}

bool rebaseBranches(const QString &, const QString &, const QString &,
                    const QString &, const QString &) {
  return false;
}

qint64 getFileSizeFromCommit(const QString &, const QString &,
                             const QString &) {
  return -1;
}

QString getActiveBranch(const QString &, BranchType, bool) {
  return {};
}

QStringList getCommitBranches(const QString &, const QString &, bool) {
  return {};
}

QStringList getCommitTags(const QString &, const QString &, bool) {
  return {};
}

QStringList getCommitHeads(const QString &, const QString &, bool) {
  return {};
}

bool getCommitIsHead(const QString &, const QString &, bool) {
  return false;
}

CommitStatus getCommitStatus(const QString &, const QString &) {
  return {};
}

CommitStatus getStashStatus(const QString &, int) {
  return {};
}

bool getIsMergeInProgress(const QString &) {
  return false;
}

SvnHistory getBranchHistory(const QString &, const QString &, BranchType,
                            bool) {
  return {};
}

QString getSubmodulePath(const QString &, const QString &) {
  return {};
}

std::tuple<bool, QString, QString> getUpstream(const QString &,
                                               const QString &) {
  return {false, QString(), QString()};
}

bool initRepository(const QString &, const QString &, const QString &) {
  return false;
}

QString listConflictsForAFile(const QString &, const QString &) {
  return {};
}

QStringList listLocalBranches(const QString &, const bool &) {
  return {};
}

QStringList listRemoteBranches(const QString &, const bool &) {
  return {};
}

QVector<BranchInfo> listRemoteBranchesWithStatus(const QString &,
                                                 const bool &) {
  return {};
}

QStringList listStashes(const QString &, const bool &) {
  return {};
}

QStringList listSubmodules(const QString &) {
  return {};
}

QVector<SubmoduleInfo> listSubmodulesWithStatus(const QString &, const bool &) {
  return {};
}

QStringList listTags(const QString &) {
  return {};
}

bool merge(const QString &, const QString &, const QString &, const QString &,
           const QString &) {
  return false;
}

bool pull(const QString &, const QString &, bool) {
  return false;
}

bool push(const QString &, const QString &, bool) {
  return false;
}

bool pushTag(const QString &, const QString &) {
  return false;
}

bool pushAllTags(const QString &) {
  return false;
}

bool removeRemote(const QString &, const QString &) {
  return false;
}

bool renameBranch(const QString &, const QString &, const QString &) {
  return false;
}

bool renameTag(const QString &, const QString &, const QString &) {
  return false;
}

bool resetToCommit(const QString &, const QString &, ResetType) {
  return false;
}

bool revertCommit(const QString &, const QString &, const QString &,
                  const QString &, const QString &) {
  return false;
}

bool cherryPickCommit(const QString &, const QString &, const QString &,
                      const QString &) {
  return false;
}

bool setUpstream(const QString &, const QString &, const QString &,
                 const QString &) {
  return false;
}

bool stageFiles(const QString &, const QStringList &) {
  return false;
}

bool stash(const QString &) {
  return false;
}

SvnStatus getStatus(const QString &, const bool &) {
  return {};
}

bool updateSubmodule(const QString &, const QString &) {
  return false;
}

bool unstageFiles(const QString &, const QStringList &) {
  return false;
}

QVector<RemoteInfo> listRemotes(const QString &) {
  return {};
}

bool addRemote(const QString &, const QString &, const QString &,
               const QString &) {
  return false;
}

bool updateRemote(const QString &, const QString &, const QString &,
                  const QString &, const QString &) {
  return false;
}

bool setDefaultRemote(const QString &, const QString &) {
  return false;
}

QString getDefaultRemote(const QString &) {
  return {};
}

} // namespace SvnRepo
