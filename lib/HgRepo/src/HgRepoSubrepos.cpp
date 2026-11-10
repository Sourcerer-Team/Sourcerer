// MIT License
// Copyright (c) 2026 Sourcerer

// Subrepos: .hgsub, .hgsubstate and the state of their checkouts.

#include "HgRepo.h"
#include "HgRepoPrivate.h"

#include <Log.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>

#define THREAD_ID "HgRepo"

namespace HgRepo {

using namespace Private;

namespace {

const QString kHgSub = QStringLiteral(".hgsub");
const QString kHgSubState = QStringLiteral(".hgsubstate");

QStringList readLines(const QString &fileName) {
  QFile file(fileName);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  QStringList lines =
      QString::fromUtf8(file.readAll()).remove('\r').split('\n');
  if (!lines.isEmpty() && lines.last().isEmpty()) {
    lines.removeLast();
  }
  return lines;
}

bool writeLines(const QString &fileName, const QStringList &lines) {
  QSaveFile file(fileName);
  QString content;
  for (const QString &line : lines) {
    content += line + '\n';
  }
  if (!file.open(QIODevice::WriteOnly) || file.write(content.toUtf8()) < 0 ||
      !file.commit()) {
    setLastError(HgError::SubrepoFailed, -1,
                 QStringLiteral("Failed to write %1: %2")
                     .arg(fileName, file.errorString()));
    return false;
  }
  return true;
}

// "path = source" lines before the first section of .hgsub ([subpaths]
// rewrites sources and is left alone).
struct SubLine {
  int index = -1;
  QString path;
  QString source;
};

QVector<SubLine> parseHgSub(const QStringList &lines) {
  static const QRegularExpression item(
      QStringLiteral("^\\s*([^=#;][^=]*?)\\s*=\\s*(.*?)\\s*$"));

  QVector<SubLine> entries;
  for (int i = 0; i < lines.size(); ++i) {
    const QString trimmed = lines.at(i).trimmed();
    if (trimmed.startsWith('[')) {
      break;
    }
    const QRegularExpressionMatch match = item.match(lines.at(i));
    if (match.hasMatch()) {
      entries.append(SubLine{i, QDir::fromNativeSeparators(match.captured(1)),
                             match.captured(2)});
    }
  }
  return entries;
}

void splitSource(const QString &value, QString *source, HgSubrepoKind *kind) {
  static const QRegularExpression prefix(QStringLiteral("^\\[(\\w+)\\](.*)$"));
  const QRegularExpressionMatch match = prefix.match(value);
  *kind = HgSubrepoKind::Mercurial;
  *source = value;
  if (match.hasMatch()) {
    const QString name = match.captured(1).toLower();
    *source = match.captured(2);
    if (name == QStringLiteral("git")) {
      *kind = HgSubrepoKind::Git;
    } else if (name == QStringLiteral("svn")) {
      *kind = HgSubrepoKind::Subversion;
    }
  }
}

QString kindPrefix(HgSubrepoKind kind) {
  switch (kind) {
  case HgSubrepoKind::Git:
    return QStringLiteral("[git]");
  case HgSubrepoKind::Subversion:
    return QStringLiteral("[svn]");
  default:
    return QString();
  }
}

// First parent in the subrepo's dirstate, without running hg: dirstate-v1
// starts with the two 20 byte parents, the dirstate-v2 docket with its magic
// and two 32 byte parent slots.
QString mercurialCheckout(const QString &path) {
  QFile dirstate(path + QStringLiteral("/.hg/dirstate"));
  if (!dirstate.open(QIODevice::ReadOnly)) {
    return QString();
  }
  const QByteArray head = dirstate.read(64);
  static const QByteArray v2Magic("dirstate-v2\n");
  const QByteArray parent =
      head.startsWith(v2Magic) ? head.mid(v2Magic.size(), 20) : head.left(20);
  if (parent.size() != 20 || parent == QByteArray(20, '\0')) {
    return QString();
  }
  return QString::fromLatin1(parent.toHex());
}

// HEAD of a Git checkout, without libgit2.
QString gitCheckout(const QString &path) {
  QString gitDir = path + QStringLiteral("/.git");
  if (QFileInfo(gitDir).isFile()) {
    // "gitdir: <path>" of a linked worktree or an absorbed submodule.
    const QString line = readLines(gitDir).value(0);
    if (!line.startsWith(QStringLiteral("gitdir:"))) {
      return QString();
    }
    gitDir = QDir(path).absoluteFilePath(line.mid(7).trimmed());
  }

  const QString head = readLines(gitDir + QStringLiteral("/HEAD")).value(0);
  if (!head.startsWith(QStringLiteral("ref:"))) {
    return head.trimmed();
  }

  const QString ref = head.mid(4).trimmed();
  const QString loose = readLines(gitDir + '/' + ref).value(0).trimmed();
  if (!loose.isEmpty()) {
    return loose;
  }
  for (const QString &line :
       readLines(gitDir + QStringLiteral("/packed-refs"))) {
    if (line.endsWith(' ' + ref)) {
      return line.section(' ', 0, 0);
    }
  }
  return QString();
}

QString checkoutState(const QString &path, HgSubrepoKind kind) {
  switch (kind) {
  case HgSubrepoKind::Mercurial:
    return mercurialCheckout(path);
  case HgSubrepoKind::Git:
    return gitCheckout(path);
  default:
    return QString();
  }
}

bool findSubrepo(Repository *repo, const QString &name, HgSubrepo *subrepo) {
  const QString path = relativeFilePath(repo->root, name);
  for (const HgSubrepo &candidate : listSubrepos(repo)) {
    if (candidate.path == path) {
      *subrepo = candidate;
      return true;
    }
  }
  setLastError(HgError::SubrepoFailed, -1,
               QStringLiteral("%1 is not a subrepo.").arg(name));
  return false;
}

// Adds paths to the staging area, with "hg add" for the files hg doesn't
// track yet.
bool stagePaths(Repository *repo, const QStringList &paths) {
  return stageFiles(repo, paths);
}

} // namespace

namespace Private {

QStringList subrepoPaths(Repository *repo) {
  QStringList paths;
  for (const SubLine &line : parseHgSub(readLines(repo->root + '/' + kHgSub))) {
    paths << line.path;
  }
  return paths;
}

QVector<StatusEntry> subrepoStatusEntries(Repository *repo) {
  QVector<StatusEntry> entries;
  if (!QFile::exists(repo->root + '/' + kHgSub)) {
    return entries;
  }

  for (const HgSubrepo &subrepo : listSubrepos(repo)) {
    if (subrepo.currentState.isEmpty() ||
        subrepo.currentState == subrepo.pinnedState) {
      continue;
    }
    StatusEntry entry;
    entry.path = subrepo.path;
    entry.status = subrepo.pinnedState.isEmpty() ? QChar('A') : QChar('M');
    entries.append(entry);
  }
  return entries;
}

} // namespace Private

QVector<HgSubrepo> listSubrepos(Repository *repo) {
  if (!repo) {
    return {};
  }

  QHash<QString, QString> pinned;
  for (const QString &line : readLines(repo->root + '/' + kHgSubState)) {
    const int space = line.indexOf(' ');
    if (space > 0) {
      pinned.insert(QDir::fromNativeSeparators(line.mid(space + 1).trimmed()),
                    line.left(space));
    }
  }

  QVector<HgSubrepo> subrepos;
  for (const SubLine &line : parseHgSub(readLines(repo->root + '/' + kHgSub))) {
    HgSubrepo subrepo;
    subrepo.path = line.path;
    splitSource(line.source, &subrepo.source, &subrepo.kind);
    subrepo.pinnedState = pinned.value(line.path);
    subrepo.currentState =
        checkoutState(repo->root + '/' + line.path, subrepo.kind);
    subrepos.append(subrepo);
  }
  return subrepos;
}

QStringList listSubmodules(Repository *repo) {
  return repo ? subrepoPaths(repo) : QStringList();
}

QVector<GitRepo::SubmoduleInfo> listSubmodulesWithStatus(Repository *repo,
                                                         const bool &quitFlag) {
  QVector<GitRepo::SubmoduleInfo> infos;
  for (const HgSubrepo &subrepo : listSubrepos(repo)) {
    if (quitFlag) {
      return {};
    }

    GitRepo::SubmoduleInfo info;
    info.name = subrepo.path;
    info.commit = subrepo.pinnedState;

    if (!QFileInfo(repo->root + '/' + subrepo.path).isDir()) {
      info.statusInfo.status = GitRepo::SubmoduleStatusType::Error;
    } else if (subrepo.kind == HgSubrepoKind::Subversion) {
      info.statusInfo.status = GitRepo::SubmoduleStatusType::Unknown;
    } else if (subrepo.currentState.isEmpty()) {
      info.statusInfo.status = GitRepo::SubmoduleStatusType::Error;
    } else if (subrepo.currentState == subrepo.pinnedState) {
      info.statusInfo.status = GitRepo::SubmoduleStatusType::UpToDate;
    } else {
      info.statusInfo.status = GitRepo::SubmoduleStatusType::Outdated;
    }
    infos.append(info);
  }
  return infos;
}

QString getSubmodulePath(Repository *repo, const QString &submoduleName) {
  HgSubrepo subrepo;
  if (!repo || !findSubrepo(repo, submoduleName, &subrepo)) {
    return QString();
  }
  return repo->root + '/' + subrepo.path;
}

bool addSubmodule(Repository *repo, const QString &url, const QString &name,
                  const QString &branch, const QString &uuid) {
  Q_UNUSED(uuid);

  if (!repo || url.isEmpty() || name.isEmpty()) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("A URL and a path are required."));
    return false;
  }

  const QString path = relativeFilePath(repo->root, name);
  const QString destination = repo->root + '/' + path;
  if (QFileInfo::exists(destination) &&
      !QDir(destination)
           .isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden)) {
    setLastError(HgError::SubrepoFailed, -1,
                 QStringLiteral("%1 already exists.").arg(path));
    return false;
  }

  QStringList args{QStringLiteral("clone")};
  if (!branch.isEmpty()) {
    args << QStringLiteral("--updaterev") << branch;
  }
  args << url << QDir::toNativeSeparators(destination);

  HgCommandResult result;
  if (!runOneShot(HgError::SubrepoFailed, args, repo->root, &result)) {
    return false;
  }

  return registerSubrepo(repo, path, url, HgSubrepoKind::Mercurial);
}

