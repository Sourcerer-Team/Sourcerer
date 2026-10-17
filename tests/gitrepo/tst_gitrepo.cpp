// Tests for lib/GitRepo: branch heads, default remote and committing a
// resolved rebase step. Repositories are set up with the git executable.

#include <GitRepo.h>

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

namespace {

const QString kAuthor = QStringLiteral("Tester");
const QString kEmail = QStringLiteral("tester@example.com");

bool writeFile(const QString &path, const QByteArray &content) {
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write(content) >= 0;
}

struct RepoCloser {
  void operator()(git_repository *repo) const {
    GitRepo::closeRepository(repo);
  }
};

using RepoPtr = std::unique_ptr<git_repository, RepoCloser>;

} // namespace

class GitRepoTest : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void init();
  void cleanup();

  void commitHeadsAndBranches();
  void defaultRemote();
  void commitRebaseContinuesAfterConflict();

private:
  // Runs git in the repository, fails the test when it fails.
  QString git(const QStringList &args, bool *ok = nullptr) const;
  QString commitAll(const QString &message) const;
  QString file(const QString &name) const { return _root + '/' + name; }

  std::unique_ptr<QTemporaryDir> _tmp;
  QString _root;
  RepoPtr _repo;
  bool _quit = false;
};

QString GitRepoTest::git(const QStringList &args, bool *ok) const {
  QProcess process;
  process.setWorkingDirectory(_root);
  process.start(QStringLiteral("git"),
                QStringList{QStringLiteral("-c"),
                            QStringLiteral("user.name=") + kAuthor,
                            QStringLiteral("-c"),
                            QStringLiteral("user.email=") + kEmail} +
                    args);
  const bool success = process.waitForFinished(60 * 1000) &&
                       process.exitStatus() == QProcess::NormalExit &&
                       process.exitCode() == 0;
  if (ok) {
    *ok = success;
  } else if (!success) {
    qWarning() << "git" << args << "failed:" << process.readAllStandardError();
  }
  return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

QString GitRepoTest::commitAll(const QString &message) const {
  git({QStringLiteral("add"), QStringLiteral("-A")});
  git({QStringLiteral("commit"), QStringLiteral("-q"), QStringLiteral("-m"),
       message});
  return git({QStringLiteral("rev-parse"), QStringLiteral("HEAD")});
}

void GitRepoTest::initTestCase() {
  if (QStandardPaths::findExecutable(QStringLiteral("git")).isEmpty()) {
    QSKIP("git was not found");
  }
  GitRepo::initForCurrentThread();
}

void GitRepoTest::init() {
  _tmp = std::make_unique<QTemporaryDir>();
  QVERIFY(_tmp->isValid());
  _root = QDir::cleanPath(_tmp->path());

  bool ok = false;
  git({QStringLiteral("init"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("main")},
      &ok);
  QVERIFY(ok);
}

void GitRepoTest::cleanup() {
  _repo.reset();
  _tmp.reset();
}

void GitRepoTest::commitHeadsAndBranches() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString root = commitAll(QStringLiteral("Root"));
  QVERIFY(writeFile(file("a.txt"), "a2\n"));
  const QString tip = commitAll(QStringLiteral("Tip"));
  git({QStringLiteral("branch"), QStringLiteral("other"), root});

  _repo.reset(GitRepo::openRepository(_root, kAuthor, kEmail));
  QVERIFY(_repo);

  // A branch tip is a head, a commit below it isn't.
  QCOMPARE(GitRepo::getCommitHeads(_repo.get(), tip, _quit),
           QStringList{QStringLiteral("main")});
  QCOMPARE(GitRepo::getCommitHeads(_repo.get(), root, _quit),
           QStringList{QStringLiteral("other")});
  QVERIFY(GitRepo::getCommitIsHead(_repo.get(), tip, _quit));
  QVERIFY(GitRepo::getCommitIsHead(_repo.get(), root, _quit));
  git({QStringLiteral("branch"), QStringLiteral("-D"),
       QStringLiteral("other")});
  QVERIFY(!GitRepo::getCommitIsHead(_repo.get(), root, _quit));

  // A branch contains its tip as well as the commits below it.
  QCOMPARE(GitRepo::getCommitBranches(_repo.get(), tip, _quit),
           QStringList{QStringLiteral("main")});
  QCOMPARE(GitRepo::getCommitBranches(_repo.get(), root, _quit),
           QStringList{QStringLiteral("main")});
}

void GitRepoTest::defaultRemote() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  commitAll(QStringLiteral("Root"));
  git({QStringLiteral("remote"), QStringLiteral("add"),
       QStringLiteral("origin"),
       QStringLiteral("https://example.invalid/origin.git")});
  git({QStringLiteral("remote"), QStringLiteral("add"),
       QStringLiteral("upstream"),
       QStringLiteral("https://example.invalid/upstream.git")});

  _repo.reset(GitRepo::openRepository(_root, kAuthor, kEmail));
  QVERIFY(_repo);
  QCOMPARE(GitRepo::getDefaultRemote(_repo.get()), QStringLiteral("origin"));

  // The chosen default wins, also in the remotes list.
  QVERIFY(GitRepo::setDefaultRemote(_repo.get(), QStringLiteral("upstream")));
  QCOMPARE(GitRepo::getDefaultRemote(_repo.get()), QStringLiteral("upstream"));
  for (const GitRepo::RemoteInfo &remote : GitRepo::listRemotes(_repo.get())) {
    QCOMPARE(remote.isDefault, remote.name == QStringLiteral("upstream"));
  }

  // A default remote that was removed no longer counts.
  git({QStringLiteral("remote"), QStringLiteral("remove"),
       QStringLiteral("upstream")});
  QCOMPARE(GitRepo::getDefaultRemote(_repo.get()), QStringLiteral("origin"));
}

