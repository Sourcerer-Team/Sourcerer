// End to end tests of SvnRepo against a local file:// repository.

#include <SvnRepo.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

namespace {

const QString kAuthor = QStringLiteral("Tester");
const QString kEmail = QStringLiteral("tester@example.com");

bool writeFile(const QString &path, const QByteArray &content) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write(content) >= 0;
}

QByteArray readFile(const QString &path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QString lastErrorMessage() {
  return SvnRepo::getLastError().errorMessage;
}

} // namespace

class SvnRepoTest : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();

  void initCreatesStandardLayout();
  void stageUnstageAndCommit();
  void unstagingAModificationFails();
  void diffAndDiscard();
  void historyAndCommitQueries();
  void blameAttributesLines();
  void branchesSwitchAndMerge();
  void tags();
  void cherryPickAndRevert();
  void patches();
  void conflictedUpdateAndAbort();
  void stashes();
  void externals();
  void remotes();
  void gitOnlyOperationsAreUnsupported();

private:
  QString file(const QString &name) const { return _wc + "/" + name; }
  QString commitAll(const QString &summary);
  QString headRevision();

  QTemporaryDir _tmp;
  QString _wc;
  QString _rootUrl;
  bool _quit = false;
};

QString SvnRepoTest::commitAll(const QString &summary) {
  const SvnRepo::SvnStatus status = SvnRepo::getStatus(_wc, _quit);
  if (!status.newFiles.isEmpty()) {
    SvnRepo::stageFiles(_wc, status.newFiles);
  }
  if (!SvnRepo::commitStaged(_wc, summary, QString(), kAuthor, kEmail, true,
                             false, false)) {
    return QString();
  }
  SvnRepo::pull(_wc, QString(), false);
  return headRevision();
}

QString SvnRepoTest::headRevision() {
  const SvnRepo::SvnHistory history = SvnRepo::getHistory(_wc, 1);
  return history.commits.isEmpty() ? QString() : history.commits.first();
}

void SvnRepoTest::initTestCase() {
  QVERIFY(_tmp.isValid());
  QVERIFY(SvnRepo::init());

  _wc = _tmp.path() + "/wc";
  QVERIFY2(SvnRepo::initRepository(QStringLiteral("repo"), _wc,
                                   QStringLiteral("main"), kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QVERIFY(!SvnRepo::openRepository(_wc).isEmpty());

  const QVector<SvnRepo::RemoteInfo> remotes = SvnRepo::listRemotes(_wc);
  QCOMPARE(remotes.size(), 1);
  _rootUrl = remotes.first().fetchUrl;
  QVERIFY(_rootUrl.startsWith(QStringLiteral("file://")));
}

void SvnRepoTest::initCreatesStandardLayout() {
  QCOMPARE(SvnRepo::getActiveBranch(_wc, SvnRepo::BranchType::Local, _quit),
           QStringLiteral("trunk"));
  QCOMPARE(SvnRepo::listLocalBranches(_wc, _quit),
           QStringList{QStringLiteral("trunk")});
  QVERIFY(SvnRepo::listRemoteBranches(_wc, _quit).isEmpty());
  QVERIFY(SvnRepo::listTags(_wc).isEmpty());
  QVERIFY(!SvnRepo::getStatus(_wc, _quit).hasChanges());
}

void SvnRepoTest::stageUnstageAndCommit() {
  QVERIFY(writeFile(file("a.txt"), "one\ntwo\nthree\n"));

  SvnRepo::SvnStatus status = SvnRepo::getStatus(_wc, _quit);
  QCOMPARE(status.newFiles, QStringList{QStringLiteral("a.txt")});

  QVERIFY2(SvnRepo::stageFiles(_wc, {QStringLiteral("a.txt")}),
           qPrintable(lastErrorMessage()));
  status = SvnRepo::getStatus(_wc, _quit);
  QCOMPARE(status.newStagedFiles, QStringList{QStringLiteral("a.txt")});
  QVERIFY(status.newFiles.isEmpty());

  QVERIFY(SvnRepo::unstageFiles(_wc, {QStringLiteral("a.txt")}));
  status = SvnRepo::getStatus(_wc, _quit);
  QCOMPARE(status.newFiles, QStringList{QStringLiteral("a.txt")});
  QVERIFY(QFileInfo::exists(file("a.txt")));

  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("a.txt")}));
  QVERIFY2(SvnRepo::commitStaged(_wc, QStringLiteral("Add a"),
                                 QStringLiteral("Body"), kAuthor, kEmail, true,
                                 true, false),
           qPrintable(lastErrorMessage()));
  QVERIFY(!SvnRepo::getStatus(_wc, _quit).hasChanges());

  const SvnRepo::CommitStatus commit =
      SvnRepo::getCommitStatus(_wc, headRevision());
  QVERIFY(commit.message.startsWith(QStringLiteral("Add a\n\nBody")));
  QVERIFY(commit.message.contains(
      QStringLiteral("Signed-off-by: Tester <tester@example.com>")));
}

