// MIT License
// Copyright (c) 2026 Sourcerer

// Remotes ([paths]), synchronization, remote names and upstreams.

#include "HgRc.h"
#include "HgRepo.h"
#include "HgRepoPrivate.h"

#include <Log.h>

#include <QJsonObject>
#include <QMap>

#define THREAD_ID "HgRepo"

namespace HgRepo {

using namespace Private;

namespace {

const QString kPaths = QStringLiteral("paths");
const QString kDefaultTarget = QStringLiteral("Default");

struct PathInfo {
  QString name;
  QString url;
  QString pushUrl;
};

// Paths of the repository and of the user's configuration.
QVector<PathInfo> readPaths(Repository *repo) {
  QJsonArray array;
  RunOptions options;
  options.readOnly = true;
  if (!runJson(repo, HgError::RemoteFailed, {QStringLiteral("paths")}, &array,
               options)) {
    return {};
  }

  QVector<PathInfo> paths;
  QString defaultPush;
  for (const QJsonValue &value : array) {
    const QJsonObject object = value.toObject();
    PathInfo path;
    path.name = object.value("name").toString();
    path.url = object.value("url").toString();
    path.pushUrl = object.value("pushurl").toString();
    if (path.name == QStringLiteral("default-push")) {
      defaultPush = path.url;
      continue;
    }
    paths << path;
  }

  for (PathInfo &path : paths) {
    if (path.name == QStringLiteral("default") && path.pushUrl.isEmpty()) {
      path.pushUrl = defaultPush;
    }
  }
  return paths;
}

bool findPath(Repository *repo, const QString &name, PathInfo *path) {
  for (const PathInfo &candidate : readPaths(repo)) {
    if (candidate.name == name) {
      if (path) {
        *path = candidate;
      }
      return true;
    }
  }
  setLastError(HgError::RemoteFailed, -1,
               QStringLiteral("Remote %1 does not exist.").arg(name));
  return false;
}

bool loadRc(Repository *repo, HgRc *rc) {
  if (!rc->load()) {
    setLastError(HgError::RemoteFailed, -1, rc->errorMessage());
    return false;
  }
  return true;
}

bool saveRc(HgRc *rc) {
  if (!rc->save()) {
    setLastError(HgError::RemoteFailed, -1, rc->errorMessage());
    return false;
  }
  return true;
}

HgRc repositoryRc(const Repository *repo) {
  return HgRc(hgFile(repo, QStringLiteral("hgrc")));
}

bool isValidRemoteName(const QString &name) {
  static const QString forbidden = QStringLiteral("/:=@[] \t");
  if (name.isEmpty()) {
    return false;
  }
  for (const QChar c : name) {
    if (forbidden.contains(c)) {
      return false;
    }
  }
  return true;
}

void setPushUrl(HgRc *rc, const QString &remote, const QString &fetchUrl,
                const QString &pushUrl) {
  const QString key = remote + QStringLiteral(":pushurl");
  if (pushUrl.isEmpty() || pushUrl == fetchUrl) {
    rc->remove(kPaths, key);
  } else {
    rc->setValue(kPaths, key, pushUrl);
  }
}

QString targetKey(const QString &remote, const QString &target,
                  const char *kind) {
  return QStringLiteral("%1.%2.%3")
      .arg(remote, target, QString::fromLatin1(kind));
}

// (remote, remote name) the branch pulls from and pushes to: its configured
// upstream, else the same name on the default remote.
QPair<QString, QString> upstreamFor(Repository *repo, const QString &branch) {
  const auto [configured, remote, remoteBranch] = getUpstream(repo, branch);
  if (configured) {
    return {remote, remoteBranch};
  }
  return {getDefaultRemote(repo), branch};
}

bool localBranchExists(const QVector<HgBranchInfo> &branches,
                       const QString &name) {
  for (const HgBranchInfo &info : branches) {
    const QString listed =
        info.kind == HgBranchKind::Bookmark ? '@' + info.name : info.name;
    if (listed == name) {
      return true;
    }
  }
  return false;
}

QStringList remoteNames(Repository *repo, const char *revset,
                        const char *keyword) {
  HgCommandResult result;
  RunOptions options;
  options.readOnly = true;
  if (!runCommand(repo, HgError::RemoteFailed,
                  {QStringLiteral("log"), QStringLiteral("--rev"),
                   QString::fromLatin1(revset), QStringLiteral("--template"),
                   QStringLiteral("{join(%1, '\\n')}\\n")
                       .arg(QString::fromLatin1(keyword))},
                  &result, options)) {
    return {};
  }

  QStringList names;
  for (const QString &line : QString::fromUtf8(result.out)
                                 .remove('\r')
                                 .split('\n', Qt::SkipEmptyParts)) {
    if (!names.contains(line)) {
      names << line;
    }
  }
  return names;
}

bool pullPath(Repository *repo, const QString &path) {
  // Exit code 1: nothing new, which is fine.
  return runSimple(repo, HgError::PullFailed, {QStringLiteral("pull"), path},
                   true);
}

} // namespace

QVector<GitRepo::RemoteInfo> listRemotes(Repository *repo) {
  HgRc rc = repositoryRc(repo);
  if (!loadRc(repo, &rc)) {
    return {};
  }

  const QString defaultRemote = getDefaultRemote(repo);
  const QList<QPair<QString, QString>> targetEntries =
      rc.entries(QString::fromLatin1(kTargetsSection));

  QVector<GitRepo::RemoteInfo> remotes;
  for (const PathInfo &path : readPaths(repo)) {
    GitRepo::RemoteInfo remote;
    remote.name = path.name;
    remote.fetchUrl = path.url;
    remote.pushUrl = path.pushUrl.isEmpty() ? path.url : path.pushUrl;
    remote.isDefault = path.name == defaultRemote;
    remote.targets.append(
        GitRepo::RemoteTarget{kDefaultTarget, remote.fetchUrl, remote.pushUrl});

    // "<remote>.<target>.fetch" and ".push", sorted by target.
    QMap<QString, GitRepo::RemoteTarget> targets;
    const QString prefix = path.name + '.';
    for (const auto &[key, value] : targetEntries) {
      if (!key.startsWith(prefix)) {
        continue;
      }
      const QString rest = key.mid(prefix.size());
      const int dot = rest.lastIndexOf('.');
      if (dot <= 0) {
        continue;
      }
      const QString name = rest.left(dot);
      const QString kind = rest.mid(dot + 1);
      if (name == kDefaultTarget) {
        continue;
      }
      GitRepo::RemoteTarget &target = targets[name];
      target.name = name;
      if (kind == QStringLiteral("fetch")) {
        target.fetchUrl = value;
      } else if (kind == QStringLiteral("push")) {
        target.pushUrl = value;
      }
    }
    for (GitRepo::RemoteTarget target : targets) {
      if (target.fetchUrl.isEmpty()) {
        continue;
      }
      if (target.pushUrl.isEmpty()) {
        target.pushUrl = target.fetchUrl;
      }
      remote.targets.append(target);
    }

    remotes << remote;
  }
  return remotes;
}

QString getDefaultRemote(Repository *repo) {
  const QVector<PathInfo> paths = readPaths(repo);
  const auto exists = [&](const QString &name) {
    return std::any_of(paths.cbegin(), paths.cend(),
                       [&](const PathInfo &path) { return path.name == name; });
  };

  HgRc rc = repositoryRc(repo);
  if (rc.load()) {
    const QString configured = rc.value(QString::fromLatin1(kSourcererSection),
                                        QStringLiteral("defaultremote"));
    if (!configured.isEmpty() && exists(configured)) {
      return configured;
    }
  }

  if (exists(QStringLiteral("default")) || paths.isEmpty()) {
    return QStringLiteral("default");
  }
  return paths.first().name;
}

bool setDefaultRemote(Repository *repo, const QString &remoteName) {
  if (!findPath(repo, remoteName, nullptr)) {
    return false;
  }

  HgRc rc = repositoryRc(repo);
  if (!loadRc(repo, &rc)) {
    return false;
  }
  rc.setValue(QString::fromLatin1(kSourcererSection),
              QStringLiteral("defaultremote"), remoteName);
  return saveRc(&rc);
}

bool addRemote(Repository *repo, const QString &remoteName,
               const QString &fetchUrl, const QString &pushUrl) {
  if (!isValidRemoteName(remoteName) || fetchUrl.isEmpty()) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("A remote needs a name without /, :, =, @, "
                                "brackets or spaces, and a URL."));
    return false;
  }

  for (const PathInfo &path : readPaths(repo)) {
    if (path.name == remoteName) {
      setLastError(HgError::RemoteFailed, -1,
                   QStringLiteral("Remote %1 already exists.").arg(remoteName));
      return false;
    }
  }

  HgRc rc = repositoryRc(repo);
  if (!loadRc(repo, &rc)) {
    return false;
  }
  rc.setValue(kPaths, remoteName, fetchUrl);
  setPushUrl(&rc, remoteName, fetchUrl, pushUrl);
  return saveRc(&rc);
}

