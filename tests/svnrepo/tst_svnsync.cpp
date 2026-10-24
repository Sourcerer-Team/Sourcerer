// Two working copies of one file:// repository: updates, out-of-date
// commits, branches and externals changed elsewhere, relocation, and the
// library and the 1.14 command line client on the same working copy. See
// docs/SVN_TEST_PLAN.md section 4; QEXPECT_FAIL markers name the suspected
// defect (section 7) that makes the check fail today.

#include "SvnTestHelpers.h"

#include <SvnRepo.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace SvnTest;

class SvnSyncTest : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void init();
  void cleanup();
  void cleanupTestCase();

  void updateBringsOtherCommits();
  void outOfDateCommitFails();
  void remoteBranchesWithStatus();
  void branchDeletedElsewhere();
  void externalsFromAnotherRepository();
  void relocateAfterRepositoryMove();
  void cliInterop();

private:
  QString file(const QString &name) const { return _wc + "/" + name; }
  QString other(const QString &name) const { return _other + "/" + name; }
  SvnRepo::SvnStatus status() const { return SvnRepo::getStatus(_wc, _quit); }
  QString head() const { return SvnTest::head(_repoDir); }

  std::unique_ptr<QTemporaryDir> _tmp;
  QString _base;
  QString _wc;
  QString _other;
  QString _repoDir;
  QString _rootUrl;
  bool _quit = false;
};

void SvnSyncTest::initTestCase() {
  if (!toolsAvailable()) {
    QSKIP("svn, svnadmin or svnlook was not found");
  }
  QStandardPaths::setTestModeEnabled(true);
  QVERIFY(SvnRepo::init());
  SvnRepo::initForCurrentThread();
  SvnRepo::Testing::installMalfunctionHandler();
  useTestCredentialsKey();
}

