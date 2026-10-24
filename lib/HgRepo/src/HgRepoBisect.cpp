// Bisect, with Mercurial's own bisect state.

#include "HgRepo.h"
#include "HgRepoPrivate.h"

#include <Log.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>

#define THREAD_ID "HgRepo"

namespace HgRepo {

using namespace Private;

namespace {

QString bisectStateFile(const Repository *repo) {
  return hgFile(repo, QStringLiteral("bisect.state"));
}

// Where the session started: "@bookmark" or a node, restored by Reset.
QString bisectOriginFile(const Repository *repo) {
  return hgFile(repo, QStringLiteral("sourcerer/bisect-origin"));
}

bool bisectInProgress(const Repository *repo) {
  const QFileInfo state(bisectStateFile(repo));
  return state.exists() && state.size() > 0;
}

QStringList nodesOf(Repository *repo, const QString &revset) {
  HgCommandResult result;
  RunOptions options;
  options.readOnly = true;
  if (!runCommand(repo, HgError::BisectFailed,
                  {QStringLiteral("log"), QStringLiteral("--rev"), revset,
                   QStringLiteral("--template"), QStringLiteral("{node}\n")},
                  &result, options)) {
    return {};
  }
  return QString::fromLatin1(result.out).split('\n', Qt::SkipEmptyParts);
}

bool commitWithSummary(Repository *repo, const QString &revision, QString *node,
                       QString *summary) {
  HgCommandResult result;
  RunOptions options;
  options.readOnly = true;
  if (!runCommand(repo, HgError::BisectFailed,
                  {QStringLiteral("log"), QStringLiteral("--rev"), revision,
                   QStringLiteral("--template"),
                   QStringLiteral("{node}\\n{desc|firstline}")},
                  &result, options)) {
    return false;
  }
  const QString output = QString::fromUtf8(result.out);
  *node = output.section('\n', 0, 0).trimmed();
  *summary = output.section('\n', 1).trimmed();
  return !node->isEmpty();
}

} // namespace

bool bisect(Repository *repo, GitRepo::BisectAction action,
            const QString &commit, GitRepo::BisectStatus &status) {
  status = GitRepo::BisectStatus();

  if (!repo) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("The repository is not open."));
    return false;
  }

  if (action == GitRepo::BisectAction::Reset) {
    if (!bisectInProgress(repo)) {
      setLastError(HgError::BisectNotInProgress, -1,
                   QStringLiteral("No bisect is in progress."));
      return false;
    }

    if (!runSimple(repo, HgError::BisectFailed,
                   {QStringLiteral("bisect"), QStringLiteral("--reset")})) {
      return false;
    }

    QFile originFile(bisectOriginFile(repo));
    QString origin;
    if (originFile.open(QIODevice::ReadOnly)) {
      origin = QString::fromUtf8(originFile.readAll()).trimmed();
      originFile.close();
    }
    originFile.remove();

    if (origin.isEmpty()) {
      return true;
    }
    return runSimple(repo, HgError::BisectFailed,
                     {QStringLiteral("update"), QStringLiteral("--rev"),
                      origin.startsWith('@') ? origin.mid(1) : origin});
  }

  if (!bisectInProgress(repo)) {
    if (getIsMergeInProgress(repo) || getIsRebaseInProgress(repo)) {
      setLastError(HgError::BisectFailed, -1,
                   QStringLiteral("Finish or abort the operation in progress "
                                  "before bisecting."));
      return false;
    }

    const QString bookmark = activeBookmark(repo);
    const QString origin =
        bookmark.isEmpty() ? workingDirectoryNode(repo) : '@' + bookmark;
    QDir().mkpath(QFileInfo(bisectOriginFile(repo)).absolutePath());
    QFile originFile(bisectOriginFile(repo));
    if (!originFile.open(QIODevice::WriteOnly) ||
        originFile.write(origin.toUtf8()) < 0) {
      setLastError(
          HgError::BisectFailed, -1,
          QStringLiteral("Failed to write %1").arg(originFile.fileName()));
      return false;
    }
  }

  QStringList args{QStringLiteral("bisect")};
  switch (action) {
  case GitRepo::BisectAction::Good:
    args << QStringLiteral("--good");
    break;
  case GitRepo::BisectAction::Bad:
    args << QStringLiteral("--bad");
    break;
  case GitRepo::BisectAction::Skip:
    args << QStringLiteral("--skip");
    break;
  default:
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("Unknown bisect action."));
    return false;
  }
  if (!commit.isEmpty()) {
    args << commit;
  }

  HgCommandResult result;
  if (!runCommand(repo, HgError::BisectFailed, args, &result, RunOptions(),
                  true)) {
    return false;
  }

  status.active = true;
  status.waitingForGood =
      nodesOf(repo, QStringLiteral("bisect(good)")).isEmpty();
  status.waitingForBad = nodesOf(repo, QStringLiteral("bisect(bad)")).isEmpty();
  if (status.waitingForGood || status.waitingForBad) {
    return true;
  }

  static const QRegularExpression testing(QStringLiteral(
      "Testing changeset (\\d+):[0-9a-f]+ .*\\((\\d+) changesets remaining, "
      "~(\\d+) tests?\\)"));
  static const QRegularExpression changeset(
      QStringLiteral("^changeset:\\s+(\\d+):([0-9a-f]+)"));

  const QStringList lines =
      QString::fromUtf8(result.out).remove('\r').split('\n');
  bool firstBadFollows = false;
  for (const QString &line : lines) {
    const QRegularExpressionMatch testingMatch = testing.match(line);
    if (testingMatch.hasMatch()) {
      commitWithSummary(repo, testingMatch.captured(1), &status.currentCommit,
                        &status.currentSummary);
      status.remaining = testingMatch.captured(2).toInt();
      status.steps = testingMatch.captured(3).toInt();
      continue;
    }

    if (line.startsWith(QStringLiteral("The first bad revision is:"))) {
      status.finished = true;
      firstBadFollows = true;
      continue;
    }

    if (line.startsWith(
            QStringLiteral("Due to skipped revisions, the first bad"))) {
      status.finished = true;
      status.ambiguous = true;
      firstBadFollows = true;
      continue;
    }

    const QRegularExpressionMatch changesetMatch = changeset.match(line);
    if (firstBadFollows && changesetMatch.hasMatch()) {
      commitWithSummary(repo, changesetMatch.captured(1),
                        &status.firstBadCommit, &status.firstBadSummary);
      firstBadFollows = false;
    }
  }

  return true;
}