bool removeRemote(Repository *repo, const QString &remoteName) {
  HgRc rc = repositoryRc(repo);
  if (!loadRc(repo, &rc)) {
    return false;
  }

  if (!rc.contains(kPaths, remoteName)) {
    setLastError(HgError::RemoteFailed, -1,
                 QStringLiteral("Remote %1 is not defined in the repository's "
                                "configuration.")
                     .arg(remoteName));
    return false;
  }

  rc.remove(kPaths, remoteName);
  rc.remove(kPaths, remoteName + QStringLiteral(":pushurl"));
  if (remoteName == QStringLiteral("default")) {
    rc.remove(kPaths, QStringLiteral("default-push"));
  }

  const QString targetsSection = QString::fromLatin1(kTargetsSection);
  for (const auto &[key, value] : rc.entries(targetsSection)) {
    Q_UNUSED(value);
    if (key.startsWith(remoteName + '.')) {
      rc.remove(targetsSection, key);
    }
  }

  const QString sourcererSection = QString::fromLatin1(kSourcererSection);
  if (rc.value(sourcererSection, QStringLiteral("defaultremote")) ==
      remoteName) {
    rc.remove(sourcererSection, QStringLiteral("defaultremote"));
  }

  return saveRc(&rc);
}

bool updateRemote(Repository *repo, const QString &remoteName,
                  const QString &newRemoteName, const QString &fetchUrl,
                  const QString &pushUrl) {
  if (!isValidRemoteName(newRemoteName) || fetchUrl.isEmpty()) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("A remote needs a name without /, :, =, @, "
                                "brackets or spaces, and a URL."));
    return false;
  }

  HgRc rc = repositoryRc(repo);
  if (!loadRc(repo, &rc)) {
    return false;
  }

  if (newRemoteName != remoteName) {
    for (const PathInfo &path : readPaths(repo)) {
      if (path.name == newRemoteName) {
        setLastError(
            HgError::RemoteFailed, -1,
            QStringLiteral("Remote %1 already exists.").arg(newRemoteName));
        return false;
      }
    }

    rc.remove(kPaths, remoteName);
    rc.remove(kPaths, remoteName + QStringLiteral(":pushurl"));

    // Targets, the default remote and upstreams follow the new name.
    const QString targetsSection = QString::fromLatin1(kTargetsSection);
    for (const auto &[key, value] : rc.entries(targetsSection)) {
      if (key.startsWith(remoteName + '.')) {
        rc.remove(targetsSection, key);
        rc.setValue(targetsSection, newRemoteName + key.mid(remoteName.size()),
                    value);
      }
    }

    const QString sourcererSection = QString::fromLatin1(kSourcererSection);
    if (rc.value(sourcererSection, QStringLiteral("defaultremote")) ==
        remoteName) {
      rc.setValue(sourcererSection, QStringLiteral("defaultremote"),
                  newRemoteName);
    }

    const QString upstreamSection = QString::fromLatin1(kUpstreamSection);
    for (const auto &[key, value] : rc.entries(upstreamSection)) {
      if (value.startsWith(remoteName + '/')) {
        rc.setValue(upstreamSection, key,
                    newRemoteName + value.mid(remoteName.size()));
      }
    }
  }

  rc.setValue(kPaths, newRemoteName, fetchUrl);
  setPushUrl(&rc, newRemoteName, fetchUrl, pushUrl);
  return saveRc(&rc);
}

