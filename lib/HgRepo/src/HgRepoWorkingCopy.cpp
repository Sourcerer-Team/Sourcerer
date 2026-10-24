// Status, staging and commits of the working directory.

#include "HgRepo.h"
#include "HgRepoPrivate.h"

#include <Log.h>

#include <QDir>
#include <QFile>

#define THREAD_ID "HgRepo"

namespace HgRepo {

using namespace Private;

namespace {

// Status flags of the changes shown in the working tree: modified, added,
// removed, deleted (missing) and unknown.
const QStringList kChangedFlags = {QStringLiteral("-mardu")};

bool isBelow(const QString &path, const QString &directory) {
  return path.startsWith(directory + '/');
}

// Changed subrepos that are one of paths or below one of them.
QVector<StatusEntry> subrepoEntriesBelow(Repository *repo,
                                         const QStringList &paths) {
  QVector<StatusEntry> entries;
  for (const StatusEntry &entry : subrepoStatusEntries(repo)) {
    for (const QString &path : paths) {
      if (entry.path == path || isBelow(entry.path, path) || path == '.') {
        entries.append(entry);
        break;
      }
    }
  }
  return entries;
}

// Paths whose removal is the other half of a rename in entries.
QSet<QString> renameSources(const QVector<StatusEntry> &entries) {
  QSet<QString> removed;
  for (const StatusEntry &entry : entries) {
    if (entry.status == 'R') {
      removed.insert(entry.path);
    }
  }

  QSet<QString> sources;
  for (const StatusEntry &entry : entries) {
    if (entry.status == 'A' && removed.contains(entry.source)) {
      sources.insert(entry.source);
    }
  }
  return sources;
}

QString commitUser(const Repository *repo, const QString &author,
                   const QString &email) {
  return author.isEmpty() ? userString(repo->author, repo->email)
                          : userString(author, email);
}

bool runCommit(Repository *repo, const QStringList &extraArgs,
               const QString &summary, const QString &message,
               const QString &author, const QString &email, bool cleanup,
               bool signoff, const QStringList &patterns) {
  const QString user = commitUser(repo, author, email);
  QString signoffAuthor = author.isEmpty() ? repo->author : author;
  QString signoffEmail = author.isEmpty() ? repo->email : email;

  const QString logMessage = makeLogMessage(summary, message, cleanup, signoff,
                                            signoffAuthor, signoffEmail);
  if (logMessage.trimmed().isEmpty()) {
    setLastError(HgError::CommitFailed, -1,
                 QStringLiteral("The commit message is empty."));
    return false;
  }

  // Arguments are sent over the command server pipe, so a long, multi-line
  // message is fine.
  QStringList args{QStringLiteral("commit")};
  args << extraArgs;
  if (!user.isEmpty()) {
    args << QStringLiteral("--user") << user;
  }
  args << QStringLiteral("--message") << logMessage;
  if (!patterns.isEmpty()) {
    args << QStringLiteral("--") << patterns;
  }

  HgCommandResult result;
  return runCommand(repo, HgError::CommitFailed, args, &result);
}

// Staged paths that still have changes to commit, and the staged set pruned
// of paths without changes.
bool collectStagedChanges(Repository *repo, QStringList *toCommit,
                          QSet<QString> *staged) {
  *staged = readStagedPaths(repo);
  toCommit->clear();
  if (staged->isEmpty()) {
    return true;
  }

  QVector<StatusEntry> entries;
  if (!readStatusEntries(repo, {QStringLiteral("-mard")}, {}, &entries,
                         HgError::CommitFailed)) {
    return false;
  }
  entries += subrepoStatusEntries(repo);

  for (const StatusEntry &entry : entries) {
    if (staged->contains(entry.path) && entry.status != '!') {
      toCommit->append(entry.path);
    }
  }

  // hg refuses to commit .hgsub without the subrepos it lists.
  if (toCommit->contains(QStringLiteral(".hgsub"))) {
    for (const QString &path : subrepoPaths(repo)) {
      if (!toCommit->contains(path)) {
        toCommit->append(path);
      }
    }
  }

  toCommit->sort();
  return true;
}

} // namespace

GitRepo::GitStatus getStatus(Repository *repo, const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  const QuitScope quitScope(&quitFlag);

  QVector<StatusEntry> entries;
  QSet<QString> unresolved;
  if (!readStatusEntries(repo, kChangedFlags, {}, &entries) ||
      !readUnresolvedPaths(repo, &unresolved) || quitFlag) {
    LOG_PERF(0, "Exit");
    return {};
  }
  entries += subrepoStatusEntries(repo);

  const QSet<QString> staged = readStagedPaths(repo);
  const QSet<QString> sources = renameSources(entries);

  GitRepo::GitStatus status;
  status.path = repo->root;

  QSet<QString> stillStaged;

  for (const StatusEntry &entry : entries) {
    if (unresolved.contains(entry.path)) {
      status.conflictedFiles += entry.path;
      continue;
    }

    const bool isStaged = staged.contains(entry.path);
    if (isStaged) {
      stillStaged.insert(entry.path);
    }

    switch (entry.status.toLatin1()) {
    case '?':
      status.newFiles += entry.path;
      break;
    case 'A':
      if (sources.contains(entry.source)) {
        (isStaged ? status.renamedStagedFiles : status.renamedFiles) +=
            entry.path;
      } else {
        (isStaged ? status.newStagedFiles : status.newFiles) += entry.path;
      }
      break;
    case 'M':
      (isStaged ? status.modifiedStagedFiles : status.modifiedFiles) +=
          entry.path;
      break;
    case 'R':
      if (sources.contains(entry.path)) {
        // Shown as the rename of the added file.
        break;
      }
      (isStaged ? status.deletedStagedFiles : status.deletedFiles) +=
          entry.path;
      break;
    case '!':
      status.deletedFiles += entry.path;
      break;
    default:
      break;
    }
  }

  // Forget staged paths whose changes were committed or reverted outside
  // Sourcerer.
  if (stillStaged.size() != staged.size()) {
    writeStagedPaths(repo, stillStaged);
  }

  LOG_PERF(0, "Exit");
  return status;
}

bool hasConflicts(Repository *repo) {
  QSet<QString> unresolved;
  return readUnresolvedPaths(repo, &unresolved) && !unresolved.isEmpty();
}

bool stageFiles(Repository *repo, const QStringList &files) {
  LOG_PERF(0, "Enter");

  if (!repo) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("The repository is not open."));
    return false;
  }

  const QStringList paths = relativeFilePaths(repo->root, files);
  if (paths.isEmpty()) {
    return true;
  }

  QVector<StatusEntry> entries;
  QSet<QString> unresolved;
  if (!readStatusEntries(repo, kChangedFlags, pathPatterns(paths), &entries,
                         HgError::StagingFailed) ||
      !readUnresolvedPaths(repo, &unresolved)) {
    LOG_PERF(0, "Exit");
    return false;
  }
  entries += subrepoEntriesBelow(repo, paths);

  QStringList toAdd;
  QStringList toRemove;
  QStringList toResolve;
  QSet<QString> staged = readStagedPaths(repo);

  for (const StatusEntry &entry : entries) {
    if (entry.status == '?') {
      toAdd << entry.path;
    } else if (entry.status == '!') {
      toRemove << entry.path;
    } else if (entry.status == 'A' && !entry.source.isEmpty()) {
      // A rename is committed together with the removal of its source.
      staged.insert(entry.source);
    }
    if (unresolved.contains(entry.path)) {
      toResolve << entry.path;
    }
    staged.insert(entry.path);
  }

  HgCommandResult result;
  if (!toAdd.isEmpty() &&
      !runCommand(repo, HgError::StagingFailed,
                  QStringList{QStringLiteral("add"), QStringLiteral("--")} +
                      pathPatterns(toAdd),
                  &result)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!toRemove.isEmpty() &&
      !runCommand(repo, HgError::StagingFailed,
                  QStringList{QStringLiteral("remove"),
                              QStringLiteral("--after"), QStringLiteral("--")} +
                      pathPatterns(toRemove),
                  &result)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  // Staging a conflicted file marks it resolved, as "git add" does.
  if (!toResolve.isEmpty() && !markResolved(repo, toResolve)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  // An unshelve stopped by conflicts is finished once nothing is left to
  // resolve. It restores the changes without committing them.
  if (!toResolve.isEmpty() &&
      QFile::exists(repo->root + QStringLiteral("/.hg/shelvedstate")) &&
      !hasConflicts(repo)) {
    if (!runCommand(repo, HgError::StagingFailed,
                    {QStringLiteral("unshelve"), QStringLiteral("--continue")},
                    &result)) {
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  const bool written = writeStagedPaths(repo, staged);
  LOG_PERF(0, "Exit");
  return written;
}

bool unstageFiles(Repository *repo, const QStringList &files) {
  if (!repo) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("The repository is not open."));
    return false;
  }

  const QStringList paths = relativeFilePaths(repo->root, files);
  if (paths.isEmpty()) {
    return true;
  }

  QVector<StatusEntry> entries;
  if (!readStatusEntries(repo, kChangedFlags, pathPatterns(paths), &entries,
                         HgError::StagingFailed)) {
    return false;
  }

  QSet<QString> staged = readStagedPaths(repo);

  for (const StatusEntry &entry : entries) {
    staged.remove(entry.path);
    if (!entry.source.isEmpty()) {
      staged.remove(entry.source);
    }
  }

  for (const QString &path : paths) {
    staged.remove(path);
    for (auto it = staged.begin(); it != staged.end();) {
      it = isBelow(*it, path) ? staged.erase(it) : std::next(it);
    }
  }

  return writeStagedPaths(repo, staged);
}

bool discardFiles(Repository *repo, const QStringList &files) {
  if (!repo) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("The repository is not open."));
    return false;
  }

  const QStringList paths = relativeFilePaths(repo->root, files);
  if (paths.isEmpty()) {
    return true;
  }

  QVector<StatusEntry> entries;
  if (!readStatusEntries(repo, {QStringLiteral("-mard")}, pathPatterns(paths),
                         &entries, HgError::DiscardFailed)) {
    return false;
  }

  // A moved Mercurial subrepo goes back to its pinned node.
  for (const StatusEntry &entry : subrepoEntriesBelow(repo, paths)) {
    if (entry.status == 'M') {
      for (const HgSubrepo &subrepo : listSubrepos(repo)) {
        if (subrepo.path == entry.path &&
            subrepo.kind == HgSubrepoKind::Mercurial &&
            !updateSubmodule(repo, subrepo.path)) {
          return false;
        }
      }
    }
  }

  QStringList toRevert;
  QStringList toForget;
  for (const StatusEntry &entry : entries) {
    if (entry.status == 'A') {
      toForget << entry.path;
      if (!entry.source.isEmpty()) {
        // Undoing a rename brings the source back.
        toRevert << entry.source;
      }
    } else {
      toRevert << entry.path;
    }
  }
  toRevert.removeDuplicates();

  HgCommandResult result;
  if (!toForget.isEmpty() &&
      !runCommand(repo, HgError::DiscardFailed,
                  QStringList{QStringLiteral("forget"), QStringLiteral("--")} +
                      pathPatterns(toForget),
                  &result)) {
    return false;
  }

  if (!toRevert.isEmpty() &&
      !runCommand(repo, HgError::DiscardFailed,
                  QStringList{QStringLiteral("revert"),
                              QStringLiteral("--no-backup"),
                              QStringLiteral("--")} +
                      pathPatterns(toRevert),
                  &result)) {
    return false;
  }

  QSet<QString> staged = readStagedPaths(repo);
  for (const QString &path : toForget + toRevert) {
    staged.remove(path);
  }
  return writeStagedPaths(repo, staged);
}