void SvnSyncTest::init() {
  _tmp = std::make_unique<QTemporaryDir>();
  QVERIFY(_tmp->isValid());
  _base = QDir::cleanPath(QDir::fromNativeSeparators(_tmp->path()));
  setConfigDir(_base + QStringLiteral("/svnconfig"));

  // The user's working copy, made by the library, and someone else's, made
  // by the command line client, with a.txt committed.
  _wc = _base + QStringLiteral("/wc");
  _other = _base + QStringLiteral("/other");
  _repoDir = _base + QStringLiteral("/repo-svnrepo");
  QVERIFY2(SvnRepo::initRepository(QStringLiteral("repo"), _wc,
                                   QStringLiteral("main"), kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QVERIFY(!SvnRepo::openRepository(_wc).isEmpty());
  _rootUrl = SvnRepo::listRemotes(_wc).first().fetchUrl;
  QVERIFY(writeFile(file("a.txt"), "base\n"));
  QVERIFY(svnCommitAll(_wc, QStringLiteral("Base")));
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  QVERIFY(ok);
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk"), _other));
}

void SvnSyncTest::cleanup() {
  SvnRepo::closeRepository(_wc);
  if (_tmp) {
    makeWritable(_tmp->path());
  }
  _tmp.reset();
  _quit = false;

  const QStringList malfunctions = SvnRepo::Testing::takeMalfunctions();
  QVERIFY2(malfunctions.isEmpty(), qPrintable(malfunctions.join('\n')));
}

void SvnSyncTest::cleanupTestCase() {
  SvnRepo::Testing::setConfigDirectory(QString());
}

void SvnSyncTest::updateBringsOtherCommits() {
  QVERIFY(writeFile(other("a.txt"), "theirs\n"));
  QVERIFY(writeFile(other("new.txt"), "new\n"));
  QVERIFY(svnCommitAll(_other, QStringLiteral("Theirs")));
  bool ok = false;
  svnIn(_other, {QStringLiteral("delete"), QStringLiteral("new.txt")}, &ok);
  QVERIFY(writeFile(other("later.txt"), "later\n"));
  QVERIFY(svnCommitAll(_other, QStringLiteral("Later")));
  QVERIFY(writeFile(file("mine.txt"), "untracked\n"));

  QVERIFY2(SvnRepo::pull(_wc, QStringLiteral("trunk"), false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("a.txt")), QByteArray("theirs\n"));
  QVERIFY(!QFileInfo::exists(file("new.txt")));
  QCOMPARE(readFile(file("later.txt")), QByteArray("later\n"));
  QCOMPARE(svnInfo(_wc, QStringLiteral("revision")), head());
  // Local work is left alone.
  QCOMPARE(status().newFiles, QStringList{QStringLiteral("mine.txt")});
}

void SvnSyncTest::outOfDateCommitFails() {
  QVERIFY(writeFile(other("a.txt"), "theirs\nbase\n"));
  QVERIFY(svnCommitAll(_other, QStringLiteral("Theirs")));
  const QString before = head();
  QVERIFY(writeFile(file("a.txt"), "base\nmine\n"));
  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("a.txt")}));

  resetLastError();
  QVERIFY(!SvnRepo::commitStaged(_wc, QStringLiteral("Mine"), QString(),
                                 kAuthor, kEmail, true, false, false));
  EXPECT_SVN_ERROR(SvnRepo::SvnError::CommitFailed);
  QCOMPARE(head(), before);

  // After an update, which merges the change, the staged file commits.
  QVERIFY(SvnRepo::pull(_wc, QStringLiteral("trunk"), false));
  QCOMPARE(readFile(file("a.txt")), QByteArray("theirs\nbase\nmine\n"));
  QCOMPARE(changelistMembers(svnStatusXml(_wc)),
           QStringList{QStringLiteral("a.txt")});
  QVERIFY2(SvnRepo::commitStaged(_wc, QStringLiteral("Mine"), QString(),
                                 kAuthor, kEmail, true, false, false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(changedPaths(_repoDir, head()),
           QStringList{QStringLiteral("U trunk/a.txt")});
}

void SvnSyncTest::remoteBranchesWithStatus() {
  QVERIFY(SvnRepo::createBranchFromCommit(_wc, QString(), QStringLiteral("b")));
  QVERIFY(SvnRepo::checkoutBranch(_wc, QStringLiteral("b"),
                                  SvnRepo::BranchType::Local));
  const auto statusOf = [&](const QString &name) {
    for (const SvnRepo::BranchInfo &info :
         SvnRepo::listRemoteBranchesWithStatus(_wc, _quit)) {
      if (info.name == name) {
        return info.statusInfo;
      }
    }
    return SvnRepo::BranchStatus();
  };

  // Two revisions on b from the other working copy: Behind 2.
  bool ok = false;
  svnIn(_other,
        {QStringLiteral("switch"), _rootUrl + QStringLiteral("/branches/b")},
        &ok);
  QVERIFY(ok);
  for (int i = 0; i < 2; ++i) {
    QVERIFY(writeFile(other("a.txt"), QByteArray::number(i) + '\n'));
    QVERIFY(svnCommitAll(_other, QStringLiteral("Theirs %1").arg(i)));
  }
  SvnRepo::BranchStatus b = statusOf(QStringLiteral("b"));
  QCOMPARE(b.status, SvnRepo::BranchStatusType::Behind);
  QCOMPARE(b.behindCount, size_t(2));

  // A branch made there shows up, up to date.
  svn({QStringLiteral("copy"), QStringLiteral("-m"), QStringLiteral("Branch"),
       _rootUrl + QStringLiteral("/trunk"),
       _rootUrl + QStringLiteral("/branches/c")},
      &ok);
  QVERIFY(ok);
  QCOMPARE(statusOf(QStringLiteral("c")).status,
           SvnRepo::BranchStatusType::UpToDate);

  // After the update, b is up to date again.
  QVERIFY(SvnRepo::pull(_wc, QStringLiteral("b"), false));
  b = statusOf(QStringLiteral("b"));
  QCOMPARE(b.status, SvnRepo::BranchStatusType::UpToDate);

  // trunk, changed elsewhere while b is checked out, is listed too.
  svnIn(_other,
        {QStringLiteral("switch"), _rootUrl + QStringLiteral("/trunk")}, &ok);
  QVERIFY(writeFile(other("a.txt"), "trunk\n"));
  QVERIFY(svnCommitAll(_other, QStringLiteral("On trunk")));
  QCOMPARE(statusOf(QStringLiteral("trunk")).status,
           SvnRepo::BranchStatusType::UpToDate);
}

void SvnSyncTest::branchDeletedElsewhere() {
  QVERIFY(SvnRepo::createBranchFromCommit(_wc, QString(), QStringLiteral("b")));
  QVERIFY(SvnRepo::checkoutBranch(_wc, QStringLiteral("b"),
                                  SvnRepo::BranchType::Local));
  bool ok = false;
  svn({QStringLiteral("delete"), QStringLiteral("-m"), QStringLiteral("Gone"),
       _rootUrl + QStringLiteral("/branches/b")},
      &ok);
  QVERIFY(ok);

  // Status still works; history works or explains; another branch can be
  // checked out.
  resetLastError();
  QVERIFY(!status().hasChanges());
  QCOMPARE(lastError(), SvnRepo::SvnError::Unknown);
  const SvnRepo::SvnHistory history = SvnRepo::getBranchHistory(
      _wc, QString(), SvnRepo::BranchType::All, -1, _quit);
  QVERIFY2(history.valid, qPrintable(lastErrorMessage()));
  QVERIFY(!SvnRepo::listRemoteBranches(_wc, _quit).contains(
      QStringLiteral("b")));
  QVERIFY2(SvnRepo::checkoutBranch(_wc, QStringLiteral("trunk"),
                                   SvnRepo::BranchType::Local),
           qPrintable(lastErrorMessage()));
  QCOMPARE(svnInfo(_wc, QStringLiteral("url")),
           _rootUrl + QStringLiteral("/trunk"));
}

void SvnSyncTest::externalsFromAnotherRepository() {
  const QString lib = makeRepo(_base + QStringLiteral("/lib"), Layout::Standard);
  const QString libWc = _base + QStringLiteral("/libwc");
  QVERIFY(secondWc(lib + QStringLiteral("/trunk"), libWc));
  QVERIFY(writeFile(libWc + QStringLiteral("/l.txt"), "one\n"));
  QVERIFY(svnCommitAll(libWc, QStringLiteral("Lib one")));

  QVERIFY2(SvnRepo::addSubmodule(_wc, lib + QStringLiteral("/trunk"),
                                 QStringLiteral("ext"), QString(), QString()),
           qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("ext/l.txt")), QByteArray("one\n"));

  // A newer revision of the other repository is fetched by an update of the
  // external.
  QVERIFY(writeFile(libWc + QStringLiteral("/l.txt"), "two\n"));
  QVERIFY(svnCommitAll(libWc, QStringLiteral("Lib two")));
  QVERIFY2(SvnRepo::updateSubmodule(_wc, QStringLiteral("ext")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("ext/l.txt")), QByteArray("two\n"));
  QCOMPARE(SvnRepo::listSubmodules(_wc), QStringList{QStringLiteral("ext")});
}