bool addTarget(Repository *repo, const QString &remoteName,
               const QString &targetName, const QString &fetchUrl,
               const QString &pushUrl) {
  if (targetName.isEmpty() || targetName == kDefaultTarget ||
      fetchUrl.isEmpty()) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("A target needs a name other than Default and "
                                "a fetch URL."));
    return false;
  }
  if (!findPath(repo, remoteName, nullptr)) {
    return false;
  }

  HgRc rc = repositoryRc(repo);
  if (!loadRc(repo, &rc)) {
    return false;
  }
  const QString section = QString::fromLatin1(kTargetsSection);
  rc.setValue(section, targetKey(remoteName, targetName, "fetch"), fetchUrl);
  rc.setValue(section, targetKey(remoteName, targetName, "push"),
              pushUrl.isEmpty() ? fetchUrl : pushUrl);
  return saveRc(&rc);
}

bool editTarget(Repository *repo, const QString &remoteName,
                const QString &targetName, const QString &newFetchUrl,
                const QString &newPushUrl) {
  return addTarget(repo, remoteName, targetName, newFetchUrl, newPushUrl);
}

bool deleteTarget(Repository *repo, const QString &remoteName,
                  const QString &targetName) {
  if (targetName.isEmpty() || targetName == kDefaultTarget) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("The Default target can't be deleted."));
    return false;
  }

  HgRc rc = repositoryRc(repo);
  if (!loadRc(repo, &rc)) {
    return false;
  }
  const QString section = QString::fromLatin1(kTargetsSection);
  rc.remove(section, targetKey(remoteName, targetName, "fetch"));
  rc.remove(section, targetKey(remoteName, targetName, "push"));
  return saveRc(&rc);
}