bool registerSubrepo(Repository *repo, const QString &path,
                     const QString &source, HgSubrepoKind kind) {
  if (!repo || path.isEmpty() || source.isEmpty()) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("A path and a source are required."));
    return false;
  }

  const QString relativePath = relativeFilePath(repo->root, path);
  const QString fileName = repo->root + '/' + kHgSub;
  QStringList lines = readLines(fileName);

  const QVector<SubLine> entries = parseHgSub(lines);
  for (const SubLine &entry : entries) {
    if (entry.path == relativePath) {
      setLastError(
          HgError::SubrepoFailed, -1,
          QStringLiteral("%1 is already a subrepo.").arg(relativePath));
      return false;
    }
  }

  // After the last entry, before any section.
  const int insertAt = entries.isEmpty() ? 0 : entries.last().index + 1;
  lines.insert(
      insertAt,
      QStringLiteral("%1 = %2%3").arg(relativePath, kindPrefix(kind), source));
  if (!writeLines(fileName, lines)) {
    return false;
  }

  return stagePaths(repo, {kHgSub, relativePath});
}

bool deleteSubmodule(Repository *repo, const QString &submoduleName,
                     const QString &submodulePath) {
  HgSubrepo subrepo;
  if (!repo || !findSubrepo(repo, submoduleName, &subrepo)) {
    return false;
  }

  const QString fileName = repo->root + '/' + kHgSub;
  QStringList lines = readLines(fileName);
  QVector<SubLine> entries = parseHgSub(lines);
  for (int i = entries.size() - 1; i >= 0; --i) {
    if (entries[i].path == subrepo.path) {
      lines.removeAt(entries[i].index);
    }
  }
  if (!writeLines(fileName, lines)) {
    return false;
  }

  const QString directory =
      submodulePath.isEmpty() ? repo->root + '/' + subrepo.path : submodulePath;
  if (QFileInfo(directory).isDir() && !QDir(directory).removeRecursively()) {
    setLastError(HgError::SubrepoFailed, -1,
                 QStringLiteral("Failed to delete %1.").arg(directory));
    return false;
  }

  if (!parseHgSub(lines).isEmpty()) {
    return stagePaths(repo, {kHgSub});
  }

  // Without subrepos, .hgsub and .hgsubstate go away.
  QFile::remove(fileName);
  QStringList toStage{kHgSub};
  if (QFile::exists(repo->root + '/' + kHgSubState)) {
    QFile::remove(repo->root + '/' + kHgSubState);
    toStage << kHgSubState;
  }
  return stagePaths(repo, toStage);
}

