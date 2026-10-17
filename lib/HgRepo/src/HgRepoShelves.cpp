// Shelves, Mercurial's stashes, and conflict contents.

#include "HgRepo.h"
#include "HgRepoPrivate.h"

#include <Log.h>

#include <QDateTime>
#include <QFileInfo>
#include <QLocale>
#include <QRegularExpression>

#define THREAD_ID "HgRepo"

namespace HgRepo {

using namespace Private;

namespace {

QStringList shelveListLines(Repository *repo, bool quiet) {
  QStringList args{QStringLiteral("shelve"), QStringLiteral("--list")};
  if (quiet) {
    args << QStringLiteral("--quiet");
  }

  HgCommandResult result;
  RunOptions options;
  options.readOnly = true;
  if (!runCommand(repo, HgError::ShelveFailed, args, &result, options)) {
    return {};
  }
  return QString::fromUtf8(result.out)
      .remove('\r')
      .split('\n', Qt::SkipEmptyParts);
}

bool shelveName(Repository *repo, int index, QString *name) {
  const QStringList names = listStashNames(repo);
  if (index < 0 || index >= names.size()) {
    setLastError(HgError::ShelveFailed, -1,
                 QStringLiteral("There is no shelve %1.").arg(index));
    return false;
  }
  *name = names.at(index);
  return true;
}

} // namespace

bool stash(Repository *repo, const QString &note) {
  QStringList args{QStringLiteral("shelve"), QStringLiteral("--unknown")};
  if (!note.isEmpty()) {
    args << QStringLiteral("--message") << note;
  }

  HgCommandResult result;
  if (!runCommand(repo, HgError::ShelveFailed, args, &result, RunOptions(),
                  true)) {
    return false;
  }
  if (result.exitCode == 1) {
    setLastError(HgError::ShelveFailed, 1,
                 QStringLiteral("There are no changes to shelve."));
    return false;
  }

  writeStagedPaths(repo, {});
  return true;
}

QStringList listStashNames(Repository *repo) {
  return shelveListLines(repo, true);
}

QStringList listStashes(Repository *repo, const bool &quitFlag) {
  const QuitScope quitScope(&quitFlag);

  const QStringList names = listStashNames(repo);
  const QStringList lines = shelveListLines(repo, false);

  // "<name, padded>  (<age>)  <message>": the names are known, so names with
  // spaces are no problem.
  static const QRegularExpression rest(
      QStringLiteral("^\\s+\\([^)]*\\)\\s*(.*)$"));

  QStringList messages;
  for (int i = 0; i < names.size(); ++i) {
    const QString &name = names.at(i);
    QString message = name;
    const QString line = lines.value(i);
    if (line.startsWith(name)) {
      const QRegularExpressionMatch match = rest.match(line.mid(name.size()));
      if (match.hasMatch() && !match.captured(1).trimmed().isEmpty()) {
        message = match.captured(1).trimmed();
      }
    }
    messages << message;
  }
  return messages;
}

bool applyStash(Repository *repo, int index) {
  QString name;
  if (!shelveName(repo, index, &name)) {
    return false;
  }

  HgCommandResult result;
  if (!runCommand(repo, HgError::ShelveFailed,
                  {QStringLiteral("unshelve"), QStringLiteral("--keep"),
                   QStringLiteral("--name"), name},
                  &result, RunOptions(), true)) {
    return false;
  }

  if (result.exitCode == 1) {
    setLastError(HgError::MergeConflicts, 1,
                 QStringLiteral("Restoring the shelve %1 has conflicts. "
                                "Resolve and stage them, or abort.")
                     .arg(name));
    return false;
  }
  return true;
}

bool deleteStash(Repository *repo, int index) {
  QString name;
  return shelveName(repo, index, &name) &&
         runSimple(
             repo, HgError::ShelveFailed,
             {QStringLiteral("shelve"), QStringLiteral("--delete"), name});
}

bool deleteAllStashes(Repository *repo) {
  return runSimple(repo, HgError::ShelveFailed,
                   {QStringLiteral("shelve"), QStringLiteral("--cleanup")});
}

GitRepo::GitCommitStatus getStashStatus(Repository *repo, int index) {
  QString name;
  if (!shelveName(repo, index, &name)) {
    return {};
  }

  HgCommandResult result;
  RunOptions options;
  options.readOnly = true;
  if (!runCommand(repo, HgError::ShelveFailed,
                  {QStringLiteral("shelve"), QStringLiteral("--patch"), name},
                  &result, options)) {
    return {};
  }

  GitRepo::GitCommitStatus status;
  status.commit = name;
  status.summary = listStashes(repo, false).value(index, name);
  status.authorName = repo->author;
  status.authorEmail = repo->email;

  const QFileInfo shelveFile(
      hgFile(repo, QStringLiteral("shelved/%1.patch").arg(name)));
  if (shelveFile.exists()) {
    status.committerDate = QLocale::system().toString(shelveFile.lastModified(),
                                                      QLocale::ShortFormat);
  }

  // Each file starts with "diff --git"; the following header lines tell
  // whether it was added, removed or renamed.
  enum class Kind { Modified, New, Deleted, Renamed };
  QString path;
  Kind kind = Kind::Modified;
  const auto flush = [&]() {
    if (path.isEmpty()) {
      return;
    }
    switch (kind) {
    case Kind::New:
      status.newFiles += path;
      break;
    case Kind::Deleted:
      status.deletedFiles += path;
      break;
    case Kind::Renamed:
      status.renamedFiles += path;
      break;
    case Kind::Modified:
      status.modifiedFiles += path;
      break;
    }
    path.clear();
  };

  const QStringList lines =
      QString::fromUtf8(result.out).remove('\r').split('\n');
  for (const QString &line : lines) {
    if (line.startsWith(QStringLiteral("diff --git "))) {
      flush();
      const QVector<GitRepo::FilesDiffDelta> deltas = parseGitDiff(line);
      path = deltas.isEmpty() ? QString() : deltas.first().newFilePath;
      kind = Kind::Modified;
    } else if (line.startsWith(QStringLiteral("new file mode"))) {
      kind = Kind::New;
    } else if (line.startsWith(QStringLiteral("deleted file mode"))) {
      kind = Kind::Deleted;
    } else if (line.startsWith(QStringLiteral("rename to "))) {
      kind = Kind::Renamed;
      path = line.mid(10);
    }
  }
  flush();

  return status;
}

QString listConflictsForAFile(Repository *repo, const QString &filePath) {
  QSet<QString> unresolved;
  if (!readUnresolvedPaths(repo, &unresolved)) {
    return QString();
  }

  const QString path = relativeFilePath(repo->root, filePath);
  if (!unresolved.contains(path)) {
    return QString();
  }

  // The merge left the conflict markers in the working file.
  return QString::fromUtf8(readWorkingFile(repo, path));
}

QString listConflictsForStashFile(Repository *repo, int index,
                                  const QString &filePath) {
  Q_UNUSED(index);
  return QString::fromUtf8(
      readWorkingFile(repo, relativeFilePath(repo->root, filePath)));
}

} // namespace HgRepo