void GitRepoTest::commitRebaseContinuesAfterConflict() {
  QVERIFY(writeFile(file("a.txt"), "base\n"));
  commitAll(QStringLiteral("Base"));

  // feature: a conflicting step, then one that applies cleanly.
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("feature")});
  QVERIFY(writeFile(file("a.txt"), "feature\n"));
  commitAll(QStringLiteral("Feature change"));
  QVERIFY(writeFile(file("b.txt"), "b\n"));
  commitAll(QStringLiteral("Feature file"));

  git({QStringLiteral("checkout"), QStringLiteral("-q"),
       QStringLiteral("main")});
  QVERIFY(writeFile(file("a.txt"), "main\n"));
  const QString mainTip = commitAll(QStringLiteral("Main change"));

  _repo.reset(GitRepo::openRepository(_root, kAuthor, kEmail));
  QVERIFY(_repo);

  // The first step conflicts and the rebase stops.
  QVERIFY(!GitRepo::rebaseBranches(_repo.get(), QStringLiteral("feature"),
                                   QStringLiteral("main"), kAuthor, kEmail,
                                   false));
  QVERIFY(GitRepo::hasConflicts(_repo.get()));

  // Resolved and committed, the remaining step follows and the rebase ends.
  QVERIFY(writeFile(file("a.txt"), "resolved\n"));
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  QVERIFY2(GitRepo::commitRebase(_repo.get(), QStringLiteral("Resolved"),
                                 QString(), kAuthor, kEmail, true, false,
                                 false),
           qPrintable(GitRepo::getLastError().errorMessage));

  QCOMPARE(git_repository_state(_repo.get()),
           static_cast<int>(GIT_REPOSITORY_STATE_NONE));
  QCOMPARE(git({QStringLiteral("log"), QStringLiteral("--format=%s"),
                QStringLiteral("-3"), QStringLiteral("feature")}),
           QStringLiteral("Feature file\nResolved\nMain change"));
  QCOMPARE(git({QStringLiteral("rev-parse"), QStringLiteral("feature~2")}),
           mainTip);
  QCOMPARE(git({QStringLiteral("status"), QStringLiteral("--porcelain")}),
           QString());
}

QTEST_GUILESS_MAIN(GitRepoTest)

#include "tst_gitrepo.moc"