bool updateSubmodule(Repository *repo, const QString &submoduleName) {
  HgSubrepo subrepo;
  if (!repo || !findSubrepo(repo, submoduleName, &subrepo)) {
    return false;
  }

  if (subrepo.kind != HgSubrepoKind::Mercurial) {
    return setUnsupported("Updating Git and Subversion subrepos in HgRepo");
  }
  if (subrepo.pinnedState.isEmpty()) {
    setLastError(HgError::SubrepoFailed, -1,
                 QStringLiteral("%1 has no pinned revision yet, commit it "
                                "first.")
                     .arg(subrepo.path));
    return false;
  }

  const QString directory = repo->root + '/' + subrepo.path;
  HgCommandResult result;
  if (!QFileInfo(directory + QStringLiteral("/.hg")).isDir()) {
    // Missing checkout: clone it like "hg update" would.
    const QString source =
        QFileInfo(subrepo.source).isRelative() &&
                !subrepo.source.contains(QStringLiteral("://"))
            ? QDir(repo->root).absoluteFilePath(subrepo.source)
            : subrepo.source;
    if (!runOneShot(HgError::SubrepoFailed,
                    {QStringLiteral("clone"), QStringLiteral("--noupdate"),
                     source, QDir::toNativeSeparators(directory)},
                    repo->root, &result)) {
      return false;
    }
  } else {
    // The pinned node may not have been pulled into the subrepo yet; exit
    // code 1 means nothing new.
    runOneShot(HgError::SubrepoFailed, {QStringLiteral("pull")}, directory,
               &result);
  }

  return runOneShot(
      HgError::SubrepoFailed,
      {QStringLiteral("update"), QStringLiteral("--rev"), subrepo.pinnedState},
      directory, &result);
}

} // namespace HgRepo