QHash<QString, int> getBisectMarks(Repository *repo) {
  QHash<QString, int> marks;
  if (!repo || !bisectInProgress(repo)) {
    return marks;
  }

  for (const QString &node : nodesOf(repo, QStringLiteral("bisect(good)"))) {
    marks.insert(node, static_cast<int>(GitRepo::BisectMark::Good));
  }
  for (const QString &node : nodesOf(repo, QStringLiteral("bisect(skip)"))) {
    marks.insert(node, static_cast<int>(GitRepo::BisectMark::Skip));
  }
  bool hasBad = false;
  for (const QString &node : nodesOf(repo, QStringLiteral("bisect(bad)"))) {
    marks.insert(node, static_cast<int>(GitRepo::BisectMark::Bad));
    hasBad = true;
  }

  const bool hasGood = std::any_of(marks.cbegin(), marks.cend(), [](int mark) {
    return mark == static_cast<int>(GitRepo::BisectMark::Good);
  });
  if (hasGood && hasBad) {
    for (const QString &node :
         nodesOf(repo, QStringLiteral("bisect(current)"))) {
      if (!marks.contains(node)) {
        marks.insert(node, static_cast<int>(GitRepo::BisectMark::Current));
      }
    }
  }
  return marks;
}

} // namespace HgRepo
