// Named branches, bookmarks, tags, merging and history editing.

#include "HgRepo.h"
#include "HgRepoPrivate.h"

#include <Log.h>

#include <QFile>
#include <QJsonObject>
#include <QSet>

#include <algorithm>

#define THREAD_ID "HgRepo"

namespace HgRepo {

using namespace Private;

namespace Private {

bool runSimple(Repository *repo, HgError kind, const QStringList &args,
               bool allowExitCodeOne) {
  HgCommandResult result;
  return runCommand(repo, kind, args, &result, RunOptions(), allowExitCodeOne);
}

QString branchRevset(const QString &name, GitRepo::BranchType type) {
  if (type == GitRepo::BranchType::Tag) {
    return QStringLiteral("tag(%1)").arg(revsetString(name));
  }

  QString path;
  QString remoteName;
  if (type == GitRepo::BranchType::Remote &&
      splitRemoteName(name, &path, &remoteName)) {
    if (remoteName.startsWith('@')) {
      return QStringLiteral("remotebookmarks(%1)")
          .arg(revsetString(path + '/' + remoteName.mid(1)));
    }
    return QStringLiteral("max(remotebranches(%1))")
        .arg(revsetString(path + '/' + remoteName));
  }

  if (name.startsWith('@')) {
    return QStringLiteral("bookmark(%1)").arg(revsetString(name.mid(1)));
  }
  return QStringLiteral("max(branch(%1))").arg(revsetString(name));
}

bool resolveNode(Repository *repo, HgError kind, const QString &revset,
                 QString *node) {
  HgCommandResult result;
  RunOptions options;
  options.readOnly = true;
  if (!runCommand(repo, kind,
                  {QStringLiteral("log"), QStringLiteral("--rev"),
                   QStringLiteral("last(%1)").arg(revset),
                   QStringLiteral("--template"), QStringLiteral("{node}")},
                  &result, options)) {
    return false;
  }

  *node = QString::fromLatin1(result.out).trimmed();
  if (node->isEmpty()) {
    setLastError(kind, -1, QStringLiteral("%1 matches no commit.").arg(revset));
    return false;
  }
  return true;
}

int countRevisions(Repository *repo, const QString &revset) {
  HgCommandResult result;
  RunOptions options;
  options.readOnly = true;
  if (!runCommand(repo, HgError::CommandFailed,
                  {QStringLiteral("log"), QStringLiteral("--rev"), revset,
                   QStringLiteral("--template"), QStringLiteral("x")},
                  &result, options)) {
    return -1;
  }
  return static_cast<int>(result.out.count('x'));
}

QString workingDirectoryNode(Repository *repo) {
  HgCommandResult result;
  RunOptions options;
  options.readOnly = true;
  if (!runCommand(repo, HgError::CommandFailed,
                  {QStringLiteral("log"), QStringLiteral("--rev"),
                   QStringLiteral("."), QStringLiteral("--template"),
                   QStringLiteral("{node}")},
                  &result, options)) {
    return QString();
  }
  const QString node = QString::fromLatin1(result.out).trimmed();
  return node == QLatin1String(kNullNode) ? QString() : node;
}

QString activeBookmark(Repository *repo) {
  // The bookmark stays active when a pull moves it past the working
  // directory, where the {activebookmark} keyword of "." would be empty.
  QJsonArray bookmarks;
  RunOptions options;
  options.readOnly = true;
  if (!runJson(repo, HgError::CommandFailed, {QStringLiteral("bookmarks")},
               &bookmarks, options)) {
    return QString();
  }
  for (const QJsonValue &value : bookmarks) {
    const QJsonObject object = value.toObject();
    if (object.value("active").toBool()) {
      return object.value("bookmark").toString();
    }
  }
  return QString();
}

bool requireCleanWorkingDirectory(Repository *repo) {
  QVector<StatusEntry> entries;
  if (!readStatusEntries(repo, {QStringLiteral("-mard")}, {}, &entries)) {
    return false;
  }
  if (!entries.isEmpty()) {
    setLastError(HgError::UncommittedChanges, -1,
                 QStringLiteral("Commit, shelve or discard the changes of the "
                                "working directory first."));
    return false;
  }
  return true;
}

QString hgFile(const Repository *repo, const QString &name) {
  return repo->root + QStringLiteral("/.hg/") + name;
}

QStringList userArgs(const Repository *repo, const QString &author,
                     const QString &email) {
  const QString user = author.isEmpty() ? userString(repo->author, repo->email)
                                        : userString(author, email);
  return user.isEmpty() ? QStringList()
                        : QStringList{QStringLiteral("--user"), user};
}

bool splitRemoteName(const QString &remoteName, QString *path, QString *name) {
  const int slash = remoteName.indexOf('/');
  if (slash <= 0 || slash == remoteName.size() - 1) {
    return false;
  }
  *path = remoteName.left(slash);
  *name = remoteName.mid(slash + 1);
  return true;
}

} // namespace Private

namespace {

QString displayName(const QString &branchName) {
  return branchName.startsWith('@') ? branchName.mid(1) : branchName;
}

QString currentNamedBranch(Repository *repo) {
  HgCommandResult result;
  RunOptions options;
  options.readOnly = true;
  if (!runCommand(repo, HgError::BranchFailed, {QStringLiteral("branch")},
                  &result, options)) {
    return QString();
  }
  return QString::fromUtf8(result.out).trimmed();
}

QStringList outputLines(const QByteArray &out) {
  QStringList lines;
  for (const QString &line :
       QString::fromUtf8(out).remove('\r').split('\n', Qt::SkipEmptyParts)) {
    if (!line.trimmed().isEmpty() && !lines.contains(line)) {
      lines << line;
    }
  }
  return lines;
}

QStringList templateLines(Repository *repo, const QString &revset,
                          const QString &templ) {
  HgCommandResult result;
  RunOptions options;
  options.readOnly = true;
  if (!runCommand(repo, HgError::BranchFailed,
                  {QStringLiteral("log"), QStringLiteral("--rev"), revset,
                   QStringLiteral("--template"), templ},
                  &result, options)) {
    return {};
  }
  return outputLines(result.out);
}

bool updateTo(Repository *repo, const QString &revisionOrBookmark) {
  return runSimple(
      repo, HgError::CheckoutFailed,
      {QStringLiteral("update"), QStringLiteral("--rev"), revisionOrBookmark});
}

// Brings the working directory back to a bookmark or node after a temporary
// update.
bool restoreWorkingDirectory(Repository *repo, const QString &bookmark,
                             const QString &node) {
  if (!bookmark.isEmpty()) {
    return updateTo(repo, bookmark);
  }
  if (!node.isEmpty()) {
    return updateTo(repo, node);
  }
  return true;
}

QString defaultPushPath(Repository *repo) {
  return getDefaultRemote(repo);
}

struct TagInfo {
  QString name;
  QString node;
  bool local = false;
};

QVector<TagInfo> readTags(Repository *repo) {
  QJsonArray array;
  RunOptions options;
  options.readOnly = true;
  if (!runJson(repo, HgError::TagFailed, {QStringLiteral("tags")}, &array,
               options)) {
    return {};
  }

  QVector<TagInfo> tags;
  for (const QJsonValue &value : array) {
    const QJsonObject object = value.toObject();
    TagInfo tag;
    tag.name = object.value("tag").toString();
    if (tag.name == QStringLiteral("tip")) {
      continue;
    }
    tag.node = object.value("node").toString();
    tag.local = object.value("type").toString() == QStringLiteral("local");
    tags << tag;
  }
  return tags;
}

bool findTag(Repository *repo, const QString &tagName, TagInfo *tag) {
  for (const TagInfo &candidate : readTags(repo)) {
    if (candidate.name == tagName) {
      *tag = candidate;
      return true;
    }
  }
  setLastError(HgError::TagFailed, -1,
               QStringLiteral("Tag %1 does not exist.").arg(tagName));
  return false;
}

// The commit that added tag to .hgtags.
bool findTagCommit(Repository *repo, const TagInfo &tag, QString *node) {
  const QStringList candidates =
      templateLines(repo, QStringLiteral("reverse(file('path:.hgtags'))"),
                    QStringLiteral("{node}\n"));
  const QByteArray entry = (tag.node + ' ' + tag.name).toUtf8();
  for (const QString &candidate : candidates) {
    bool exists = false;
    const QByteArray content =
        readFileAtRevision(repo, QStringLiteral(".hgtags"), candidate, &exists);
    if (exists && content.split('\n').contains(entry)) {
      // The oldest commit with the entry added it, keep looking further back.
      *node = candidate;
    } else if (!node->isEmpty()) {
      break;
    }
  }

  if (node->isEmpty()) {
    setLastError(HgError::TagFailed, -1,
                 QStringLiteral("No commit records the tag %1.").arg(tag.name));
    return false;
  }
  return true;
}

bool pushRevisions(Repository *repo, const QString &revset,
                   const QStringList &extraArgs = {}) {
  QStringList args{QStringLiteral("push"), QStringLiteral("--rev"), revset,
                   QStringLiteral("--new-branch")};
  args << extraArgs << defaultPushPath(repo);
  // Exit code 1: nothing to push.
  return runSimple(repo, HgError::PushFailed, args, true);
}

bool hasState(const Repository *repo, const char *stateFile) {
  return QFile::exists(hgFile(repo, QString::fromLatin1(stateFile)));
}

} // namespace

QString getActiveBranch(Repository *repo, GitRepo::BranchType type,
                        const bool &quitFlag) {
  const QuitScope quitScope(&quitFlag);

  switch (type) {
  case GitRepo::BranchType::Local: {
    const QString bookmark = activeBookmark(repo);
    return bookmark.isEmpty() ? currentNamedBranch(repo) : '@' + bookmark;
  }
  case GitRepo::BranchType::Tag: {
    const QStringList tags = templateLines(
        repo, QStringLiteral("."), QStringLiteral("{join(tags, '\\n')}\n"));
    for (const QString &tag : tags) {
      if (tag != QStringLiteral("tip")) {
        return tag;
      }
    }
    return QString();
  }
  default:
    return QString();
  }
}

QVector<HgBranchInfo> listLocalBranchesWithKind(Repository *repo,
                                                bool includeClosed,
                                                const bool &quitFlag) {
  const QuitScope quitScope(&quitFlag);

  RunOptions options;
  options.readOnly = true;

  QStringList branchArgs{QStringLiteral("branches")};
  if (includeClosed) {
    branchArgs << QStringLiteral("--closed");
  }

  QJsonArray branches;
  QJsonArray bookmarks;
  if (!runJson(repo, HgError::BranchFailed, branchArgs, &branches, options) ||
      !runJson(repo, HgError::BranchFailed, {QStringLiteral("bookmarks")},
               &bookmarks, options)) {
    return {};
  }

  const QString current = currentNamedBranch(repo);

  QVector<HgBranchInfo> named;
  bool currentListed = false;
  for (const QJsonValue &value : branches) {
    const QJsonObject object = value.toObject();
    HgBranchInfo info;
    info.name = object.value("branch").toString();
    info.kind = HgBranchKind::NamedBranch;
    info.active = info.name == current;
    info.closed = object.value("closed").toBool();
    info.node = object.value("node").toString();
    currentListed = currentListed || info.active;
    named << info;
  }

  // "hg branch NAME" without a commit yet.
  if (!current.isEmpty() && !currentListed) {
    QJsonArray closedBranches;
    runJson(repo, HgError::BranchFailed,
            {QStringLiteral("branches"), QStringLiteral("--closed")},
            &closedBranches, options);
    bool exists = false;
    for (const QJsonValue &value : closedBranches) {
      exists = exists || value.toObject().value("branch").toString() == current;
    }
    if (!exists) {
      HgBranchInfo info;
      info.name = current;
      info.kind = HgBranchKind::NamedBranch;
      info.active = true;
      info.isNew = true;
      named << info;
    }
  }

  QVector<HgBranchInfo> marks;
  for (const QJsonValue &value : bookmarks) {
    const QJsonObject object = value.toObject();
    HgBranchInfo info;
    info.name = object.value("bookmark").toString();
    info.kind = HgBranchKind::Bookmark;
    info.active = object.value("active").toBool();
    info.node = object.value("node").toString();
    marks << info;
  }

  const auto byName = [](const HgBranchInfo &a, const HgBranchInfo &b) {
    return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
  };
  std::sort(named.begin(), named.end(), byName);
  std::sort(marks.begin(), marks.end(), byName);

  return named + marks;
}

QStringList listLocalBranches(Repository *repo, const bool &quitFlag) {
  QStringList names;
  for (const HgBranchInfo &info :
       listLocalBranchesWithKind(repo, false, quitFlag)) {
    names << (info.kind == HgBranchKind::Bookmark ? '@' + info.name
                                                  : info.name);
  }
  return names;
}

bool checkoutBranch(Repository *repo, const QString &branchOrTagName,
                    GitRepo::BranchType type) {
  if (!repo || branchOrTagName.isEmpty()) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("A branch name is required."));
    return false;
  }

  switch (type) {
  case GitRepo::BranchType::Tag:
    return updateTo(repo, branchRevset(branchOrTagName, type));

  case GitRepo::BranchType::Remote: {
    QString path;
    QString name;
    if (!splitRemoteName(branchOrTagName, &path, &name)) {
      setLastError(
          HgError::InvalidArgument, -1,
          QStringLiteral("%1 is not a remote branch.").arg(branchOrTagName));
      return false;
    }

    QString remoteNode;
    if (!resolveNode(repo, HgError::CheckoutFailed,
                     branchRevset(branchOrTagName, type), &remoteNode)) {
      return false;
    }

    if (name.startsWith('@')) {
      const QString bookmark = name.mid(1);
      bool exists = false;
      for (const HgBranchInfo &info :
           listLocalBranchesWithKind(repo, false, false)) {
        exists = exists ||
                 (info.kind == HgBranchKind::Bookmark && info.name == bookmark);
      }
      // An existing bookmark is not moved, like an existing Git branch.
      if (!exists &&
          !runSimple(repo, HgError::CheckoutFailed,
                     {QStringLiteral("bookmark"), QStringLiteral("--rev"),
                      remoteNode, bookmark})) {
        return false;
      }
      setUpstream(repo, path, name, name);
      return updateTo(repo, bookmark);
    }

    setUpstream(repo, path, name, name);
    return updateTo(repo, remoteNode);
  }

  default:
    if (branchOrTagName.startsWith('@')) {
      return updateTo(repo, branchOrTagName.mid(1));
    }
    // A new branch has no commits to update to.
    if (branchOrTagName == currentNamedBranch(repo) &&
        countRevisions(repo, QStringLiteral("branch(%1)")
                                 .arg(revsetString(branchOrTagName))) <= 0) {
      return true;
    }
    return updateTo(repo, branchRevset(branchOrTagName, type));
  }
}

