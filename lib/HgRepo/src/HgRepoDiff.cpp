// MIT License
// Copyright (c) 2026 Sourcerer

// Diffs, blame and file contents.

#include "HgRepo.h"
#include "HgRepoPrivate.h"

#include <ConfigManager.h>
#include <Log.h>

#include <QFileInfo>
#include <QHash>
#include <QJsonObject>
#include <QRegularExpression>

#define THREAD_ID "HgRepo"

namespace HgRepo {

using namespace Private;

namespace {

// Content of a working directory file, empty when it is larger than
// GitRepoMaxDiffFileSize.
QString workingContent(const Repository *repo, const QString &relativePath) {
  const QFileInfo fileInfo(repo->root + '/' + relativePath);
  if (!fileInfo.isFile()) {
    return QString();
  }

  const qint64 limit = ConfigManager::ConfigManager::getInstance().getLong(
      ConfigManager::ConfigKey::GitRepoMaxDiffFileSize);
  if (limit > 0 && fileInfo.size() > limit) {
    return QString();
  }

  return QString::fromUtf8(readWorkingFile(repo, relativePath));
}

bool runDiff(Repository *repo, const QStringList &extraArgs,
             const QStringList &paths,
             QVector<GitRepo::FilesDiffDelta> *deltas) {
  deltas->clear();
  if (paths.isEmpty()) {
    return true;
  }

  QStringList args{QStringLiteral("diff"), QStringLiteral("--git")};
  args << extraArgs << QStringLiteral("--") << pathPatterns(paths);

  HgCommandResult result;
  RunOptions options;
  options.readOnly = true;
  if (!runCommand(repo, HgError::DiffFailed, args, &result, options)) {
    return false;
  }

  *deltas = parseGitDiff(QString::fromUtf8(result.out));
  return true;
}

QString lineContent(QString line) {
  while (line.endsWith('\n') || line.endsWith('\r')) {
    line.chop(1);
  }
  return line;
}

} // namespace

namespace Private {

QVector<GitRepo::FilesDiffDelta> parseGitDiff(const QString &diff) {
  static const QRegularExpression hunkHeader(
      QStringLiteral("^@@ -(\\d+)(?:,(\\d+))? \\+(\\d+)(?:,(\\d+))? @@"));

  // "diff --git a/<old> b/<new>". Paths may contain " b/", so prefer the
  // split that gives the same path on both sides.
  const auto headerPaths = [](const QString &line, QString *oldPath,
                              QString *newPath) {
    const QString rest = line.mid(QStringLiteral("diff --git ").size());
    int split = -1;
    for (int at = rest.indexOf(QStringLiteral(" b/")); at >= 0;
         at = rest.indexOf(QStringLiteral(" b/"), at + 1)) {
      if (split < 0) {
        split = at;
      }
      if (rest.left(at).mid(2) == rest.mid(at + 3)) {
        split = at;
        break;
      }
    }
    if (split < 0 || !rest.startsWith(QStringLiteral("a/"))) {
      *oldPath = rest;
      *newPath = rest;
      return;
    }
    *oldPath = rest.left(split).mid(2);
    *newPath = rest.mid(split + 3);
  };

  QVector<GitRepo::FilesDiffDelta> deltas;
  GitRepo::FilesDiffDelta *delta = nullptr;
  GitRepo::FilesDiffHunk *hunk = nullptr;
  int oldLinesLeft = 0;
  int newLinesLeft = 0;

  const QStringList lines = QString(diff).remove('\r').split('\n');
  for (const QString &line : lines) {
    if (hunk && (oldLinesLeft > 0 || newLinesLeft > 0)) {
      const QChar origin = line.isEmpty() ? QChar(' ') : line.at(0);
      if (origin == ' ' || origin == '-' || origin == '+') {
        hunk->lines.append(GitRepo::FilesDiffLine{origin, line.mid(1)});
        if (origin != '+') {
          --oldLinesLeft;
        }
        if (origin != '-') {
          --newLinesLeft;
        }
        continue;
      }
    }

    if (line.startsWith(QStringLiteral("diff --git "))) {
      deltas.append(GitRepo::FilesDiffDelta());
      delta = &deltas.last();
      headerPaths(line, &delta->oldFilePath, &delta->newFilePath);
      hunk = nullptr;
      continue;
    }

    if (!delta) {
      continue;
    }

    if (!hunk) {
      if (line.startsWith(QStringLiteral("rename from "))) {
        delta->oldFilePath = line.mid(12);
        continue;
      }
      if (line.startsWith(QStringLiteral("rename to "))) {
        delta->newFilePath = line.mid(10);
        continue;
      }
      if (line.startsWith(QStringLiteral("--- a/"))) {
        delta->oldFilePath = line.mid(6);
        continue;
      }
      if (line.startsWith(QStringLiteral("+++ b/"))) {
        delta->newFilePath = line.mid(6);
        continue;
      }
    }

    const QRegularExpressionMatch match = hunkHeader.match(line);
    if (match.hasMatch()) {
      delta->hunks.append(GitRepo::FilesDiffHunk());
      hunk = &delta->hunks.last();
      hunk->oldStart = match.captured(1).toInt();
      hunk->oldLines =
          match.captured(2).isEmpty() ? 1 : match.captured(2).toInt();
      hunk->newStart = match.captured(3).toInt();
      hunk->newLines =
          match.captured(4).isEmpty() ? 1 : match.captured(4).toInt();
      oldLinesLeft = hunk->oldLines;
      newLinesLeft = hunk->newLines;
      continue;
    }

    // "\ No newline at end of file" belongs to the previous line.
  }

  return deltas;
}

QString diffText(const QVector<GitRepo::FilesDiffDelta> &deltas) {
  QString text;
  for (const GitRepo::FilesDiffDelta &delta : deltas) {
    text += QStringLiteral("diff --git a/%1 b/%2\n")
                .arg(delta.oldFilePath, delta.newFilePath);
    for (const GitRepo::FilesDiffHunk &hunk : delta.hunks) {
      text += QStringLiteral("@@ -%1,%2 +%3,%4 @@\n")
                  .arg(hunk.oldStart)
                  .arg(hunk.oldLines)
                  .arg(hunk.newStart)
                  .arg(hunk.newLines);
      for (const GitRepo::FilesDiffLine &line : hunk.lines) {
        text += line.origin + line.content + QStringLiteral("\n");
      }
    }
  }
  return text;
}

GitRepo::FilesDiffDelta addedFileDelta(const QString &file,
                                       const QString &content) {
  GitRepo::FilesDiffDelta delta;
  delta.oldFilePath = file;
  delta.newFilePath = file;

  QStringList lines = QString(content).remove('\r').split('\n');
  if (!lines.isEmpty() && lines.last().isEmpty()) {
    lines.removeLast();
  }

  if (lines.isEmpty()) {
    return delta;
  }

  GitRepo::FilesDiffHunk hunk;
  hunk.oldStart = 0;
  hunk.oldLines = 0;
  hunk.newStart = 1;
  hunk.newLines = static_cast<int>(lines.size());
  for (const QString &line : lines) {
    hunk.lines.append(GitRepo::FilesDiffLine{QChar('+'), line});
  }
  delta.hunks.append(hunk);
  return delta;
}

} // namespace Private

GitRepo::FilesDiff diffFiles(Repository *repo, const QStringList &files) {
  LOG_PERF(0, "Enter");

  if (!repo) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("The repository is not open."));
    return {};
  }

  GitRepo::FilesDiff result;

  QStringList paths;
  for (const QString &file : files) {
    paths << relativeFilePath(repo->root, file);
  }

  QVector<StatusEntry> entries;
  if (!readStatusEntries(repo, {QStringLiteral("-mardu")}, pathPatterns(paths),
                         &entries, HgError::DiffFailed)) {
    LOG_PERF(0, "Exit");
    return {};
  }

  QHash<QString, StatusEntry> entryOf;
  QStringList tracked;
  for (const StatusEntry &entry : entries) {
    entryOf.insert(entry.path, entry);
    if (entry.status != '?') {
      tracked << entry.path;
    }
  }

  QVector<GitRepo::FilesDiffDelta> trackedDeltas;
  if (!runDiff(repo, {}, tracked, &trackedDeltas)) {
    LOG_PERF(0, "Exit");
    return {};
  }

  for (int i = 0; i < files.size(); ++i) {
    const QString &file = files.at(i);
    const QString &path = paths.at(i);
    const QString content = workingContent(repo, path);
    result.filesContent.insert(file, content);

    const auto entry = entryOf.constFind(path);
    if (entry != entryOf.constEnd() && entry->status == '?') {
      result.repoContent.insert(file, QString());
      result.deltas.append(addedFileDelta(path, content));
      continue;
    }

    // Deltas in the order of files.
    for (const GitRepo::FilesDiffDelta &delta : trackedDeltas) {
      if (delta.newFilePath == path) {
        result.deltas.append(delta);
      }
    }

    if (entry != entryOf.constEnd() && entry->status == 'A' &&
        entry->source.isEmpty()) {
      result.repoContent.insert(file, QString());
    } else {
      // The source of a rename holds the previous content.
      const QString previousPath =
          entry != entryOf.constEnd() && !entry->source.isEmpty()
              ? entry->source
              : path;
      result.repoContent.insert(
          file, contentWithinLimit(readFileAtRevision(repo, previousPath,
                                                      QStringLiteral("."))));
    }
  }

  result.diff = diffText(result.deltas);
  result.valid = true;

  LOG_PERF(0, "Exit");
  return result;
}