bool commitStaged(Repository *repo, const QString &summary,
                  const QString &message, const QString &author,
                  const QString &email, bool cleanup, bool signoff,
                  bool signCommit) {
  LOG_PERF(0, "Enter");

  if (!repo) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("The repository is not open."));
    return false;
  }

  if (signCommit) {
    return setUnsupported("Signing commits");
  }

  // A graft stopped by conflicts is finished by Mercurial, with the message of
  // the grafted commit.
  if (QFile::exists(repo->root + QStringLiteral("/.hg/graftstate"))) {
    HgCommandResult result;
    const bool continued = runCommand(
        repo, HgError::CommitFailed,
        {QStringLiteral("graft"), QStringLiteral("--continue")}, &result);
    if (continued) {
      writeStagedPaths(repo, {});
    }
    LOG_PERF(0, "Exit");
    return continued;
  }

  if (QFile::exists(repo->root + QStringLiteral("/.hg/shelvedstate"))) {
    setLastError(HgError::CommitFailed, -1,
                 QStringLiteral("A shelve is being restored. Stage the "
                                "resolved files to finish it, or abort it."));
    LOG_PERF(0, "Exit");
    return false;
  }

  // Mercurial commits a merge with every change of the working directory.
  const bool merging = getIsMergeInProgress(repo);

  QStringList toCommit;
  QSet<QString> staged;
  if (!merging && !collectStagedChanges(repo, &toCommit, &staged)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!merging && toCommit.isEmpty()) {
    setLastError(HgError::NothingToCommit, -1,
                 QStringLiteral("There are no staged changes to commit."));
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!runCommit(repo, {}, summary, message, author, email, cleanup, signoff,
                 merging ? QStringList() : pathPatterns(toCommit))) {
    LOG_PERF(0, "Exit");
    return false;
  }

  if (merging) {
    staged.clear();
  } else {
    for (const QString &path : toCommit) {
      staged.remove(path);
    }
  }

  const bool written = writeStagedPaths(repo, staged);
  LOG_PERF(0, "Exit");
  return written;
}