bool setDefaultTarget(Repository *repo, const QString &remoteName,
                      const QString &targetName) {
  if (targetName == kDefaultTarget) {
    return true;
  }

  PathInfo path;
  if (!findPath(repo, remoteName, &path)) {
    return false;
  }

  HgRc rc = repositoryRc(repo);
  if (!loadRc(repo, &rc)) {
    return false;
  }

  const QString section = QString::fromLatin1(kTargetsSection);
  const QString fetchUrl =
      rc.value(section, targetKey(remoteName, targetName, "fetch"));
  QString pushUrl =
      rc.value(section, targetKey(remoteName, targetName, "push"));
  if (fetchUrl.isEmpty()) {
    setLastError(HgError::RemoteFailed, -1,
                 QStringLiteral("Target %1 of %2 has no fetch URL.")
                     .arg(targetName, remoteName));
    return false;
  }
  if (pushUrl.isEmpty()) {
    pushUrl = fetchUrl;
  }

  // The previous URLs are kept as the hidden Default target, as GitRepo does.
  rc.setValue(section, targetKey(remoteName, kDefaultTarget, "fetch"),
              path.url);
  rc.setValue(section, targetKey(remoteName, kDefaultTarget, "push"),
              path.pushUrl.isEmpty() ? path.url : path.pushUrl);

  rc.setValue(kPaths, remoteName, fetchUrl);
  setPushUrl(&rc, remoteName, fetchUrl, pushUrl);
  return saveRc(&rc);
}