bool checkoutCommit(Repository *repo, const QString &commit) {
  return updateTo(repo, commit);
}

bool createBranchFromCommit(Repository *repo, const QString &commit,
                            const QString &branchName) {
  const QString bookmark = displayName(branchName);
  if (bookmark.isEmpty()) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("A bookmark name is required."));
    return false;
  }

  return runSimple(repo, HgError::BranchFailed,
                   {QStringLiteral("bookmark"), QStringLiteral("--rev"),
                    commit.isEmpty() ? QStringLiteral(".") : commit, bookmark});
}

bool createNamedBranchFromCommit(Repository *repo, const QString &commit,
                                 const QString &branchName) {
  if (branchName.isEmpty() || branchName.startsWith('@')) {
    setLastError(
        HgError::InvalidArgument, -1,
        QStringLiteral("%1 is not a valid branch name.").arg(branchName));
    return false;
  }

  if (!commit.isEmpty() && !updateTo(repo, commit)) {
    return false;
  }

  return runSimple(repo, HgError::BranchFailed,
                   {QStringLiteral("branch"), branchName});
}

bool closeNamedBranch(Repository *repo, const QString &branchName,
                      const QString &author, const QString &email) {
  if (!requireCleanWorkingDirectory(repo)) {
    return false;
  }

  QString tip;
  if (!resolveNode(repo, HgError::BranchFailed,
                   branchRevset(branchName, GitRepo::BranchType::Local),
                   &tip)) {
    return false;
  }

  const QString previousNode = workingDirectoryNode(repo);
  const QString previousBookmark = activeBookmark(repo);

  if (!updateTo(repo, tip)) {
    return false;
  }

  QStringList args{QStringLiteral("commit"), QStringLiteral("--close-branch"),
                   QStringLiteral("--message"),
                   QStringLiteral("Close branch %1").arg(branchName)};
  args << userArgs(repo, author, email);
  const bool closed = runSimple(repo, HgError::BranchFailed, args);

  // The parent of the closing commit when the branch was checked out.
  const HgErrorContext error = getLastError();
  restoreWorkingDirectory(repo, previousBookmark, previousNode);
  if (!closed) {
    setLastError(error.error, error.errorCode, error.errorMessage);
  }
  return closed;
}