bool amend(Repository *repo, const QString &summary, const QString &message,
           const QString &author, const QString &email, bool signoff,
           bool signCommit) {
  if (!repo) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("The repository is not open."));
    return false;
  }

  if (signCommit) {
    return setUnsupported("Signing commits");
  }

  QStringList toCommit;
  QSet<QString> staged;
  if (!collectStagedChanges(repo, &toCommit, &staged)) {
    return false;
  }

  // Without staged files only the message changes: exclude every file, or
  // hg would amend all changes of the working directory.
  QStringList extraArgs{QStringLiteral("--amend")};
  if (toCommit.isEmpty()) {
    extraArgs << QStringLiteral("--exclude") << QStringLiteral("glob:**");
  }

  if (!runCommit(repo, extraArgs, summary, message, author, email, false,
                 signoff, pathPatterns(toCommit))) {
    return false;
  }

  for (const QString &path : toCommit) {
    staged.remove(path);
  }
  return writeStagedPaths(repo, staged);
}

bool markResolved(Repository *repo, const QStringList &files) {
  if (!repo) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("The repository is not open."));
    return false;
  }

  const QStringList paths = relativeFilePaths(repo->root, files);
  if (paths.isEmpty()) {
    return true;
  }

  HgCommandResult result;
  return runCommand(repo, HgError::ResolveFailed,
                    QStringList{QStringLiteral("resolve"),
                                QStringLiteral("--mark"),
                                QStringLiteral("--")} +
                        pathPatterns(paths),
                    &result);
}

bool getIsMergeInProgress(Repository *repo) {
  if (!repo) {
    return false;
  }

  // Interrupted grafts and unshelves are finished or aborted like a merge.
  if (QFile::exists(repo->root + QStringLiteral("/.hg/graftstate")) ||
      QFile::exists(repo->root + QStringLiteral("/.hg/shelvedstate"))) {
    return true;
  }

  HgCommandResult result;
  RunOptions options;
  options.readOnly = true;
  return runCommand(repo, HgError::StatusFailed,
                    {QStringLiteral("log"), QStringLiteral("--rev"),
                     QStringLiteral("p2()"), QStringLiteral("--template"),
                     QStringLiteral("{node}")},
                    &result, options) &&
         !result.out.trimmed().isEmpty();
}

} // namespace HgRepo