void SvnRepoTest::unstagingAModificationFails() {
  QVERIFY(writeFile(file("a.txt"), "one\nTWO\nthree\n"));
  QCOMPARE(SvnRepo::getStatus(_wc, _quit).modifiedStagedFiles,
           QStringList{QStringLiteral("a.txt")});

  QVERIFY(!SvnRepo::unstageFiles(_wc, {QStringLiteral("a.txt")}));
  QVERIFY(lastErrorMessage().contains(QStringLiteral("no staging area")));

  QVERIFY(SvnRepo::discardFiles(_wc, {QStringLiteral("a.txt")}));
}

void SvnRepoTest::diffAndDiscard() {
  QVERIFY(writeFile(file("a.txt"), "one\nTWO\nthree\n"));
  QVERIFY(writeFile(file("new.txt"), "fresh\n"));

  const SvnRepo::FilesDiff diff = SvnRepo::diffFiles(
      _wc, {QStringLiteral("a.txt"), QStringLiteral("new.txt")});
  QVERIFY2(diff.valid, qPrintable(lastErrorMessage()));
  QCOMPARE(diff.deltas.size(), 2);

  const SvnRepo::FilesDiffDelta &modified = diff.deltas[0];
  QCOMPARE(modified.newFilePath, QStringLiteral("a.txt"));
  QCOMPARE(modified.hunks.size(), 1);
  QStringList changed;
  for (const SvnRepo::FilesDiffLine &line : modified.hunks[0].lines) {
    if (line.origin != ' ') {
      changed.append(line.origin + line.content);
    }
  }
  QCOMPARE(changed,
           (QStringList{QStringLiteral("-two"), QStringLiteral("+TWO")}));
  QCOMPARE(diff.repoContent.value(QStringLiteral("a.txt")),
           QStringLiteral("one\ntwo\nthree\n"));
  QCOMPARE(diff.filesContent.value(QStringLiteral("a.txt")),
           QStringLiteral("one\nTWO\nthree\n"));

  const SvnRepo::FilesDiffDelta &added = diff.deltas[1];
  QCOMPARE(added.hunks.size(), 1);
  QCOMPARE(added.hunks[0].lines.size(), 1);
  QCOMPARE(added.hunks[0].lines[0].origin, QChar('+'));
  QVERIFY(diff.diff.contains(QStringLiteral("diff --git a/a.txt b/a.txt")));

  QVERIFY(SvnRepo::discardFiles(
      _wc, {QStringLiteral("a.txt"), QStringLiteral("new.txt")}));
  QVERIFY(!SvnRepo::getStatus(_wc, _quit).hasChanges());
  QVERIFY(!QFileInfo::exists(file("new.txt")));
  QCOMPARE(readFile(file("a.txt")), QByteArray("one\ntwo\nthree\n"));
}