bool renameBranch(Repository *repo, const QString &oldName,
                  const QString &newName) {
  if (oldName.startsWith('@')) {
    return runSimple(repo, HgError::BranchFailed,
                     {QStringLiteral("bookmark"), QStringLiteral("--rename"),
                      oldName.mid(1), displayName(newName)});
  }

  for (const HgBranchInfo &info :
       listLocalBranchesWithKind(repo, false, false)) {
    if (info.kind == HgBranchKind::NamedBranch && info.name == oldName &&
        info.isNew) {
      return runSimple(repo, HgError::BranchFailed,
                       {QStringLiteral("branch"), newName});
    }
  }

  setLastError(HgError::BranchFailed, -1,
               QStringLiteral("Mercurial can't rename the named branch %1 "
                              "because it has commits.")
                   .arg(oldName));
  return false;
}

bool deleteBranch(Repository *repo, const QString &branchName) {
  if (branchName.startsWith('@')) {
    return runSimple(repo, HgError::BranchFailed,
                     {QStringLiteral("bookmark"), QStringLiteral("--delete"),
                      branchName.mid(1)});
  }
  return closeNamedBranch(repo, branchName, QString(), QString());
}

QStringList getCommitBranches(Repository *repo, const QString &commitOid,
                              const bool &quitFlag) {
  const QuitScope quitScope(&quitFlag);
  const QString descendants =
      QStringLiteral("descendants(%1)").arg(revsetString(commitOid));

  QStringList names = templateLines(
      repo, descendants + QStringLiteral(" and head() and not closed()"),
      QStringLiteral("{branch}\n"));
  for (const QString &bookmark :
       templateLines(repo, descendants + QStringLiteral(" and bookmark()"),
                     QStringLiteral("{join(bookmarks, '\\n')}\n"))) {
    names << '@' + bookmark;
  }
  return names;
}