GitRepo::FilesDiff diffFilesFromCommit(Repository *repo,
                                       const QStringList &files,
                                       const QString &commit) {
  LOG_PERF(0, "Enter");

  if (!repo) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("The repository is not open."));
    return {};
  }

  GitRepo::FilesDiff result;

  QStringList paths;
  for (const QString &file : files) {
    paths << relativeFilePath(repo->root, file);
  }

  if (!runDiff(repo, {QStringLiteral("--change"), commit}, paths,
               &result.deltas)) {
    LOG_PERF(0, "Exit");
    return {};
  }

  const QString parent = QStringLiteral("p1(%1)").arg(revsetString(commit));
  for (int i = 0; i < files.size(); ++i) {
    result.repoContent.insert(
        files.at(i),
        contentWithinLimit(readFileAtRevision(repo, paths.at(i), parent)));
    result.filesContent.insert(
        files.at(i),
        contentWithinLimit(readFileAtRevision(repo, paths.at(i), commit)));
  }

  result.diff = diffText(result.deltas);
  result.valid = true;

  LOG_PERF(0, "Exit");
  return result;
}

GitRepo::BlameReport blame(Repository *repo, const QStringList &files) {
  LOG_PERF(0, "Enter");

  if (!repo) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("The repository is not open."));
    return {};
  }

  GitRepo::BlameReport report;
  QSet<QString> nodes;

  RunOptions options;
  options.readOnly = true;

  for (const QString &file : files) {
    const QString path = relativeFilePath(repo->root, file);

    GitRepo::BlameFileInfo fileInfo;
    fileInfo.filePath = path;

    QJsonArray array;
    if (!runJson(repo, HgError::BlameFailed,
                 {QStringLiteral("annotate"), QStringLiteral("--rev"),
                  QStringLiteral("wdir()"), QStringLiteral("--user"),
                  QStringLiteral("--changeset"),
                  QStringLiteral("--line-number"), QStringLiteral("--"),
                  pathPattern(path)},
                 &array, options)) {
      fileInfo.errorMessage = getLastError().errorMessage;
      report.fileInfos.append(fileInfo);
      continue;
    }

    // hg exits with 1 and annotates nothing for files it doesn't track.
    if (array.isEmpty()) {
      fileInfo.errorMessage =
          QStringLiteral("%1 is not tracked by Mercurial.").arg(path);
      report.fileInfos.append(fileInfo);
      continue;
    }

    for (const QJsonValue &fileValue : array) {
      for (const QJsonValue &lineValue :
           fileValue.toObject().value("lines").toArray()) {
        const QJsonObject line = lineValue.toObject();

        GitRepo::BlameLineInfo lineInfo;
        lineInfo.filePath = path;
        lineInfo.lineNumber = line.value("lineno").toInteger();
        lineInfo.content = lineContent(line.value("line").toString());
        const QString node = line.value("node").toString();
        if (node == QLatin1String(kWorkingDirectoryNode)) {
          lineInfo.commit = QString::fromLatin1(kNullNode);
        } else {
          lineInfo.commit = node;
          nodes.insert(node);
        }
        splitUser(line.value("user").toString(), &lineInfo.author,
                  &lineInfo.email);
        fileInfo.lineInfos.append(lineInfo);
      }
    }

    report.fileInfos.append(fileInfo);
  }

  // Summaries of the commits, with a single command.
  if (!nodes.isEmpty()) {
    const QStringList nodeList(nodes.cbegin(), nodes.cend());
    QJsonArray array;
    if (runJson(repo, HgError::BlameFailed,
                {QStringLiteral("log"), QStringLiteral("--rev"),
                 nodeList.join('+')},
                &array, options)) {
      QHash<QString, QString> summaries;
      for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();
        summaries.insert(object.value("node").toString(),
                         summaryOf(object.value("desc").toString()));
      }
      for (GitRepo::BlameFileInfo &fileInfo : report.fileInfos) {
        for (GitRepo::BlameLineInfo &lineInfo : fileInfo.lineInfos) {
          lineInfo.summary = summaries.value(lineInfo.commit);
        }
      }
    }
  }

  report.valid = true;

  LOG_PERF(0, "Exit");
  return report;
}