void SvnRepoTest::historyAndCommitQueries() {
  QVERIFY(writeFile(file("a.txt"), "one\ntwo\nthree\nfour\n"));
  const QString revision = commitAll(QStringLiteral("Add four"));
  QVERIFY2(!revision.isEmpty(), qPrintable(lastErrorMessage()));

  const SvnRepo::SvnHistory history = SvnRepo::getBranchHistory(
      _wc, QStringLiteral("trunk"), SvnRepo::BranchType::Local, -1, _quit);
  QVERIFY(history.valid);
  QVERIFY(!history.hasMore);
  QCOMPARE(history.commits.first(), revision);
  // Newest first, each revision's parent is the next one.
  for (int i = 0; i + 1 < history.commits.size(); ++i) {
    QVERIFY(history.commits[i].toInt() > history.commits[i + 1].toInt());
    QCOMPARE(history.commitInfos[history.commits[i]].parents,
             QVector<QString>{history.commits[i + 1]});
  }
  QCOMPARE(history.commitInfos[revision].summary, QStringLiteral("Add four"));
  QCOMPARE(history.commitInfos[revision].branches,
           QVector<QString>{QStringLiteral("trunk")});

  const SvnRepo::SvnHistory firstPage = SvnRepo::getBranchHistory(
      _wc, QStringLiteral("trunk"), SvnRepo::BranchType::Local, 0, _quit);
  QCOMPARE(firstPage.commits, history.commits);
  QVERIFY(SvnRepo::getHistory(_wc, 1).hasMore);

  QCOMPARE(SvnRepo::getCommitBranches(_wc, revision, _quit),
           QStringList{QStringLiteral("trunk")});
  QVERIFY(SvnRepo::getCommitIsHead(_wc, revision, _quit));
  QVERIFY(!SvnRepo::getCommitIsHead(_wc, history.commits.last(), _quit));
  QCOMPARE(SvnRepo::getCommitHeads(_wc, revision, _quit),
           QStringList{QStringLiteral("trunk")});
  QCOMPARE(
      SvnRepo::getFileSizeFromCommit(_wc, QStringLiteral("a.txt"), revision),
      qint64(19));

  const SvnRepo::FilesDiff commitDiff =
      SvnRepo::diffFilesFromCommit(_wc, {QStringLiteral("a.txt")}, revision);
  QVERIFY(commitDiff.valid);
  QCOMPARE(commitDiff.repoContent.value(QStringLiteral("a.txt")),
           QStringLiteral("one\ntwo\nthree\n"));
  QCOMPARE(commitDiff.filesContent.value(QStringLiteral("a.txt")),
           QStringLiteral("one\ntwo\nthree\nfour\n"));

  QVERIFY(SvnRepo::getCommitStatus(_wc, QStringLiteral("not-a-revision"))
              .commit.isEmpty());
  QCOMPARE(SvnRepo::getLastError().error, SvnRepo::SvnError::InvalidRevision);
}

void SvnRepoTest::blameAttributesLines() {
  const SvnRepo::BlameReport report =
      SvnRepo::blame(_wc, {QStringLiteral("a.txt")});
  QVERIFY(report.valid);
  QCOMPARE(report.fileInfos.size(), 1);
  const QVector<SvnRepo::BlameLineInfo> &lines =
      report.fileInfos.first().lineInfos;
  QCOMPARE(lines.size(), 4);
  QCOMPARE(lines[0].lineNumber, qint64(1));
  QCOMPARE(lines[3].content, QStringLiteral("four"));
  QCOMPARE(lines[3].summary, QStringLiteral("Add four"));
  QVERIFY(lines[0].commit.toInt() < lines[3].commit.toInt());
}