QStringList getCommitTags(Repository *repo, const QString &commitOid,
                          const bool &quitFlag) {
  const QuitScope quitScope(&quitFlag);
  QStringList tags = templateLines(repo, revsetString(commitOid),
                                   QStringLiteral("{join(tags, '\\n')}\n"));
  tags.removeAll(QStringLiteral("tip"));
  return tags;
}

QStringList getCommitHeads(Repository *repo, const QString &commitOid,
                           const bool &quitFlag) {
  const QuitScope quitScope(&quitFlag);
  const QString commit = revsetString(commitOid);

  QStringList names = templateLines(
      repo, commit + QStringLiteral(" and head() and not closed()"),
      QStringLiteral("{branch}\n"));
  for (const QString &bookmark : templateLines(
           repo, commit, QStringLiteral("{join(bookmarks, '\\n')}\n"))) {
    names << '@' + bookmark;
  }
  return names;
}

bool getCommitIsHead(Repository *repo, const QString &commitOid,
                     const bool &quitFlag) {
  return !getCommitHeads(repo, commitOid, quitFlag).isEmpty();
}

QStringList listTags(Repository *repo) {
  QStringList names;
  for (const TagInfo &tag : readTags(repo)) {
    names << tag.name;
  }
  return names;
}

QStringList listLocalTags(Repository *repo) {
  QStringList names;
  for (const TagInfo &tag : readTags(repo)) {
    if (tag.local) {
      names << tag.name;
    }
  }
  return names;
}