QString createPatchFromCommit(Repository *repo, const QString &commit) {
  HgCommandResult result;
  RunOptions options;
  options.readOnly = true;
  if (!runCommand(repo, HgError::PatchFailed,
                  {QStringLiteral("export"), QStringLiteral("--git"),
                   QStringLiteral("--rev"), commit},
                  &result, options)) {
    return QString();
  }
  return QString::fromUtf8(result.out);
}

bool applyPatch(Repository *repo, const QString &patchContent) {
  if (patchContent.isEmpty()) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("The patch is empty."));
    return false;
  }

  // The patch is read from the command's standard input.
  HgCommandResult result;
  RunOptions options;
  options.input = patchContent.toUtf8();
  return runCommand(repo, HgError::PatchFailed,
                    {QStringLiteral("import"), QStringLiteral("--no-commit"),
                     QStringLiteral("-")},
                    &result, options);
}

qint64 getFileSizeFromCommit(Repository *repo, const QString &filePath,
                             const QString &commit) {
  if (!repo) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("The repository is not open."));
    return -1;
  }

  HgCommandResult result;
  RunOptions options;
  options.readOnly = true;
  if (!runCommand(repo, HgError::FileSizeFailed,
                  {QStringLiteral("files"), QStringLiteral("--rev"),
                   commit.isEmpty() ? QStringLiteral(".") : commit,
                   QStringLiteral("--template"), QStringLiteral("{size}"),
                   QStringLiteral("--"),
                   pathPattern(relativeFilePath(repo->root, filePath))},
                  &result, options)) {
    return -1;
  }

  bool ok = false;
  const qint64 size = QString::fromLatin1(result.out).trimmed().toLongLong(&ok);
  if (!ok) {
    setLastError(
        HgError::FileSizeFailed, -1,
        QStringLiteral("%1 does not exist at %2")
            .arg(filePath, commit.isEmpty() ? QStringLiteral(".") : commit));
    return -1;
  }
  return size;
}

} // namespace HgRepo