bool pull(Repository *repo, const QString &activeBranchName, bool rebase) {
  LOG_PERF(0, "Enter");

  const QString branch =
      activeBranchName.isEmpty()
          ? getActiveBranch(repo, GitRepo::BranchType::Local, false)
          : activeBranchName;
  const auto [remote, remoteBranch] = upstreamFor(repo, branch);

  if (!pullPath(repo, remote)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  // The upstream may not exist yet, e.g. a branch that was never pushed.
  const QString remoteRevset =
      branchRevset(remote + '/' + remoteBranch, GitRepo::BranchType::Remote);
  if (countRevisions(repo, remoteRevset) <= 0) {
    LOG_PERF(0, "Exit");
    return true;
  }

  QString target;
  if (!resolveNode(repo, HgError::PullFailed, remoteRevset, &target)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString current = workingDirectoryNode(repo);
  if (current.isEmpty()) {
    const bool updated =
        runSimple(repo, HgError::PullFailed,
                  {QStringLiteral("update"), QStringLiteral("--rev"), target});
    LOG_PERF(0, "Exit");
    return updated;
  }

  QString ancestor;
  if (!resolveNode(repo, HgError::PullFailed,
                   QStringLiteral("ancestor(%1, %2)").arg(current, target),
                   &ancestor)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  if (ancestor == target) {
    // Nothing new for this branch.
    LOG_PERF(0, "Exit");
    return true;
  }

  if (ancestor == current) {
    // A bookmark was moved forward by the pull; updating to it keeps it
    // active.
    const QString updateTo = branch.startsWith('@') ? branch.mid(1) : target;
    const bool updated = runSimple(
        repo, HgError::PullFailed,
        {QStringLiteral("update"), QStringLiteral("--rev"), updateTo});
    LOG_PERF(0, "Exit");
    return updated;
  }

  HgCommandResult result;
  if (rebase) {
    const bool rebased =
        runCommand(repo, HgError::PullFailed,
                   {QStringLiteral("rebase"), QStringLiteral("--base"),
                    QStringLiteral("."), QStringLiteral("--dest"), target,
                    QStringLiteral("--keepbranches")},
                   &result, RunOptions(), true);
    if (rebased && result.exitCode == 1 && getIsRebaseInProgress(repo)) {
      // Like GitRepo, a conflicting rebase is not left behind.
      runSimple(repo, HgError::PullFailed,
                {QStringLiteral("rebase"), QStringLiteral("--abort")});
      setLastError(HgError::MergeConflicts, 1,
                   QStringLiteral("Rebasing onto %1/%2 has conflicts, the "
                                  "rebase was aborted.")
                       .arg(remote, remoteBranch));
      LOG_PERF(0, "Exit");
      return false;
    }
    LOG_PERF(0, "Exit");
    return rebased;
  }

  // Diverged: the merge is left for the user to commit, with any conflicts.
  const bool merged =
      runCommand(repo, HgError::PullFailed,
                 {QStringLiteral("merge"), QStringLiteral("--rev"), target},
                 &result, RunOptions(), true);
  LOG_PERF(0, "Exit");
  return merged;
}

bool push(Repository *repo, const QString &activeBranchName, bool force) {
  LOG_PERF(0, "Enter");

  const QString branch =
      activeBranchName.isEmpty()
          ? getActiveBranch(repo, GitRepo::BranchType::Local, false)
          : activeBranchName;
  const auto [remote, remoteBranch] = upstreamFor(repo, branch);
  Q_UNUSED(remoteBranch);

  QStringList args{QStringLiteral("push")};
  if (branch.startsWith('@')) {
    args << QStringLiteral("--bookmark") << branch.mid(1);
  } else {
    args << QStringLiteral("--rev")
         << branchRevset(branch, GitRepo::BranchType::Local);
  }
  args << QStringLiteral("--new-branch");
  if (force) {
    args << QStringLiteral("--force");
  }
  args << remote;

  // Exit code 1: nothing to push.
  const bool pushed = runSimple(repo, HgError::PushFailed, args, true);
  LOG_PERF(0, "Exit");
  return pushed;
}

bool fetchAll(Repository *repo) {
  bool success = true;
  HgErrorContext firstError;
  for (const PathInfo &path : readPaths(repo)) {
    if (!pullPath(repo, path.name) && success) {
      success = false;
      firstError = getLastError();
    }
  }
  if (!success) {
    setLastError(firstError.error, firstError.errorCode,
                 firstError.errorMessage);
  }
  return success;
}

bool fetchRemote(Repository *repo, const QString &remoteName) {
  return pullPath(repo, remoteName);
}

QStringList listRemoteBranches(Repository *repo, const bool &quitFlag) {
  const QuitScope quitScope(&quitFlag);

  QStringList pathNames;
  for (const PathInfo &path : readPaths(repo)) {
    pathNames << path.name;
  }

  // Names recorded for remotes pulled by URL are left out.
  QStringList names;
  QString path;
  QString name;
  for (const QString &remoteName :
       remoteNames(repo, "remotebranches()", "remotebranches")) {
    if (splitRemoteName(remoteName, &path, &name) && pathNames.contains(path)) {
      names << remoteName;
    }
  }
  for (const QString &remoteName :
       remoteNames(repo, "remotebookmarks()", "remotebookmarks")) {
    if (splitRemoteName(remoteName, &path, &name) && pathNames.contains(path)) {
      names << path + QStringLiteral("/@") + name;
    }
  }
  names.removeDuplicates();
  return names;
}

QVector<GitRepo::BranchInfo>
listRemoteBranchesWithStatus(Repository *repo, const bool &quitFlag) {
  const QuitScope quitScope(&quitFlag);

  const QStringList remoteBranches = listRemoteBranches(repo, quitFlag);
  const QVector<HgBranchInfo> localBranches =
      listLocalBranchesWithKind(repo, false, quitFlag);
  const QString activeBranch =
      getActiveBranch(repo, GitRepo::BranchType::Local, quitFlag);

  // Local branches by the upstream they track.
  QHash<QString, QString> localOfUpstream;
  HgRc rc = repositoryRc(repo);
  if (rc.load()) {
    for (const auto &[local, upstream] :
         rc.entries(QString::fromLatin1(kUpstreamSection))) {
      localOfUpstream.insert(upstream, local);
    }
  }

  QVector<GitRepo::BranchInfo> infos;
  for (const QString &remoteBranch : remoteBranches) {
    if (quitFlag) {
      return {};
    }

    GitRepo::BranchInfo info;
    info.name = remoteBranch;

    QString path;
    QString name;
    splitRemoteName(remoteBranch, &path, &name);
    const QString local = localOfUpstream.value(remoteBranch, name);

    if (!localBranchExists(localBranches, local)) {
      info.statusInfo.status = GitRepo::BranchStatusType::NoUpstream;
      infos << info;
      continue;
    }

    // A pull already moves the branch head and the bookmark to the pulled
    // commits, so the active branch is where the working directory is.
    const QString localRevset =
        local == activeBranch ? QStringLiteral(".")
                              : branchRevset(local, GitRepo::BranchType::Local);
    const QString remoteRevset =
        branchRevset(remoteBranch, GitRepo::BranchType::Remote);
    const int ahead = countRevisions(
        repo, QStringLiteral("only(%1, %2)").arg(localRevset, remoteRevset));
    const int behind = countRevisions(
        repo, QStringLiteral("only(%1, %2)").arg(remoteRevset, localRevset));

    if (ahead < 0 || behind < 0) {
      info.statusInfo.status = GitRepo::BranchStatusType::Unknown;
    } else {
      info.statusInfo.aheadCount = static_cast<size_t>(ahead);
      info.statusInfo.behindCount = static_cast<size_t>(behind);
      if (ahead > 0 && behind > 0) {
        info.statusInfo.status = GitRepo::BranchStatusType::Diverged;
      } else if (ahead > 0) {
        info.statusInfo.status = GitRepo::BranchStatusType::Ahead;
      } else if (behind > 0) {
        info.statusInfo.status = GitRepo::BranchStatusType::Behind;
      } else {
        info.statusInfo.status = GitRepo::BranchStatusType::UpToDate;
      }
    }
    infos << info;
  }
  return infos;
}

bool deleteRemoteBranch(Repository *repo, const QString &branchName) {
  QString path;
  QString name;
  if (!splitRemoteName(branchName, &path, &name)) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("%1 is not a remote branch.").arg(branchName));
    return false;
  }

  if (name.startsWith('@')) {
    const QString bookmark = name.mid(1);

    // Pushing a bookmark that doesn't exist locally deletes it on the
    // remote. The local bookmark is put back afterwards.
    QString localNode;
    for (const HgBranchInfo &info :
         listLocalBranchesWithKind(repo, false, false)) {
      if (info.kind == HgBranchKind::Bookmark && info.name == bookmark) {
        localNode = info.node;
      }
    }

    if (!localNode.isEmpty() &&
        !runSimple(repo, HgError::PushFailed,
                   {QStringLiteral("bookmark"), QStringLiteral("--delete"),
                    bookmark})) {
      return false;
    }

    const bool deleted = runSimple(
        repo, HgError::PushFailed,
        {QStringLiteral("push"), QStringLiteral("--bookmark"), bookmark, path},
        true);
    const HgErrorContext error = getLastError();

    if (!localNode.isEmpty()) {
      runSimple(repo, HgError::PushFailed,
                {QStringLiteral("bookmark"), QStringLiteral("--inactive"),
                 QStringLiteral("--rev"), localNode, bookmark});
    }
    if (!deleted) {
      setLastError(error.error, error.errorCode, error.errorMessage);
    }
    return deleted;
  }

  // A named branch can't be deleted, it is closed on the remote.
  return closeNamedBranch(repo, name, QString(), QString()) &&
         runSimple(repo, HgError::PushFailed,
                   {QStringLiteral("push"), QStringLiteral("--rev"),
                    QStringLiteral("max(branch(%1))").arg(revsetString(name)),
                    path},
                   true);
}

bool setUpstream(Repository *repo, const QString &remoteName,
                 const QString &remoteBranchName,
                 const QString &localBranchName) {
  if (remoteName.isEmpty() || remoteBranchName.isEmpty() ||
      localBranchName.isEmpty()) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("A remote, a remote branch and a local branch "
                                "are required."));
    return false;
  }

  HgRc rc = repositoryRc(repo);
  if (!loadRc(repo, &rc)) {
    return false;
  }
  rc.setValue(QString::fromLatin1(kUpstreamSection), localBranchName,
              remoteName + '/' + remoteBranchName);
  return saveRc(&rc);
}

std::tuple<bool, QString, QString> getUpstream(Repository *repo,
                                               const QString &localBranchName) {
  HgRc rc = repositoryRc(repo);
  if (!repo || !rc.load()) {
    return {false, QString(), QString()};
  }

  QString remote;
  QString remoteBranch;
  if (!splitRemoteName(
          rc.value(QString::fromLatin1(kUpstreamSection), localBranchName),
          &remote, &remoteBranch)) {
    return {false, QString(), QString()};
  }
  return {true, remote, remoteBranch};
}

} // namespace HgRepo