bool isLocalTag(Repository *repo, const QString &tagName) {
  TagInfo tag;
  return findTag(repo, tagName, &tag) && tag.local;
}

bool createTagFromCommit(Repository *repo, const QString &commit,
                         const QString &tagName, const QString &author,
                         const QString &email, const QString &message,
                         bool local) {
  if (tagName.isEmpty()) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("A tag name is required."));
    return false;
  }

  for (const TagInfo &tag : readTags(repo)) {
    if (tag.name == tagName) {
      setLastError(HgError::TagFailed, -1,
                   QStringLiteral("Tag %1 already exists.").arg(tagName));
      return false;
    }
  }

  QStringList args{QStringLiteral("tag"), QStringLiteral("--rev"),
                   commit.isEmpty() ? QStringLiteral(".") : commit};
  if (local) {
    args << QStringLiteral("--local");
  } else {
    // A global tag is a commit; --force allows it when the working directory
    // is not at a branch head. Replacing tags was ruled out above.
    args << QStringLiteral("--force") << userArgs(repo, author, email);
    if (!message.isEmpty()) {
      args << QStringLiteral("--message") << message;
    }
  }
  args << tagName;

  return runSimple(repo, HgError::TagFailed, args);
}

bool deleteTag(Repository *repo, const QString &tagName) {
  TagInfo tag;
  if (!findTag(repo, tagName, &tag)) {
    return false;
  }

  QStringList args{QStringLiteral("tag"), QStringLiteral("--remove")};
  if (tag.local) {
    args << QStringLiteral("--local");
  } else {
    args << QStringLiteral("--force") << userArgs(repo, QString(), QString());
  }
  args << tagName;
  return runSimple(repo, HgError::TagFailed, args);
}

bool renameTag(Repository *repo, const QString &oldName,
               const QString &newName) {
  TagInfo tag;
  if (!findTag(repo, oldName, &tag)) {
    return false;
  }

  return createTagFromCommit(
             repo, tag.node, newName, QString(), QString(),
             tag.local
                 ? QString()
                 : QStringLiteral("Rename tag %1 to %2").arg(oldName, newName),
             tag.local) &&
         deleteTag(repo, oldName);
}

bool pushTag(Repository *repo, const QString &tagName) {
  TagInfo tag;
  if (!findTag(repo, tagName, &tag)) {
    return false;
  }
  if (tag.local) {
    setLastError(HgError::TagFailed, -1,
                 QStringLiteral("%1 is a local tag, local tags are never "
                                "pushed.")
                     .arg(tagName));
    return false;
  }

  QString node;
  return findTagCommit(repo, tag, &node) && pushRevisions(repo, node);
}

bool pushAllTags(Repository *repo) {
  const int count =
      countRevisions(repo, QStringLiteral("file('path:.hgtags')"));
  if (count < 0) {
    return false;
  }
  if (count == 0) {
    return true;
  }
  return pushRevisions(repo, QStringLiteral("file('path:.hgtags')"));
}