void SvnSyncTest::relocateAfterRepositoryMove() {
  // An external into the same repository, by absolute URL and by ^/. (Not
  // to trunk itself: an external of trunk inside trunk includes itself
  // again and again.)
  bool ok = false;
  svn({QStringLiteral("mkdir"), QStringLiteral("-m"), QStringLiteral("Add lib"),
       _rootUrl + QStringLiteral("/lib")},
      &ok);
  QVERIFY(ok);
  const QString libWc = _base + QStringLiteral("/libwc");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/lib"), libWc));
  QVERIFY(writeFile(libWc + QStringLiteral("/a.txt"), "lib\n"));
  QVERIFY(svnCommitAll(libWc, QStringLiteral("Lib file")));
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("svn:externals"),
              _rootUrl + QStringLiteral("/lib abs\n^/lib rel"),
              QStringLiteral(".")},
        &ok);
  QVERIFY(ok);
  QVERIFY(svnCommitAll(_wc, QStringLiteral("Externals")));
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  QVERIFY(ok);
  QVERIFY(QFileInfo::exists(file("abs/a.txt")));

  const QString movedDir = _base + QStringLiteral("/moved-repo");
  QVERIFY(QDir().rename(_repoDir, movedDir));
  _repoDir = movedDir;
  const QString movedUrl = fileUrl(movedDir);
  QVERIFY2(SvnRepo::updateRemote(_wc, QStringLiteral("origin"), QString(),
                                 movedUrl, movedUrl),
           qPrintable(lastErrorMessage()));

  // The working copy and the ^/ external follow; the update works.
  QCOMPARE(svnInfo(_wc, QStringLiteral("repos-root-url")), movedUrl);
  const bool pulled = SvnRepo::pull(_wc, QStringLiteral("trunk"), false);
  QVERIFY2(pulled, qPrintable(lastErrorMessage()));
  QCOMPARE(svnInfo(file("rel"), QStringLiteral("repos-root-url")), movedUrl);
  QCOMPARE(svnInfo(file("abs"), QStringLiteral("repos-root-url")), movedUrl);
}

void SvnSyncTest::cliInterop() {
  // The library commits; the 1.14 client sees a clean working copy.
  QVERIFY(writeFile(file("lib.txt"), "lib\n"));
  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("lib.txt")}));
  QVERIFY(SvnRepo::commitStaged(_wc, QStringLiteral("From the library"),
                                QString(), kAuthor, kEmail, true, false,
                                false));
  QVERIFY(svnStatusXml(_wc).isEmpty());
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  QVERIFY(ok);

  // The client commits in the same working copy; the library sees it.
  QVERIFY(writeFile(file("cli.txt"), "cli\n"));
  QVERIFY(svnCommitAll(_wc, QStringLiteral("From the CLI")));
  QVERIFY(!status().hasChanges());
  QCOMPARE(SvnRepo::getHistory(_wc, 1).commits.first(), head());

  // The client commits exactly what the library staged.
  QVERIFY(writeFile(file("a.txt"), "staged\n"));
  QVERIFY(writeFile(file("lib.txt"), "unstaged\n"));
  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("a.txt")}));
  svnIn(_wc, {QStringLiteral("commit"), QStringLiteral("-m"),
              QStringLiteral("Staged by the library"),
              QStringLiteral("--changelist"), kStagingChangelist},
        &ok);
  QVERIFY(ok);
  QCOMPARE(changedPaths(_repoDir, head()),
           QStringList{QStringLiteral("U trunk/a.txt")});
  QCOMPARE(status().modifiedFiles, QStringList{QStringLiteral("lib.txt")});

  // The other working copy, made by the client, is updated by the library.
  QVERIFY(!SvnRepo::openRepository(_other).isEmpty());
  QVERIFY2(SvnRepo::pull(_other, QStringLiteral("trunk"), false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(other("a.txt")), QByteArray("staged\n"));
  QVERIFY(svnStatusXml(_other).isEmpty());
  SvnRepo::closeRepository(_other);
}

QTEST_GUILESS_MAIN(SvnSyncTest)

#include "tst_svnsync.moc"