void SvnRepoTest::branchesSwitchAndMerge() {
  QVERIFY2(SvnRepo::createBranchFromCommit(_wc, headRevision(),
                                           QStringLiteral("feature")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(SvnRepo::listLocalBranches(_wc, _quit),
           (QStringList{QStringLiteral("trunk"), QStringLiteral("feature")}));
  QCOMPARE(SvnRepo::listRemoteBranches(_wc, _quit),
           QStringList{QStringLiteral("feature")});

  QVERIFY2(SvnRepo::checkoutBranch(_wc, QStringLiteral("feature"),
                                   SvnRepo::BranchType::Local),
           qPrintable(lastErrorMessage()));
  QCOMPARE(SvnRepo::getActiveBranch(_wc, SvnRepo::BranchType::Local, _quit),
           QStringLiteral("feature"));

  QVERIFY(writeFile(file("feature.txt"), "feature\n"));
  QVERIFY(!commitAll(QStringLiteral("Feature work")).isEmpty());

  QVERIFY(!SvnRepo::deleteBranch(_wc, QStringLiteral("feature")));

  QVERIFY2(SvnRepo::merge(_wc, QStringLiteral("feature"),
                          QStringLiteral("trunk"), kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QCOMPARE(SvnRepo::getActiveBranch(_wc, SvnRepo::BranchType::Local, _quit),
           QStringLiteral("trunk"));
  QVERIFY(QFileInfo::exists(file("feature.txt")));
  QVERIFY(!SvnRepo::getStatus(_wc, _quit).hasChanges());
  QVERIFY(SvnRepo::getCommitStatus(_wc, headRevision())
              .message.startsWith(QStringLiteral("Merge branch 'feature'")));

  QVERIFY2(SvnRepo::renameBranch(_wc, QStringLiteral("feature"),
                                 QStringLiteral("feature2")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(SvnRepo::listRemoteBranches(_wc, _quit),
           QStringList{QStringLiteral("feature2")});
  QVERIFY(SvnRepo::deleteBranch(_wc, QStringLiteral("feature2")));
  QVERIFY(SvnRepo::listRemoteBranches(_wc, _quit).isEmpty());
}

void SvnRepoTest::tags() {
  const QString revision = headRevision();
  QVERIFY2(SvnRepo::createTagFromCommit(_wc, revision, QStringLiteral("v1"),
                                        kAuthor, kEmail, QString()),
           qPrintable(lastErrorMessage()));
  QCOMPARE(SvnRepo::listTags(_wc), QStringList{QStringLiteral("v1")});
  QCOMPARE(SvnRepo::getCommitTags(_wc, revision, _quit),
           QStringList{QStringLiteral("v1")});

  QVERIFY(SvnRepo::renameTag(_wc, QStringLiteral("v1"), QStringLiteral("v2")));
  QCOMPARE(SvnRepo::listTags(_wc), QStringList{QStringLiteral("v2")});

  QVERIFY(SvnRepo::checkoutBranch(_wc, QStringLiteral("v2"),
                                  SvnRepo::BranchType::Tag));
  QCOMPARE(SvnRepo::getActiveBranch(_wc, SvnRepo::BranchType::Tag, _quit),
           QStringLiteral("v2"));
  QVERIFY(SvnRepo::checkoutBranch(_wc, QStringLiteral("trunk"),
                                  SvnRepo::BranchType::Local));

  QVERIFY(SvnRepo::deleteTag(_wc, QStringLiteral("v2")));
  QVERIFY(SvnRepo::listTags(_wc).isEmpty());
}

void SvnRepoTest::cherryPickAndRevert() {
  QVERIFY(writeFile(file("a.txt"), "one\ntwo\nthree\nfour\nfive\n"));
  const QString revision = commitAll(QStringLiteral("Add five"));
  QVERIFY(!revision.isEmpty());

  QVERIFY2(SvnRepo::revertCommit(_wc, revision, kAuthor, kEmail, QString()),
           qPrintable(lastErrorMessage()));
  SvnRepo::pull(_wc, QString(), false);
  QCOMPARE(readFile(file("a.txt")), QByteArray("one\ntwo\nthree\nfour\n"));
  QVERIFY(SvnRepo::getCommitStatus(_wc, headRevision())
              .message.startsWith(QStringLiteral("Revert \"Add five\"")));

  // Cherry-picking the reverted revision brings the change back.
  QVERIFY2(SvnRepo::cherryPickCommit(_wc, revision, kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  SvnRepo::pull(_wc, QString(), false);
  QCOMPARE(readFile(file("a.txt")),
           QByteArray("one\ntwo\nthree\nfour\nfive\n"));
}

void SvnRepoTest::patches() {
  const QString patch = SvnRepo::createPatchFromCommit(_wc, headRevision());
  QVERIFY(patch.contains(QStringLiteral("diff --git a/a.txt b/a.txt")));
  QVERIFY(patch.contains(QStringLiteral("+five")));

  QVERIFY(writeFile(file("a.txt"), "one\ntwo\nthree\nfour\n"));
  QVERIFY(SvnRepo::commitStaged(_wc, QStringLiteral("Drop five"), QString(),
                                kAuthor, kEmail, true, false, false));

  QVERIFY2(SvnRepo::applyPatch(_wc, patch), qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("a.txt")),
           QByteArray("one\ntwo\nthree\nfour\nfive\n"));
  QVERIFY(SvnRepo::discardFiles(_wc, {QStringLiteral("a.txt")}));
}

void SvnRepoTest::conflictedUpdateAndAbort() {
  SvnRepo::pull(_wc, QString(), false);

  // Another working copy commits a conflicting change.
  const QString other = _tmp.path() + "/other";
  QVERIFY2(SvnRepo::cloneRepository(_rootUrl + "/trunk", other),
           qPrintable(lastErrorMessage()));
  QVERIFY(writeFile(other + "/a.txt", "ONE\ntwo\nthree\nfour\n"));
  QVERIFY(SvnRepo::commitStaged(other, QStringLiteral("Other change"),
                                QString(), kAuthor, kEmail, true, false,
                                false));

  QVERIFY(writeFile(file("a.txt"), "uno\ntwo\nthree\nfour\n"));
  QVERIFY(SvnRepo::pull(_wc, QString(), false));
  QVERIFY(SvnRepo::hasConflicts(_wc));
  QVERIFY(SvnRepo::getIsMergeInProgress(_wc));
  QCOMPARE(SvnRepo::getStatus(_wc, _quit).conflictedFiles,
           QStringList{QStringLiteral("a.txt")});
  QVERIFY(SvnRepo::listConflictsForAFile(_wc, QStringLiteral("a.txt"))
              .contains(QStringLiteral("<<<<<<<")));

  QVERIFY2(SvnRepo::abortMerge(_wc), qPrintable(lastErrorMessage()));
  QVERIFY(!SvnRepo::hasConflicts(_wc));
  QCOMPARE(readFile(file("a.txt")), QByteArray("ONE\ntwo\nthree\nfour\n"));
  QVERIFY(!SvnRepo::abortMerge(_wc));
}

void SvnRepoTest::stashes() {
  QVERIFY(writeFile(file("a.txt"), "ONE\ntwo\nthree\nfour\nstashed\n"));

  QVERIFY2(SvnRepo::stash(_wc, QStringLiteral("my note")),
           qPrintable(lastErrorMessage()));
  QVERIFY(!SvnRepo::getStatus(_wc, _quit).hasChanges());

  const QStringList stashes = SvnRepo::listStashes(_wc, _quit);
  QCOMPARE(stashes.size(), 1);
  QCOMPARE(stashes.first(), QStringLiteral("On trunk: my note"));
  QCOMPARE(SvnRepo::getStashStatus(_wc, 0).message,
           QStringLiteral("On trunk: my note"));

  QVERIFY2(SvnRepo::applyStash(_wc, 0), qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("a.txt")),
           QByteArray("ONE\ntwo\nthree\nfour\nstashed\n"));

  QVERIFY(SvnRepo::deleteStash(_wc, 0));
  QVERIFY(SvnRepo::listStashes(_wc, _quit).isEmpty());
  QVERIFY(!SvnRepo::deleteStash(_wc, 0));
  QVERIFY(SvnRepo::discardFiles(_wc, {QStringLiteral("a.txt")}));

  QVERIFY(!SvnRepo::stash(_wc, QString()));
  QVERIFY(SvnRepo::listStashes(_wc, _quit).isEmpty());
}

void SvnRepoTest::externals() {
  QVERIFY2(SvnRepo::addSubmodule(_wc, _rootUrl + "/trunk",
                                 QStringLiteral("ext"), QString(), QString()),
           qPrintable(lastErrorMessage()));
  QCOMPARE(SvnRepo::listSubmodules(_wc), QStringList{QStringLiteral("ext")});
  QVERIFY(QFileInfo::exists(file("ext/a.txt")));

  const QVector<SvnRepo::SubmoduleInfo> submodules =
      SvnRepo::listSubmodulesWithStatus(_wc, _quit);
  QCOMPARE(submodules.size(), 1);
  QCOMPARE(submodules.first().statusInfo.status,
           SvnRepo::SubmoduleStatusType::UpToDate);
  QCOMPARE(SvnRepo::getSubmodulePath(_wc, QStringLiteral("ext")), file("ext"));
  QVERIFY(SvnRepo::updateSubmodule(_wc, QStringLiteral("ext")));

  QVERIFY2(SvnRepo::deleteSubmodule(_wc, QStringLiteral("ext"), QString()),
           qPrintable(lastErrorMessage()));
  QVERIFY(SvnRepo::listSubmodules(_wc).isEmpty());
  QVERIFY(!QFileInfo::exists(file("ext")));
  QVERIFY(!SvnRepo::updateSubmodule(_wc, QStringLiteral("ext")));
}

void SvnRepoTest::remotes() {
  QCOMPARE(SvnRepo::getDefaultRemote(_wc), QStringLiteral("origin"));
  const auto [found, remote, branch] =
      SvnRepo::getUpstream(_wc, QStringLiteral("trunk"));
  QVERIFY(found);
  QCOMPARE(remote, QStringLiteral("origin"));
  QCOMPARE(branch, QStringLiteral("trunk"));

  QVERIFY(SvnRepo::fetchAll(_wc));
  QVERIFY(SvnRepo::push(_wc, QStringLiteral("trunk"), false));
  QVERIFY(SvnRepo::updateRemote(_wc, QStringLiteral("origin"), QString(),
                                _rootUrl, _rootUrl));
  QVERIFY(
      !SvnRepo::addRemote(_wc, QStringLiteral("upstream"), _rootUrl, _rootUrl));
  QCOMPARE(SvnRepo::getLastError().error,
           SvnRepo::SvnError::UnsupportedOperation);

  const QVector<SvnRepo::BranchInfo> branches =
      SvnRepo::listRemoteBranchesWithStatus(_wc, _quit);
  QVERIFY(branches.isEmpty());
}

void SvnRepoTest::gitOnlyOperationsAreUnsupported() {
  QVERIFY(!SvnRepo::rebaseBranches(
      _wc, QStringLiteral("a"), QStringLiteral("b"), kAuthor, kEmail, false));
  QCOMPARE(SvnRepo::getLastError().error,
           SvnRepo::SvnError::UnsupportedOperation);

  GitRepo::BisectStatus bisectStatus;
  QVERIFY(!SvnRepo::bisect(_wc, GitRepo::BisectAction::Good, QString(),
                           bisectStatus));
  QVERIFY(!SvnRepo::addWorktree(_wc, QStringLiteral("w"), _tmp.path() + "/w",
                                QStringLiteral("trunk"), false));
  QVERIFY(SvnRepo::listWorktrees(_wc).isEmpty());
  QVERIFY(!SvnRepo::getIsRebaseInProgress(_wc));
  QVERIFY(SvnRepo::getBisectMarks(_wc).isEmpty());
  QVERIFY(!SvnRepo::startGitHubFlowBranch(_wc, QStringLiteral("x"),
                                          QStringLiteral("trunk")));
}

QTEST_GUILESS_MAIN(SvnRepoTest)

#include "tst_svnrepo.moc"