bool deleteRemoteTag(Repository *repo, const QString &tagName) {
  TagInfo tag;
  if (!findTag(repo, tagName, &tag)) {
    return false;
  }
  if (tag.local) {
    setLastError(HgError::TagFailed, -1,
                 QStringLiteral("%1 is a local tag, it is not on the remote.")
                     .arg(tagName));
    return false;
  }

  // The removal is a commit on the working directory parent, pushed with its
  // ancestors.
  return deleteTag(repo, tagName) && pushRevisions(repo, QStringLiteral("."));
}

bool merge(Repository *repo, const QString &sourceBranch,
           const QString &destinationBranch, const QString &author,
           const QString &email) {
  LOG_PERF(0, "Enter");

  if (!requireCleanWorkingDirectory(repo)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  QString source;
  QString destination;
  if (!resolveNode(repo, HgError::MergeFailed,
                   branchRevset(sourceBranch, GitRepo::BranchType::Local),
                   &source) ||
      !resolveNode(repo, HgError::MergeFailed,
                   branchRevset(destinationBranch, GitRepo::BranchType::Local),
                   &destination) ||
      !checkoutBranch(repo, destinationBranch, GitRepo::BranchType::Local)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  QString ancestor;
  if (!resolveNode(repo, HgError::MergeFailed,
                   QStringLiteral("ancestor(%1, %2)").arg(source, destination),
                   &ancestor)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  if (ancestor == source) {
    // Already merged.
    LOG_PERF(0, "Exit");
    return true;
  }

  if (ancestor == destination) {
    if (destinationBranch.startsWith('@')) {
      // Fast-forward: the bookmark moves to the source.
      const bool moved =
          runSimple(repo, HgError::MergeFailed,
                    {QStringLiteral("bookmark"), QStringLiteral("--force"),
                     QStringLiteral("--rev"), source,
                     destinationBranch.mid(1)}) &&
          updateTo(repo, destinationBranch.mid(1));
      LOG_PERF(0, "Exit");
      return moved;
    }

    const QStringList sourceBranches =
        templateLines(repo, source, QStringLiteral("{branch}"));
    if (sourceBranches.value(0) == destinationBranch) {
      // Fast-forward within the named branch.
      const bool updated = updateTo(repo, source);
      LOG_PERF(0, "Exit");
      return updated;
    }
  }

  HgCommandResult result;
  if (!runCommand(repo, HgError::MergeFailed,
                  {QStringLiteral("merge"), QStringLiteral("--rev"), source},
                  &result, RunOptions(), true)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  if (result.exitCode == 1) {
    setLastError(
        HgError::MergeConflicts, 1,
        QStringLiteral("Merging %1 into %2 has conflicts. Resolve "
                       "and stage them, then commit, or abort the "
                       "merge.")
            .arg(displayName(sourceBranch), displayName(destinationBranch)));
    LOG_PERF(0, "Exit");
    return false;
  }

  QStringList args{
      QStringLiteral("commit"), QStringLiteral("--message"),
      QStringLiteral("Merge branch '%1' into '%2'")
          .arg(displayName(sourceBranch), displayName(destinationBranch))};
  args << userArgs(repo, author, email);
  const bool committed = runSimple(repo, HgError::MergeFailed, args);
  if (committed) {
    writeStagedPaths(repo, {});
  }

  LOG_PERF(0, "Exit");
  return committed;
}

bool abortMerge(Repository *repo) {
  bool aborted = false;
  if (hasState(repo, "graftstate")) {
    aborted = runSimple(repo, HgError::MergeFailed,
                        {QStringLiteral("graft"), QStringLiteral("--abort")});
  } else if (hasState(repo, "shelvedstate")) {
    aborted =
        runSimple(repo, HgError::MergeFailed,
                  {QStringLiteral("unshelve"), QStringLiteral("--abort")});
  } else if (countRevisions(repo, QStringLiteral("p2()")) > 0) {
    aborted = runSimple(repo, HgError::MergeFailed,
                        {QStringLiteral("merge"), QStringLiteral("--abort")});
  } else {
    // A backout with conflicts leaves no state, only conflicted files.
    aborted = runSimple(repo, HgError::MergeFailed,
                        {QStringLiteral("update"), QStringLiteral("--clean"),
                         QStringLiteral("--rev"), QStringLiteral(".")});
  }

  if (aborted) {
    writeStagedPaths(repo, {});
  }
  return aborted;
}

bool rebaseBranches(Repository *repo, const QString &sourceBranch,
                    const QString &destinationBranch, const QString &author,
                    const QString &email, bool signCommit) {
  Q_UNUSED(author);
  Q_UNUSED(email);
  LOG_PERF(0, "Enter");

  if (signCommit) {
    return setUnsupported("Signing commits");
  }

  if (!requireCleanWorkingDirectory(repo)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  // --keepbranches: the rebased commits stay on the source's named branch,
  // like the Git branch does.
  HgCommandResult result;
  if (!runCommand(repo, HgError::RebaseFailed,
                  {QStringLiteral("rebase"), QStringLiteral("--base"),
                   branchRevset(sourceBranch, GitRepo::BranchType::Local),
                   QStringLiteral("--dest"),
                   branchRevset(destinationBranch, GitRepo::BranchType::Local),
                   QStringLiteral("--keepbranches")},
                  &result, RunOptions(), true)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  if (result.exitCode == 1 && getIsRebaseInProgress(repo)) {
    setLastError(
        HgError::MergeConflicts, 1,
        QStringLiteral("Rebasing %1 onto %2 has conflicts. Resolve "
                       "and stage them, then continue, or abort the "
                       "rebase.")
            .arg(displayName(sourceBranch), displayName(destinationBranch)));
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool updated =
      checkoutBranch(repo, sourceBranch, GitRepo::BranchType::Local);
  LOG_PERF(0, "Exit");
  return updated;
}

bool getIsRebaseInProgress(Repository *repo) {
  return repo && hasState(repo, "rebasestate");
}

bool abortRebase(Repository *repo) {
  const bool aborted =
      runSimple(repo, HgError::RebaseFailed,
                {QStringLiteral("rebase"), QStringLiteral("--abort")});
  if (aborted) {
    writeStagedPaths(repo, {});
  }
  return aborted;
}

bool commitRebase(Repository *repo, const QString &summary,
                  const QString &message, const QString &author,
                  const QString &email, bool cleanup, bool signoff,
                  bool signCommit) {
  Q_UNUSED(summary);
  Q_UNUSED(message);
  Q_UNUSED(author);
  Q_UNUSED(email);
  Q_UNUSED(cleanup);
  Q_UNUSED(signoff);

  if (signCommit) {
    return setUnsupported("Signing commits");
  }

  if (hasConflicts(repo)) {
    setLastError(HgError::MergeConflicts, -1,
                 QStringLiteral("Resolve and stage every conflicted file "
                                "first."));
    return false;
  }

  HgCommandResult result;
  if (!runCommand(repo, HgError::RebaseFailed,
                  {QStringLiteral("rebase"), QStringLiteral("--continue")},
                  &result, RunOptions(), true)) {
    return false;
  }

  if (result.exitCode == 1 && getIsRebaseInProgress(repo)) {
    setLastError(HgError::MergeConflicts, 1,
                 QStringLiteral("The next commit of the rebase has conflicts. "
                                "Resolve and stage them, then continue."));
    return false;
  }

  writeStagedPaths(repo, {});
  return true;
}

bool squashBranches(Repository *repo, const QString &sourceBranch,
                    const QString &destinationBranch, const QString &author,
                    const QString &email, bool signCommit) {
  LOG_PERF(0, "Enter");

  if (signCommit) {
    return setUnsupported("Signing commits");
  }

  if (sourceBranch.isEmpty() || destinationBranch.isEmpty() ||
      sourceBranch == destinationBranch) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("Two different branches are required."));
    return false;
  }

  if (!requireCleanWorkingDirectory(repo)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  QString source;
  QString destination;
  if (!resolveNode(repo, HgError::MergeFailed,
                   branchRevset(sourceBranch, GitRepo::BranchType::Local),
                   &source) ||
      !resolveNode(repo, HgError::MergeFailed,
                   branchRevset(destinationBranch, GitRepo::BranchType::Local),
                   &destination)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString onlySource =
      QStringLiteral("only(%1, %2)").arg(source, destination);
  const QStringList summaries =
      templateLines(repo, QStringLiteral("reverse(%1)").arg(onlySource),
                    QStringLiteral("* {desc|firstline}\n"));

  if (summaries.isEmpty()) {
    const bool updated =
        checkoutBranch(repo, destinationBranch, GitRepo::BranchType::Local);
    LOG_PERF(0, "Exit");
    return updated;
  }

  const QString squashMessage =
      QStringLiteral("Squash merge branch '%1' into '%2'\n\n%3")
          .arg(displayName(sourceBranch), displayName(destinationBranch),
               summaries.join('\n'));

  // A collapsed rebase that keeps the originals is a squash merge. Without
  // --keepbranches the commit is on the destination's branch.
  const QString user = author.isEmpty() ? userString(repo->author, repo->email)
                                        : userString(author, email);
  QStringList args;
  if (!user.isEmpty()) {
    args << QStringLiteral("--config") << QStringLiteral("ui.username=") + user;
  }
  args << QStringLiteral("rebase") << QStringLiteral("--collapse")
       << QStringLiteral("--keep") << QStringLiteral("--rev") << onlySource
       << QStringLiteral("--dest") << destination << QStringLiteral("--message")
       << squashMessage;

  HgCommandResult result;
  if (!runCommand(repo, HgError::MergeFailed, args, &result, RunOptions(),
                  true)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  if (result.exitCode == 1 && getIsRebaseInProgress(repo)) {
    setLastError(
        HgError::MergeConflicts, 1,
        QStringLiteral("Squashing %1 into %2 has conflicts.")
            .arg(displayName(sourceBranch), displayName(destinationBranch)));
    LOG_PERF(0, "Exit");
    return false;
  }

  QString squashed;
  if (!resolveNode(repo, HgError::MergeFailed,
                   QStringLiteral("max(children(%1))").arg(destination),
                   &squashed)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  bool updated = false;
  if (destinationBranch.startsWith('@')) {
    updated = runSimple(repo, HgError::MergeFailed,
                        {QStringLiteral("bookmark"), QStringLiteral("--force"),
                         QStringLiteral("--rev"), squashed,
                         destinationBranch.mid(1)}) &&
              updateTo(repo, destinationBranch.mid(1));
  } else {
    updated = updateTo(repo, squashed);
  }

  LOG_PERF(0, "Exit");
  return updated;
}

bool cherryPickCommit(Repository *repo, const QString &commit,
                      const QString &author, const QString &email) {
  QStringList args{QStringLiteral("graft"), QStringLiteral("--rev"), commit};
  args << userArgs(repo, author, email);

  HgCommandResult result;
  if (!runCommand(repo, HgError::GraftFailed, args, &result, RunOptions(),
                  true)) {
    return false;
  }
  // Exit code 1: conflicts, the graft waits for them to be resolved.
  return result.exitCode == 0 || hasState(repo, "graftstate");
}

bool revertCommit(Repository *repo, const QString &commit,
                  const QString &author, const QString &email,
                  const QString &message) {
  QStringList args{QStringLiteral("backout"), QStringLiteral("--rev"), commit};
  args << userArgs(repo, author, email);
  if (!message.isEmpty()) {
    args << QStringLiteral("--message") << message;
  }

  HgCommandResult result;
  if (!runCommand(repo, HgError::BackoutFailed, args, &result, RunOptions(),
                  true)) {
    return false;
  }
  // Exit code 1: conflicts left to resolve and commit.
  return true;
}

bool strip(Repository *repo, const QString &commit, bool keepWorkingCopy,
           bool backup) {
  if (commit.isEmpty()) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("A commit is required."));
    return false;
  }

  QStringList args{QStringLiteral("strip"), QStringLiteral("--rev"), commit};
  if (keepWorkingCopy) {
    args << QStringLiteral("--keep");
  }
  if (!backup) {
    args << QStringLiteral("--no-backup");
  }

  const bool stripped = runSimple(repo, HgError::StripFailed, args);
  if (stripped) {
    repo->historyCursor = HistoryCursor();
  }
  return stripped;
}

bool resetToCommit(Repository *repo, const QString &commit,
                   GitRepo::ResetType type) {
  QString target;
  if (!resolveNode(repo, HgError::StripFailed, revsetString(commit), &target)) {
    return false;
  }

  // The commits after the target on the line of the working directory.
  const QString descendants = QStringLiteral("(%1::.) - %1").arg(target);
  const int count = countRevisions(repo, descendants);
  if (count < 0) {
    return false;
  }

  const bool keep = type != GitRepo::ResetType::Hard;
  if (count == 0) {
    // Already at the commit, or the commit is not an ancestor.
    if (QString parent = workingDirectoryNode(repo); parent == target) {
      return keep ? true
                  : runSimple(repo, HgError::StripFailed,
                              {QStringLiteral("update"),
                               QStringLiteral("--clean"),
                               QStringLiteral("--rev"), target});
    }
    setLastError(HgError::StripFailed, -1,
                 QStringLiteral("%1 is not an ancestor of the working "
                                "directory parent.")
                     .arg(commit));
    return false;
  }

  QStringList args{QStringLiteral("strip"), QStringLiteral("--rev"),
                   descendants};
  if (keep) {
    args << QStringLiteral("--keep");
  } else {
    args << QStringLiteral("--force");
  }

  const bool stripped = runSimple(repo, HgError::StripFailed, args);
  if (stripped) {
    repo->historyCursor = HistoryCursor();
  }
  return stripped;
}

} // namespace HgRepo
