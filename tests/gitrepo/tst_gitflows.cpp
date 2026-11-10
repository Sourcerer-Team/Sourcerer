// MIT License
// Copyright (c) 2026 Sourcerer

// Tests for commit signing and the branching workflows of lib/GitRepo: Git
// Flow, git-flow-next, GitHub Flow and GitLab Flow. See docs/GIT_TEST_PLAN.md,
// sections 3.18 and 3.19. Signing uses real ssh-keygen and gpg keys made for
// the test and is skipped without them.

#include "GitTestHelpers.h"

#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace GitTest;
using GitRepo::GitError;

Q_DECLARE_METATYPE(GitRepo::GitError)
Q_DECLARE_METATYPE(GitRepo::GitFlowBranchType)

class GitFlowsTest : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void init();
  void cleanup();

  // Signing.
  void signCommitWithSshKey();
  void signAmendAndSquash();
  void signRebasedCommits();
  void signWithPublicKeyPath();
  void signingFailuresLeaveIndexAlone_data();
  void signingFailuresLeaveIndexAlone();
  void signingDisabledIgnoresSignFlag();
  void signCommitWithGpg();

  // Git Flow.
  void gitFlowStartAndFinish_data();
  void gitFlowStartAndFinish();
  void gitFlowMasterFallback();
  void gitFlowStartFailures();
  void gitFlowFinishConflict();

  // git-flow-next.
  void gitFlowNextInit();
  void gitFlowNextFeature();
  void gitFlowNextReleaseTagsAndUpdatesDevelop();
  void gitFlowNextStrategiesAndKeep();
  void applyCustomGitFlowNextWorkflow();

  // GitHub Flow and GitLab Flow.
  void gitHubFlow();
  void gitLabFlowFeatureAndRelease();
  void gitLabFlowPromotion();
  void gitLabFlowCherryPickToRelease();
  void workflowTypeRoundTrip();

private:
  QString git(const QStringList &args, bool *ok = nullptr) const {
    return gitIn(_config, _root, args, ok);
  }
  QString file(const QString &name) const { return _root + '/' + name; }
  QString head() const {
    return git({QStringLiteral("rev-parse"), QStringLiteral("HEAD")});
  }
  QString rev(const QString &revision) const {
    bool ok = false;
    const QString id =
        git({QStringLiteral("rev-parse"), QStringLiteral("-q"),
             QStringLiteral("--verify"), revision},
            &ok);
    return ok ? id : QString();
  }
  QString currentBranch() const {
    return GitRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local,
                                    _quit);
  }
  bool isAncestor(const QString &ancestor, const QString &of) const {
    bool ok = false;
    git({QStringLiteral("merge-base"), QStringLiteral("--is-ancestor"),
         ancestor, of},
        &ok);
    return ok;
  }
  QString commitAll(const QString &summary) const {
    git({QStringLiteral("add"), QStringLiteral("-A")});
    git({QStringLiteral("commit"), QStringLiteral("-q"),
         QStringLiteral("--allow-empty"), QStringLiteral("-m"), summary});
    return head();
  }
  // Writes `name` on the current branch and commits it.
  QString commitFile(const QString &name, const QByteArray &content,
                     const QString &summary) const {
    writeFile(file(name), content);
    return commitAll(summary);
  }
  void reopen();
  // An ed25519 key without passphrase (or with one) for SSH signing.
  QString makeSshKey(const QString &name, const QString &passphrase = {}) const;
  // `git verify-commit` with the key as the only allowed signer.
  bool verifySshCommit(const QString &revision, const QString &key) const;
  // Turns SSH signing on with the key, for the lifetime of the result.
  std::vector<std::unique_ptr<ConfigOverride>>
  sshSigning(const QString &key, const QString &program = {}) const;

  IsolatedConfig _config;
  std::unique_ptr<QTemporaryDir> _tmp;
  QString _base;
  QString _root;
  RepoPtr _repo;
  bool _quit = false;
  QString _sshKeygen;
};

void GitFlowsTest::reopen() {
  _repo.reset();
  _repo.reset(GitRepo::openRepository(_root, kAuthor, kEmail));
  QVERIFY2(_repo, qPrintable(lastErrorMessage()));
}

QString GitFlowsTest::makeSshKey(const QString &name,
                                 const QString &passphrase) const {
  const QString key = _base + QStringLiteral("/keys/") + name;
  QDir().mkpath(QFileInfo(key).absolutePath());
  QProcess process;
  process.start(_sshKeygen, {QStringLiteral("-q"), QStringLiteral("-t"),
                             QStringLiteral("ed25519"), QStringLiteral("-N"),
                             passphrase, QStringLiteral("-C"), kEmail,
                             QStringLiteral("-f"), key});
  if (!process.waitForFinished(30000) || process.exitCode() != 0) {
    qWarning() << "ssh-keygen failed:" << process.readAllStandardError();
    return {};
  }
  return key;
}

bool GitFlowsTest::verifySshCommit(const QString &revision,
                                   const QString &key) const {
  const QString allowedSigners = _base + QStringLiteral("/allowed_signers");
  writeFile(allowedSigners,
            kEmail.toUtf8() + " namespaces=\"git\" " +
                readFile(key + QStringLiteral(".pub")).trimmed() + '\n');
  bool ok = false;
  git({QStringLiteral("-c"), QStringLiteral("gpg.format=ssh"),
       QStringLiteral("-c"),
       QStringLiteral("gpg.ssh.allowedSignersFile=") + allowedSigners,
       QStringLiteral("-c"), QStringLiteral("gpg.ssh.program=") + _sshKeygen,
       QStringLiteral("verify-commit"), revision},
      &ok);
  return ok;
}

std::vector<std::unique_ptr<ConfigOverride>>
GitFlowsTest::sshSigning(const QString &key, const QString &program) const {
  std::vector<std::unique_ptr<ConfigOverride>> overrides;
  overrides.push_back(std::make_unique<ConfigOverride>(
      ConfigManager::ConfigKey::GitRepoSigningEnabled, true));
  overrides.push_back(std::make_unique<ConfigOverride>(
      ConfigManager::ConfigKey::GitRepoSigningFormat, QStringLiteral("ssh")));
  overrides.push_back(std::make_unique<ConfigOverride>(
      ConfigManager::ConfigKey::GitRepoSigningKey, key));
  overrides.push_back(std::make_unique<ConfigOverride>(
      ConfigManager::ConfigKey::GitRepoSigningProgram,
      program.isEmpty() ? _sshKeygen : program));
  return overrides;
}

void GitFlowsTest::initTestCase() {
  if (QStandardPaths::findExecutable(QStringLiteral("git")).isEmpty()) {
    QSKIP("git was not found");
  }
  QStandardPaths::setTestModeEnabled(true);
  _sshKeygen = QStandardPaths::findExecutable(QStringLiteral("ssh-keygen"));
  GitRepo::initForCurrentThread();
}

void GitFlowsTest::init() {
  _tmp = std::make_unique<QTemporaryDir>();
  QVERIFY(_tmp->isValid());
  _base = QDir::cleanPath(QDir::fromNativeSeparators(_tmp->path()));
  _config.setUp(_base + QStringLiteral("/home"));
  _root = _base + QStringLiteral("/repo");
  QVERIFY2(GitRepo::initRepository(QStringLiteral("repo"), _base,
                                   QStringLiteral("main"), kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  _repo.reset(GitRepo::openRepository(_root, kAuthor, kEmail));
  QVERIFY2(_repo, qPrintable(lastErrorMessage()));
}

void GitFlowsTest::cleanup() {
  _repo.reset();
  if (_tmp) {
    makeWritable(_tmp->path());
  }
  _tmp.reset();
  _quit = false;
}

// Signing.

void GitFlowsTest::signCommitWithSshKey() {
  if (_sshKeygen.isEmpty()) {
    QSKIP("ssh-keygen was not found");
  }
  const QString key = makeSshKey(QStringLiteral("id_ed25519"));
  QVERIFY(!key.isEmpty());
  const auto signing = sshSigning(key);

  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  QVERIFY2(GitRepo::commitStaged(_repo.get(), QStringLiteral("Signed"),
                                 QStringLiteral("Body"), kAuthor, kEmail, true,
                                 true, true),
           qPrintable(lastErrorMessage()));
  QVERIFY(verifySshCommit(QStringLiteral("HEAD"), key));
  // The message and the tree are what was asked for.
  QCOMPARE(commitMessage(_repo.get(), QStringLiteral("HEAD")),
           QStringLiteral("Signed\n\nBody\n\nSigned-off-by: Tester "
                          "<tester@example.com>"));
  QCOMPARE(git({QStringLiteral("ls-tree"), QStringLiteral("--name-only"),
                QStringLiteral("HEAD")}),
           QStringLiteral("a.txt"));
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/main"));

  // The history shows the signature.
  const GitRepo::GitHistory history = GitRepo::getBranchHistory(
      _repo.get(), QStringLiteral("main"), GitRepo::BranchType::Local, 0, _quit);
  QVERIFY(history.valid);
  const GitRepo::GitCommitInfo signedInfo = history.commitInfos[head()];
  QVERIFY(signedInfo.signedCommit);
  QCOMPARE(signedInfo.signatureType, QStringLiteral("SSH"));
  QVERIFY(signedInfo.signature.contains(QStringLiteral("BEGIN SSH SIGNATURE")));
  const GitRepo::GitCommitInfo unsignedInfo =
      history.commitInfos[rev(QStringLiteral("HEAD~1"))];
  QVERIFY(!unsignedInfo.signedCommit);
  QCOMPARE(unsignedInfo.signatureType, QStringLiteral("Unsigned"));

  // A signed merge commit.
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("feature")});
  const QString feature =
      commitFile(QStringLiteral("f.txt"), "f\n", QStringLiteral("F"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"),
       QStringLiteral("main")});
  commitFile(QStringLiteral("m.txt"), "m\n", QStringLiteral("M"));
  git({QStringLiteral("merge"), QStringLiteral("-q"), QStringLiteral("--no-ff"),
       QStringLiteral("--no-commit"), QStringLiteral("feature")});
  QVERIFY2(GitRepo::commitStaged(_repo.get(), QStringLiteral("Merge"),
                                 QString(), kAuthor, kEmail, true, false, true),
           qPrintable(lastErrorMessage()));
  QVERIFY(verifySshCommit(QStringLiteral("HEAD"), key));
  QCOMPARE(rev(QStringLiteral("HEAD^2")), feature);
}

void GitFlowsTest::signAmendAndSquash() {
  if (_sshKeygen.isEmpty()) {
    QSKIP("ssh-keygen was not found");
  }
  const QString key = makeSshKey(QStringLiteral("id_ed25519"));
  QVERIFY(!key.isEmpty());
  const auto signing = sshSigning(key);

  commitFile(QStringLiteral("a.txt"), "a\n", QStringLiteral("Unsigned"));
  QVERIFY(!verifySshCommit(QStringLiteral("HEAD"), key));
  QVERIFY2(GitRepo::amend(_repo.get(), QStringLiteral("Amended"), QString(),
                          kAuthor, kEmail, false, true),
           qPrintable(lastErrorMessage()));
  QVERIFY(verifySshCommit(QStringLiteral("HEAD"), key));
  QCOMPARE(commitMessage(_repo.get(), QStringLiteral("HEAD")),
           QStringLiteral("Amended"));
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/main"));

  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("feature")});
  commitFile(QStringLiteral("f1.txt"), "1\n", QStringLiteral("F1"));
  commitFile(QStringLiteral("f2.txt"), "2\n", QStringLiteral("F2"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"),
       QStringLiteral("main")});
  QVERIFY2(GitRepo::squashBranches(_repo.get(), QStringLiteral("feature"),
                                   QStringLiteral("main"), kAuthor, kEmail,
                                   true),
           qPrintable(lastErrorMessage()));
  QVERIFY(verifySshCommit(QStringLiteral("main"), key));
}

void GitFlowsTest::signRebasedCommits() {
  if (_sshKeygen.isEmpty()) {
    QSKIP("ssh-keygen was not found");
  }
  const QString key = makeSshKey(QStringLiteral("id_ed25519"));
  QVERIFY(!key.isEmpty());
  const auto signing = sshSigning(key);

  commitFile(QStringLiteral("a.txt"), "a\n", QStringLiteral("Base"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("feature")});
  commitFile(QStringLiteral("f.txt"), "f\n", QStringLiteral("Feature"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"),
       QStringLiteral("main")});
  const QString mainTip =
      commitFile(QStringLiteral("m.txt"), "m\n", QStringLiteral("Main"));

  QVERIFY2(GitRepo::rebaseBranches(_repo.get(), QStringLiteral("feature"),
                                   QStringLiteral("main"), kAuthor, kEmail,
                                   true),
           qPrintable(lastErrorMessage()));
  QCOMPARE(rev(QStringLiteral("feature~1")), mainTip);
  // Like `git rebase -S`, every replayed commit carries a valid signature.
  QVERIFY(git({QStringLiteral("cat-file"), QStringLiteral("commit"),
               QStringLiteral("feature")})
              .contains(QStringLiteral("gpgsig")));
  QVERIFY(verifySshCommit(QStringLiteral("feature"), key));
}

void GitFlowsTest::signWithPublicKeyPath() {
  if (_sshKeygen.isEmpty()) {
    QSKIP("ssh-keygen was not found");
  }
  const QString key = makeSshKey(QStringLiteral("id_ed25519"));
  QVERIFY(!key.isEmpty());
  // Settings often name the .pub file, the private key next to it is used.
  const auto signing = sshSigning(key + QStringLiteral(".pub"));
  QVERIFY2(GitRepo::commitStaged(_repo.get(), QStringLiteral("Signed"),
                                 QString(), kAuthor, kEmail, true, false, true),
           qPrintable(lastErrorMessage()));
  QVERIFY(verifySshCommit(QStringLiteral("HEAD"), key));
}

void GitFlowsTest::signingFailuresLeaveIndexAlone_data() {
  QTest::addColumn<QString>("kind");
  QTest::addColumn<GitError>("error");
  QTest::newRow("passphrase key")
      << QStringLiteral("passphrase")
      << GitError::CommitStagedFailedToLoadSigningConfiguration;
  QTest::newRow("no key") << QStringLiteral("empty")
                          << GitError::CommitStagedFailedToLoadSigningConfiguration;
  QTest::newRow("missing key file")
      << QStringLiteral("missing") << GitError::CommitStagedFailedToSignCommit;
  QTest::newRow("missing program")
      << QStringLiteral("program") << GitError::CommitStagedFailedToSignCommit;
}

void GitFlowsTest::signingFailuresLeaveIndexAlone() {
  QFETCH(QString, kind);
  QFETCH(GitError, error);
  if (_sshKeygen.isEmpty()) {
    QSKIP("ssh-keygen was not found");
  }

  QString key;
  QString program;
  if (kind == QStringLiteral("passphrase")) {
    key = makeSshKey(QStringLiteral("protected"), QStringLiteral("secret"));
    QVERIFY(!key.isEmpty());
  } else if (kind == QStringLiteral("missing")) {
    key = _base + QStringLiteral("/keys/missing");
  } else if (kind == QStringLiteral("program")) {
    key = makeSshKey(QStringLiteral("id_ed25519"));
    program = _base + QStringLiteral("/no/such/ssh-keygen.exe");
  }
  const auto signing = sshSigning(key, program);

  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  const QString before = head();

  QVERIFY(!GitRepo::commitStaged(_repo.get(), QStringLiteral("Signed"),
                                 QString(), kAuthor, kEmail, true, false, true));
  QCOMPARE(lastError(), error);
  // No commit, the file is still staged.
  QCOMPARE(head(), before);
  QCOMPARE(GitRepo::getStatus(_repo.get(), _quit).newStagedFiles,
           QStringList{QStringLiteral("a.txt")});

  // Amending fails the same way and keeps HEAD.
  QVERIFY(!GitRepo::amend(_repo.get(), QStringLiteral("Amended"), QString(),
                          kAuthor, kEmail, false, true));
  QCOMPARE(head(), before);
}

void GitFlowsTest::signingDisabledIgnoresSignFlag() {
  const ConfigOverride disabled(ConfigManager::ConfigKey::GitRepoSigningEnabled,
                                false);
  QVERIFY2(GitRepo::commitStaged(_repo.get(), QStringLiteral("Unsigned"),
                                 QString(), kAuthor, kEmail, true, false, true),
           qPrintable(lastErrorMessage()));
  QVERIFY(!git({QStringLiteral("cat-file"), QStringLiteral("commit"),
                QStringLiteral("HEAD")})
               .contains(QStringLiteral("gpgsig")));
  QVERIFY(GitRepo::amend(_repo.get(), QStringLiteral("Amended"), QString(),
                         kAuthor, kEmail, false, true));
  QVERIFY(!git({QStringLiteral("cat-file"), QStringLiteral("commit"),
                QStringLiteral("HEAD")})
               .contains(QStringLiteral("gpgsig")));
}

void GitFlowsTest::signCommitWithGpg() {
  const QString gpg = QStandardPaths::findExecutable(QStringLiteral("gpg"));
  if (gpg.isEmpty()) {
    QSKIP("gpg was not found");
  }
  // A throwaway keyring, also for the gpg git runs to verify.
  const QString home = _base + QStringLiteral("/gnupg");
  QVERIFY(QDir().mkpath(home));
  const QByteArray previousHome = qgetenv("GNUPGHOME");
  qputenv("GNUPGHOME", QDir::toNativeSeparators(home).toLocal8Bit());
  const auto restore = qScopeGuard([&] {
    QProcess::execute(QStringLiteral("gpgconf"),
                      {QStringLiteral("--kill"), QStringLiteral("gpg-agent")});
    if (previousHome.isEmpty()) {
      qunsetenv("GNUPGHOME");
    } else {
      qputenv("GNUPGHOME", previousHome);
    }
  });
  QProcess keygen;
  keygen.start(gpg, {QStringLiteral("--batch"), QStringLiteral("--passphrase"),
                     QString(), QStringLiteral("--quick-gen-key"),
                     QStringLiteral("Tester <tester@example.com>"),
                     QStringLiteral("ed25519"), QStringLiteral("sign"),
                     QStringLiteral("never")});
  if (!keygen.waitForFinished(60000) || keygen.exitCode() != 0) {
    QSKIP(qPrintable(QStringLiteral("gpg can't make a key: ") +
                     QString::fromUtf8(keygen.readAllStandardError())));
  }

  const ConfigOverride enabled(ConfigManager::ConfigKey::GitRepoSigningEnabled,
                               true);
  const ConfigOverride format(ConfigManager::ConfigKey::GitRepoSigningFormat,
                              QStringLiteral("openpgp"));
  const ConfigOverride key(ConfigManager::ConfigKey::GitRepoSigningKey, kEmail);
  const ConfigOverride program(ConfigManager::ConfigKey::GitRepoSigningProgram,
                               gpg);

  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  QVERIFY2(GitRepo::commitStaged(_repo.get(), QStringLiteral("Signed"),
                                 QString(), kAuthor, kEmail, true, false, true),
           qPrintable(lastErrorMessage()));
  bool ok = false;
  git({QStringLiteral("-c"), QStringLiteral("gpg.program=") + gpg,
       QStringLiteral("verify-commit"), QStringLiteral("HEAD")},
      &ok);
  QVERIFY(ok);
  const GitRepo::GitHistory history = GitRepo::getBranchHistory(
      _repo.get(), QStringLiteral("main"), GitRepo::BranchType::Local, 0, _quit);
  QCOMPARE(history.commitInfos[head()].signatureType, QStringLiteral("OpenPGP"));

  // An unknown key id fails the commit.
  const ConfigOverride unknown(ConfigManager::ConfigKey::GitRepoSigningKey,
                               QStringLiteral("nobody@example.invalid"));
  const QString before = head();
  QVERIFY(!GitRepo::commitStaged(_repo.get(), QStringLiteral("Not signed"),
                                 QString(), kAuthor, kEmail, true, false, true));
  QCOMPARE(lastError(), GitError::CommitStagedFailedToSignCommit);
  QCOMPARE(head(), before);
}

// Git Flow.

void GitFlowsTest::gitFlowStartAndFinish_data() {
  QTest::addColumn<GitRepo::GitFlowBranchType>("type");
  QTest::addColumn<QString>("name");
  QTest::addColumn<QString>("branch");
  QTest::addColumn<QString>("base");
  QTest::addColumn<bool>("intoMain");
  QTest::addColumn<bool>("intoDevelop");
  QTest::newRow("feature") << GitRepo::GitFlowBranchType::Feature
                           << QStringLiteral("login")
                           << QStringLiteral("feature/login")
                           << QStringLiteral("develop") << false << true;
  QTest::newRow("feature with prefix")
      << GitRepo::GitFlowBranchType::Feature << QStringLiteral("feature/login")
      << QStringLiteral("feature/login") << QStringLiteral("develop") << false
      << true;
  QTest::newRow("release") << GitRepo::GitFlowBranchType::Release
                           << QStringLiteral("1.0")
                           << QStringLiteral("release/1.0")
                           << QStringLiteral("develop") << true << true;
  QTest::newRow("hotfix") << GitRepo::GitFlowBranchType::Hotfix
                          << QStringLiteral(" 1.0.1 ")
                          << QStringLiteral("hotfix/1.0.1")
                          << QStringLiteral("main") << true << true;
}

void GitFlowsTest::gitFlowStartAndFinish() {
  QFETCH(GitRepo::GitFlowBranchType, type);
  QFETCH(QString, name);
  QFETCH(QString, branch);
  QFETCH(QString, base);
  QFETCH(bool, intoMain);
  QFETCH(bool, intoDevelop);

  const ConfigOverride mainName(ConfigManager::ConfigKey::GitRepoGitFlowMainBranchName,
                                QStringLiteral("main"));
  const ConfigOverride developName(
      ConfigManager::ConfigKey::GitRepoGitFlowDevelopmentBranchName,
      QStringLiteral("develop"));
  commitFile(QStringLiteral("a.txt"), "a\n", QStringLiteral("Base"));
  git({QStringLiteral("branch"), QStringLiteral("develop")});
  git({QStringLiteral("checkout"), QStringLiteral("-q"),
       QStringLiteral("develop")});
  commitFile(QStringLiteral("d.txt"), "d\n", QStringLiteral("Develop"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"),
       QStringLiteral("main")});
  const QString mainBefore = rev(QStringLiteral("main"));
  const QString developBefore = rev(QStringLiteral("develop"));
  const QString baseTip = rev(base);

  QVERIFY2(GitRepo::startGitFlowBranch(_repo.get(), type, name),
           qPrintable(lastErrorMessage()));
  QCOMPARE(currentBranch(), branch);
  QCOMPARE(head(), baseTip);
  const QString work =
      commitFile(QStringLiteral("work.txt"), "w\n", QStringLiteral("Work"));

  QVERIFY2(GitRepo::finishGitFlowBranch(_repo.get(), type, name, kAuthor,
                                        kEmail),
           qPrintable(lastErrorMessage()));
  QCOMPARE(rev(QStringLiteral("refs/heads/") + branch), QString());
  QCOMPARE(isAncestor(work, QStringLiteral("main")), intoMain);
  QCOMPARE(isAncestor(work, QStringLiteral("develop")), intoDevelop);
  if (!intoMain) {
    QCOMPARE(rev(QStringLiteral("main")), mainBefore);
  }
  QVERIFY(isAncestor(developBefore, QStringLiteral("develop")));
  QVERIFY(!GitRepo::getIsMergeInProgress(_repo.get()));
  QVERIFY(!GitRepo::getStatus(_repo.get(), _quit).hasChanges());

  if (type != GitRepo::GitFlowBranchType::Feature) {
    // git flow tags releases and hotfixes on main.
    const QString tag = branch.mid(branch.indexOf('/') + 1);
    QCOMPARE(rev(QStringLiteral("refs/tags/") + tag + QStringLiteral("^{commit}")),
             rev(QStringLiteral("main")));
  }
}

void GitFlowsTest::gitFlowMasterFallback() {
  const ConfigOverride mainName(ConfigManager::ConfigKey::GitRepoGitFlowMainBranchName,
                                QStringLiteral("main"));
  const ConfigOverride developName(
      ConfigManager::ConfigKey::GitRepoGitFlowDevelopmentBranchName,
      QStringLiteral("develop"));
  commitFile(QStringLiteral("a.txt"), "a\n", QStringLiteral("Base"));
  git({QStringLiteral("branch"), QStringLiteral("-m"), QStringLiteral("master")});
  const QString master = head();

  // Without main and develop, a hotfix starts from master.
  QVERIFY2(GitRepo::startGitFlowBranch(_repo.get(),
                                       GitRepo::GitFlowBranchType::Hotfix,
                                       QStringLiteral("x")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(currentBranch(), QStringLiteral("hotfix/x"));
  QCOMPARE(head(), master);

  // A feature starts from the given development branch.
  git({QStringLiteral("branch"), QStringLiteral("dev"), master});
  QVERIFY(GitRepo::startGitFlowBranch(_repo.get(),
                                      GitRepo::GitFlowBranchType::Feature,
                                      QStringLiteral("y"), QStringLiteral("dev")));
  QCOMPARE(currentBranch(), QStringLiteral("feature/y"));
}

void GitFlowsTest::gitFlowStartFailures() {
  commitFile(QStringLiteral("a.txt"), "a\n", QStringLiteral("Base"));
  git({QStringLiteral("branch"), QStringLiteral("develop")});
  git({QStringLiteral("branch"), QStringLiteral("feature/taken")});

  QVERIFY(!GitRepo::startGitFlowBranch(_repo.get(),
                                       GitRepo::GitFlowBranchType::Feature,
                                       QStringLiteral("taken")));
  QCOMPARE(currentBranch(), QStringLiteral("main"));
  QVERIFY(!GitRepo::startGitFlowBranch(_repo.get(),
                                       GitRepo::GitFlowBranchType::Feature,
                                       QStringLiteral("  ")));
  QVERIFY(!GitRepo::startGitFlowBranch(_repo.get(),
                                       GitRepo::GitFlowBranchType::Feature,
                                       QStringLiteral("bad..name")));
  QCOMPARE(currentBranch(), QStringLiteral("main"));

  // A branch type that isn't a Git Flow type is refused.
  const bool started = GitRepo::startGitFlowBranch(
      _repo.get(), GitRepo::GitFlowBranchType::Unknown, QStringLiteral("x"));
  QVERIFY(!started);
  QCOMPARE(lastError(), GitError::GitFlowUnknownBranchType);
  QCOMPARE(currentBranch(), QStringLiteral("main"));
  QCOMPARE(rev(QStringLiteral("refs/heads/x")), QString());
  QVERIFY(!GitRepo::startGitFlowBranch(_repo.get(),
                                       GitRepo::GitFlowBranchType::Feature,
                                       QStringLiteral("bad..name")));
  QCOMPARE(currentBranch(), QStringLiteral("main"));
}

void GitFlowsTest::gitFlowFinishConflict() {
  const ConfigOverride developName(
      ConfigManager::ConfigKey::GitRepoGitFlowDevelopmentBranchName,
      QStringLiteral("develop"));
  commitFile(QStringLiteral("a.txt"), "base\n", QStringLiteral("Base"));
  git({QStringLiteral("branch"), QStringLiteral("develop")});
  QVERIFY(GitRepo::startGitFlowBranch(_repo.get(),
                                      GitRepo::GitFlowBranchType::Feature,
                                      QStringLiteral("clash")));
  commitFile(QStringLiteral("a.txt"), "feature\n", QStringLiteral("Feature"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"),
       QStringLiteral("develop")});
  const QString develop =
      commitFile(QStringLiteral("a.txt"), "develop\n", QStringLiteral("Develop"));

  QVERIFY(!GitRepo::finishGitFlowBranch(_repo.get(),
                                        GitRepo::GitFlowBranchType::Feature,
                                        QStringLiteral("clash"), kAuthor,
                                        kEmail));
  // The branch is kept and the merge waits to be resolved on develop.
  QVERIFY(!rev(QStringLiteral("refs/heads/feature/clash")).isEmpty());
  QCOMPARE(currentBranch(), QStringLiteral("develop"));
  QCOMPARE(rev(QStringLiteral("develop")), develop);
  QVERIFY(GitRepo::hasConflicts(_repo.get()));
  QVERIFY(GitRepo::abortMerge(_repo.get()));
  QCOMPARE(readFile(file("a.txt")), QByteArray("develop\n"));
}

// git-flow-next.

void GitFlowsTest::gitFlowNextInit() {
  commitFile(QStringLiteral("a.txt"), "a\n", QStringLiteral("Base"));

  QVERIFY(!GitRepo::startGitFlowNextBranch(_repo.get(),
                                           QStringLiteral("feature"),
                                           QStringLiteral("x")));
  QCOMPARE(lastError(), GitError::GitFlowNextNotInitialized);
  GitRepo::GitFlowNextConfig config;
  QVERIFY(GitRepo::readGitFlowNextConfig(_repo.get(), &config));
  QVERIFY(!config.initialized);

  QVERIFY(!GitRepo::initGitFlowNext(_repo.get(), QString(),
                                    QStringLiteral("develop")));
  QCOMPARE(lastError(), GitError::GitFlowNextBranchNameEmpty);
  QVERIFY(!GitRepo::initGitFlowNext(_repo.get(), QStringLiteral("trunk"),
                                    QStringLiteral("develop")));
  QCOMPARE(lastError(), GitError::GitFlowNextFailedToLookupBranch);
  bool unused = false;
  QCOMPARE(git({QStringLiteral("config"), QStringLiteral("--get-regexp"),
                QStringLiteral("^gitflow\\.")},
               &unused),
           QString());

  QVERIFY2(GitRepo::initGitFlowNext(_repo.get(), QStringLiteral("main"),
                                    QStringLiteral("develop")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(rev(QStringLiteral("develop")), rev(QStringLiteral("main")));
  // The configuration is the one `git flow init --preset=classic` writes.
  QCOMPARE(git({QStringLiteral("config"), QStringLiteral("gitflow.initialized")}),
           QStringLiteral("true"));
  QCOMPARE(git({QStringLiteral("config"),
                QStringLiteral("gitflow.branch.feature.parent")}),
           QStringLiteral("develop"));
  QCOMPARE(git({QStringLiteral("config"),
                QStringLiteral("gitflow.branch.release.startPoint")}),
           QStringLiteral("develop"));
  QCOMPARE(git({QStringLiteral("config"),
                QStringLiteral("gitflow.branch.hotfix.parent")}),
           QStringLiteral("main"));
  QVERIFY(GitRepo::readGitFlowNextConfig(_repo.get(), &config));
  QVERIFY(config.initialized);
  QCOMPARE(config.topicTypes().size(), 3);

  // Initializing again keeps a branch model the user changed.
  git({QStringLiteral("config"), QStringLiteral("gitflow.branch.feature.prefix"),
       QStringLiteral("feat/")});
  reopen();
  QVERIFY(GitRepo::initGitFlowNext(_repo.get(), QStringLiteral("main"),
                                   QStringLiteral("develop")));
  QCOMPARE(git({QStringLiteral("config"),
                QStringLiteral("gitflow.branch.feature.prefix")}),
           QStringLiteral("feat/"));
}

void GitFlowsTest::gitFlowNextFeature() {
  commitFile(QStringLiteral("a.txt"), "a\n", QStringLiteral("Base"));
  QVERIFY(GitRepo::initGitFlowNext(_repo.get(), QStringLiteral("main"),
                                   QStringLiteral("develop")));

  QVERIFY(!GitRepo::startGitFlowNextBranch(_repo.get(), QStringLiteral("epic"),
                                           QStringLiteral("x")));
  QCOMPARE(lastError(), GitError::GitFlowNextUnknownBranchType);
  QVERIFY(!GitRepo::startGitFlowNextBranch(_repo.get(),
                                           QStringLiteral("feature"),
                                           QStringLiteral(" ")));
  QCOMPARE(lastError(), GitError::GitFlowNextBranchNameEmpty);

  QVERIFY2(GitRepo::startGitFlowNextBranch(_repo.get(),
                                           QStringLiteral("feature"),
                                           QStringLiteral("x")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(currentBranch(), QStringLiteral("feature/x"));
  const QString work =
      commitFile(QStringLiteral("x.txt"), "x\n", QStringLiteral("X"));

  // develop moves, updating the feature rebases it (downstream strategy).
  git({QStringLiteral("checkout"), QStringLiteral("-q"),
       QStringLiteral("develop")});
  const QString develop =
      commitFile(QStringLiteral("d.txt"), "d\n", QStringLiteral("Develop"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"),
       QStringLiteral("feature/x")});
  // libgit2 may keep the index it read before git changed it.
  reopen();
  QVERIFY2(GitRepo::updateGitFlowNextBranch(_repo.get(),
                                            QStringLiteral("feature/x"),
                                            kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QCOMPARE(rev(QStringLiteral("feature/x~1")), develop);
  QVERIFY(rev(QStringLiteral("feature/x")) != work);

  QVERIFY(!GitRepo::finishGitFlowNextBranch(_repo.get(), QStringLiteral("develop"),
                                            kAuthor, kEmail));
  QCOMPARE(lastError(), GitError::GitFlowNextNotATopicBranch);
  QVERIFY(!GitRepo::finishGitFlowNextBranch(_repo.get(),
                                            QStringLiteral("feature/none"),
                                            kAuthor, kEmail));
  QCOMPARE(lastError(), GitError::GitFlowNextFailedToLookupBranch);

  const QString feature = rev(QStringLiteral("feature/x"));
  QVERIFY2(GitRepo::finishGitFlowNextBranch(_repo.get(),
                                            QStringLiteral("feature/x"),
                                            kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QCOMPARE(currentBranch(), QStringLiteral("develop"));
  QVERIFY(isAncestor(feature, QStringLiteral("develop")));
  QCOMPARE(rev(QStringLiteral("refs/heads/feature/x")), QString());
  QVERIFY(!isAncestor(feature, QStringLiteral("main")));
}

void GitFlowsTest::gitFlowNextReleaseTagsAndUpdatesDevelop() {
  commitFile(QStringLiteral("a.txt"), "a\n", QStringLiteral("Base"));
  QVERIFY(GitRepo::initGitFlowNext(_repo.get(), QStringLiteral("main"),
                                   QStringLiteral("develop")));
  commitFile(QStringLiteral("d.txt"), "d\n", QStringLiteral("On develop"));

  QVERIFY(GitRepo::startGitFlowNextBranch(_repo.get(), QStringLiteral("release"),
                                          QStringLiteral("1.0")));
  QCOMPARE(currentBranch(), QStringLiteral("release/1.0"));
  QCOMPARE(head(), rev(QStringLiteral("develop")));
  const QString release =
      commitFile(QStringLiteral("r.txt"), "r\n", QStringLiteral("Release"));

  QVERIFY2(GitRepo::finishGitFlowNextBranch(_repo.get(),
                                            QStringLiteral("release/1.0"),
                                            kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  // Into main, tagged there, develop follows main, the branch is gone.
  QVERIFY(isAncestor(release, QStringLiteral("main")));
  QCOMPARE(rev(QStringLiteral("refs/tags/1.0^{commit}")), rev(QStringLiteral("main")));
  QVERIFY(isAncestor(QStringLiteral("main"), QStringLiteral("develop")));
  QCOMPARE(rev(QStringLiteral("refs/heads/release/1.0")), QString());
  QCOMPARE(currentBranch(), QStringLiteral("main"));

  // A release whose tag exists already is refused before anything changes.
  QVERIFY(GitRepo::startGitFlowNextBranch(_repo.get(), QStringLiteral("release"),
                                          QStringLiteral("1.0")));
  commitFile(QStringLiteral("r2.txt"), "r2\n", QStringLiteral("Release again"));
  const QString mainBefore = rev(QStringLiteral("main"));
  QVERIFY(!GitRepo::finishGitFlowNextBranch(_repo.get(),
                                            QStringLiteral("release/1.0"),
                                            kAuthor, kEmail));
  QCOMPARE(lastError(), GitError::GitFlowNextTagExists);
  QCOMPARE(rev(QStringLiteral("main")), mainBefore);
  QVERIFY(!rev(QStringLiteral("refs/heads/release/1.0")).isEmpty());

  // A tag prefix and no tag.
  git({QStringLiteral("config"), QStringLiteral("gitflow.branch.hotfix.tagprefix"),
       QStringLiteral("v")});
  QVERIFY(GitRepo::startGitFlowNextBranch(_repo.get(), QStringLiteral("hotfix"),
                                          QStringLiteral("1.0.1")));
  commitFile(QStringLiteral("h.txt"), "h\n", QStringLiteral("Hotfix"));
  QVERIFY2(GitRepo::finishGitFlowNextBranch(_repo.get(),
                                            QStringLiteral("hotfix/1.0.1"),
                                            kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QCOMPARE(rev(QStringLiteral("refs/tags/v1.0.1^{commit}")),
           rev(QStringLiteral("main")));
}

void GitFlowsTest::gitFlowNextStrategiesAndKeep() {
  commitFile(QStringLiteral("a.txt"), "a\n", QStringLiteral("Base"));
  QVERIFY(GitRepo::initGitFlowNext(_repo.get(), QStringLiteral("main"),
                                   QStringLiteral("develop")));

  // Squash into develop.
  git({QStringLiteral("config"),
       QStringLiteral("gitflow.branch.feature.upstreamStrategy"),
       QStringLiteral("squash")});
  QVERIFY(GitRepo::startGitFlowNextBranch(_repo.get(), QStringLiteral("feature"),
                                          QStringLiteral("s")));
  commitFile(QStringLiteral("s1.txt"), "1\n", QStringLiteral("S1"));
  commitFile(QStringLiteral("s2.txt"), "2\n", QStringLiteral("S2"));
  const QString developBefore = rev(QStringLiteral("develop"));
  QVERIFY2(GitRepo::finishGitFlowNextBranch(_repo.get(), QStringLiteral("feature/s"),
                                            kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QCOMPARE(rev(QStringLiteral("develop~1")), developBefore);
  QCOMPARE(git({QStringLiteral("log"), QStringLiteral("-1"),
                QStringLiteral("--format=%P"), QStringLiteral("develop")}),
           developBefore);

  // Rebase onto develop, then fast-forward: no merge commit.
  git({QStringLiteral("config"),
       QStringLiteral("gitflow.branch.feature.upstreamStrategy"),
       QStringLiteral("rebase")});
  QVERIFY(GitRepo::startGitFlowNextBranch(_repo.get(), QStringLiteral("feature"),
                                          QStringLiteral("r")));
  commitFile(QStringLiteral("r.txt"), "r\n", QStringLiteral("R"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"),
       QStringLiteral("develop")});
  commitFile(QStringLiteral("d.txt"), "d\n", QStringLiteral("D"));
  reopen();
  QVERIFY2(GitRepo::finishGitFlowNextBranch(_repo.get(), QStringLiteral("feature/r"),
                                            kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("log"), QStringLiteral("-1"),
                QStringLiteral("--format=%s|%P"), QStringLiteral("develop")})
               .split('|')
               .first(),
           QStringLiteral("R"));
  QCOMPARE(git({QStringLiteral("rev-list"), QStringLiteral("--merges"),
                QStringLiteral("--count"), QStringLiteral("develop")}),
           QStringLiteral("0"));

  // An unknown strategy is refused.
  git({QStringLiteral("config"),
       QStringLiteral("gitflow.branch.feature.upstreamStrategy"),
       QStringLiteral("octopus")});
  QVERIFY(GitRepo::startGitFlowNextBranch(_repo.get(), QStringLiteral("feature"),
                                          QStringLiteral("o")));
  QVERIFY(!GitRepo::finishGitFlowNextBranch(_repo.get(), QStringLiteral("feature/o"),
                                            kAuthor, kEmail));
  QCOMPARE(lastError(), GitError::GitFlowNextUnsupportedStrategy);

  // finish.keep keeps the branch.
  git({QStringLiteral("config"),
       QStringLiteral("gitflow.branch.feature.upstreamStrategy"),
       QStringLiteral("merge")});
  git({QStringLiteral("config"), QStringLiteral("gitflow.feature.finish.keep"),
       QStringLiteral("true")});
  commitFile(QStringLiteral("o.txt"), "o\n", QStringLiteral("O"));
  QVERIFY2(GitRepo::finishGitFlowNextBranch(_repo.get(), QStringLiteral("feature/o"),
                                            kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QVERIFY(!rev(QStringLiteral("refs/heads/feature/o")).isEmpty());
  QVERIFY(isAncestor(QStringLiteral("feature/o"), QStringLiteral("develop")));
}

void GitFlowsTest::applyCustomGitFlowNextWorkflow() {
  commitFile(QStringLiteral("a.txt"), "a\n", QStringLiteral("Base"));
  QVERIFY(GitRepo::initGitFlowNext(_repo.get(), QStringLiteral("main"),
                                   QStringLiteral("develop")));
  git({QStringLiteral("checkout"), QStringLiteral("-q"),
       QStringLiteral("main")});

  GitRepo::GitFlowNextWorkflow workflow;
  workflow.id = QStringLiteral("team-flow");
  workflow.name = QStringLiteral("Team flow");
  workflow.revision = 3;
  GitRepo::GitFlowNextBranch main;
  main.name = QStringLiteral("main");
  GitRepo::GitFlowNextBranch staging;
  staging.name = QStringLiteral("staging");
  staging.parent = QStringLiteral("main");
  staging.autoUpdate = true;
  GitRepo::GitFlowNextBranch story;
  story.name = QStringLiteral("story");
  story.topic = true;
  story.parent = QStringLiteral("staging");
  story.prefix = QStringLiteral("story/");
  workflow.branches = {main, staging, story};

  QVERIFY2(GitRepo::applyGitFlowNextWorkflow(_repo.get(), workflow),
           qPrintable(lastErrorMessage()));
  // The missing base branch is made from its parent, not checked out.
  QCOMPARE(rev(QStringLiteral("staging")), rev(QStringLiteral("main")));
  QCOMPARE(currentBranch(), QStringLiteral("main"));
  QCOMPARE(GitRepo::getWorkflowType(_repo.get()), GitRepo::WorkflowType::GitFlowNext);
  QString id;
  int revision = 0;
  QVERIFY(GitRepo::getGitFlowNextWorkflowReference(_repo.get(), &id, &revision));
  QCOMPARE(id, workflow.id);
  QCOMPARE(revision, 3);
  // The previous branch model is replaced.
  GitRepo::GitFlowNextConfig config;
  QVERIFY(GitRepo::readGitFlowNextConfig(_repo.get(), &config));
  QCOMPARE(config.topicTypes().size(), 1);
  QCOMPARE(config.topicTypes()[0].name, QStringLiteral("story"));
  bool unused = false;
  QCOMPARE(git({QStringLiteral("config"), QStringLiteral("--get"),
                QStringLiteral("gitflow.branch.feature.type")},
               &unused),
           QString());

  QVERIFY(GitRepo::startGitFlowNextBranch(_repo.get(), QStringLiteral("story"),
                                          QStringLiteral("S-1")));
  QCOMPARE(currentBranch(), QStringLiteral("story/S-1"));

  // Choosing another workflow forgets the custom one.
  QVERIFY(GitRepo::setWorkflowType(_repo.get(), GitRepo::WorkflowType::GitHubFlow));
  QVERIFY(!GitRepo::getGitFlowNextWorkflowReference(_repo.get(), &id, &revision));

  // Invalid workflows and a missing root are refused.
  GitRepo::GitFlowNextWorkflow invalid = workflow;
  invalid.branches[2].parent = QStringLiteral("nowhere");
  QVERIFY(!GitRepo::applyGitFlowNextWorkflow(_repo.get(), invalid));
  QCOMPARE(lastError(), GitError::GitFlowNextInvalidWorkflow);
  GitRepo::GitFlowNextWorkflow rootless = workflow;
  rootless.branches[0].name = QStringLiteral("trunk");
  rootless.branches[1].parent = QStringLiteral("trunk");
  QVERIFY(!GitRepo::applyGitFlowNextWorkflow(_repo.get(), rootless));
  QCOMPARE(lastError(), GitError::GitFlowNextFailedToLookupBranch);
  QCOMPARE(GitRepo::getWorkflowType(_repo.get()), GitRepo::WorkflowType::GitHubFlow);
}

// GitHub Flow and GitLab Flow.

void GitFlowsTest::gitHubFlow() {
  commitFile(QStringLiteral("a.txt"), "a\n", QStringLiteral("Base"));

  QVERIFY(!GitRepo::startGitHubFlowBranch(_repo.get(), QStringLiteral(" "),
                                          QStringLiteral("main")));
  QCOMPARE(lastError(), GitError::GitHubFlowBranchNameEmpty);
  QVERIFY(!GitRepo::startGitHubFlowBranch(_repo.get(), QStringLiteral("fix"),
                                          QStringLiteral("trunk")));
  QCOMPARE(lastError(), GitError::GitHubFlowFailedToLookupMainBranch);

  QVERIFY(GitRepo::startGitHubFlowBranch(_repo.get(), QStringLiteral("fix"),
                                         QStringLiteral("main")));
  QCOMPARE(currentBranch(), QStringLiteral("fix"));
  const QString fix =
      commitFile(QStringLiteral("f.txt"), "f\n", QStringLiteral("Fix"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"),
       QStringLiteral("main")});
  const QString mainTip =
      commitFile(QStringLiteral("m.txt"), "m\n", QStringLiteral("Main"));

  // Update merges main into the branch.
  QVERIFY2(GitRepo::updateGitHubFlowBranch(_repo.get(), QStringLiteral("fix"),
                                           QStringLiteral("main"), kAuthor,
                                           kEmail),
           qPrintable(lastErrorMessage()));
  QVERIFY(isAncestor(mainTip, QStringLiteral("fix")));
  QVERIFY(isAncestor(fix, QStringLiteral("fix")));
  QVERIFY(!GitRepo::updateGitHubFlowBranch(_repo.get(), QStringLiteral("main"),
                                           QStringLiteral("main"), kAuthor,
                                           kEmail));
  QCOMPARE(lastError(), GitError::GitHubFlowMainBranchNotAllowed);

  // Finish merges it into main and deletes it.
  QVERIFY2(GitRepo::finishGitHubFlowBranch(_repo.get(), QStringLiteral("fix"),
                                           QStringLiteral("main"), kAuthor,
                                           kEmail),
           qPrintable(lastErrorMessage()));
  QVERIFY(isAncestor(fix, QStringLiteral("main")));
  QCOMPARE(rev(QStringLiteral("refs/heads/fix")), QString());
  QCOMPARE(currentBranch(), QStringLiteral("main"));
  QVERIFY(!GitRepo::finishGitHubFlowBranch(_repo.get(), QStringLiteral("main"),
                                           QStringLiteral("main"), kAuthor,
                                           kEmail));
  QCOMPARE(lastError(), GitError::GitHubFlowMainBranchNotAllowed);
}

void GitFlowsTest::gitLabFlowFeatureAndRelease() {
  commitFile(QStringLiteral("a.txt"), "a\n", QStringLiteral("Base"));

  QVERIFY(GitRepo::startGitLabFlowFeature(_repo.get(), QStringLiteral("login"),
                                          QStringLiteral("main")));
  QCOMPARE(currentBranch(), QStringLiteral("feature/login"));
  const QString work =
      commitFile(QStringLiteral("l.txt"), "l\n", QStringLiteral("Login"));
  QVERIFY2(GitRepo::finishGitLabFlowFeature(_repo.get(),
                                            QStringLiteral("feature/login"),
                                            QStringLiteral("main"), kAuthor,
                                            kEmail),
           qPrintable(lastErrorMessage()));
  QVERIFY(isAncestor(work, QStringLiteral("main")));
  QCOMPARE(rev(QStringLiteral("refs/heads/feature/login")), QString());

  QVERIFY(!GitRepo::startGitLabFlowFeature(_repo.get(), QString(),
                                           QStringLiteral("main")));
  QCOMPARE(lastError(), GitError::GitLabFlowBranchNameEmpty);
  QVERIFY(!GitRepo::finishGitLabFlowFeature(_repo.get(), QStringLiteral("x"),
                                            QStringLiteral("trunk"), kAuthor,
                                            kEmail));
  QCOMPARE(lastError(), GitError::GitLabFlowFailedToLookupBaseBranch);

  const QString release = GitRepo::gitLabFlowReleaseBranchName(
      QStringLiteral("1.0"), QStringLiteral("-stable"));
  QCOMPARE(release, QStringLiteral("1-0-stable"));
  QVERIFY(GitRepo::startGitLabFlowRelease(_repo.get(), release,
                                          QStringLiteral("main")));
  QCOMPARE(currentBranch(), release);
  QCOMPARE(head(), rev(QStringLiteral("main")));
  QVERIFY(!GitRepo::startGitLabFlowRelease(_repo.get(), release,
                                           QStringLiteral("main")));
}

void GitFlowsTest::gitLabFlowPromotion() {
  commitFile(QStringLiteral("a.txt"), "a\n", QStringLiteral("Base"));
  const QStringList chain = GitRepo::gitLabFlowEnvironmentChain(
      QStringLiteral("main"), QStringLiteral("staging, production"));
  QCOMPARE(chain, (QStringList{QStringLiteral("main"), QStringLiteral("staging"),
                               QStringLiteral("production")}));

  // A missing environment branch is created from the source.
  QVERIFY2(GitRepo::promoteGitLabFlowEnvironment(_repo.get(), chain,
                                                 QStringLiteral("main"),
                                                 kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QCOMPARE(rev(QStringLiteral("staging")), rev(QStringLiteral("main")));
  QVERIFY(GitRepo::promoteGitLabFlowEnvironment(_repo.get(), chain,
                                                QStringLiteral("staging"),
                                                kAuthor, kEmail));
  QCOMPARE(rev(QStringLiteral("production")), rev(QStringLiteral("main")));

  // An existing one gets the source merged in.
  git({QStringLiteral("checkout"), QStringLiteral("-q"),
       QStringLiteral("main")});
  const QString change =
      commitFile(QStringLiteral("c.txt"), "c\n", QStringLiteral("Change"));
  QVERIFY(GitRepo::promoteGitLabFlowEnvironment(_repo.get(), chain,
                                                QStringLiteral("main"),
                                                kAuthor, kEmail));
  QVERIFY(isAncestor(change, QStringLiteral("staging")));
  QVERIFY(!isAncestor(change, QStringLiteral("production")));

  // Past the last environment, or outside the chain.
  QVERIFY(!GitRepo::promoteGitLabFlowEnvironment(_repo.get(), chain,
                                                 QStringLiteral("production"),
                                                 kAuthor, kEmail));
  QCOMPARE(lastError(), GitError::GitLabFlowPromoteNotDownstream);
  QVERIFY(!GitRepo::promoteGitLabFlowEnvironment(_repo.get(), chain,
                                                 QStringLiteral("feature"),
                                                 kAuthor, kEmail));
  QCOMPARE(lastError(), GitError::GitLabFlowPromoteNotDownstream);
}

void GitFlowsTest::gitLabFlowCherryPickToRelease() {
  commitFile(QStringLiteral("a.txt"), "base\n", QStringLiteral("Base"));
  QVERIFY(GitRepo::startGitLabFlowRelease(_repo.get(),
                                          QStringLiteral("1-0-stable"),
                                          QStringLiteral("main")));
  git({QStringLiteral("checkout"), QStringLiteral("-q"),
       QStringLiteral("main")});
  const QString fix =
      commitFile(QStringLiteral("fix.txt"), "fix\n", QStringLiteral("Fix"));

  bool conflicted = true;
  QVERIFY2(GitRepo::cherryPickToGitLabFlowRelease(
               _repo.get(), fix, QStringLiteral("1-0-stable"), kAuthor, kEmail,
               &conflicted),
           qPrintable(lastErrorMessage()));
  QVERIFY(!conflicted);
  // Back on main, the release has the fix.
  QCOMPARE(currentBranch(), QStringLiteral("main"));
  QCOMPARE(git({QStringLiteral("log"), QStringLiteral("-1"),
                QStringLiteral("--format=%s"), QStringLiteral("1-0-stable")}),
           QStringLiteral("Fix"));
  QCOMPARE(git({QStringLiteral("show"), QStringLiteral("1-0-stable:fix.txt")}),
           QStringLiteral("fix"));

  // A conflicting pick stops on the release branch.
  const QString clash =
      commitFile(QStringLiteral("a.txt"), "main\n", QStringLiteral("Clash"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"),
       QStringLiteral("1-0-stable")});
  commitFile(QStringLiteral("a.txt"), "release\n", QStringLiteral("Release"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"),
       QStringLiteral("main")});
  QVERIFY(GitRepo::cherryPickToGitLabFlowRelease(
      _repo.get(), clash, QStringLiteral("1-0-stable"), kAuthor, kEmail,
      &conflicted));
  QVERIFY(conflicted);
  QCOMPARE(currentBranch(), QStringLiteral("1-0-stable"));
  QVERIFY(GitRepo::hasConflicts(_repo.get()));
  QVERIFY(GitRepo::abortMerge(_repo.get()));

  QVERIFY(!GitRepo::cherryPickToGitLabFlowRelease(
      _repo.get(), fix, QStringLiteral("9-9-stable"), kAuthor, kEmail,
      &conflicted));
  QCOMPARE(lastError(), GitError::GitLabFlowFailedToLookupReleaseBranch);
  QVERIFY(!conflicted);
}

void GitFlowsTest::workflowTypeRoundTrip() {
  QCOMPARE(GitRepo::getWorkflowType(_repo.get()), GitRepo::WorkflowType::GitFlow);
  for (const GitRepo::WorkflowType type :
       {GitRepo::WorkflowType::GitLabFlow, GitRepo::WorkflowType::GitHubFlow,
        GitRepo::WorkflowType::GitFlowNext, GitRepo::WorkflowType::Graphite,
        GitRepo::WorkflowType::GitFlow}) {
    QVERIFY(GitRepo::setWorkflowType(_repo.get(), type));
    QCOMPARE(GitRepo::getWorkflowType(_repo.get()), type);
    QCOMPARE(git({QStringLiteral("config"), QStringLiteral("sourcerer.workflow")}),
             GitRepo::workflowTypeToString(type));
  }
  // Unknown values written by hand mean Git Flow.
  git({QStringLiteral("config"), QStringLiteral("sourcerer.workflow"),
       QStringLiteral("trunk-based")});
  reopen();
  QCOMPARE(GitRepo::getWorkflowType(_repo.get()), GitRepo::WorkflowType::GitFlow);
  QCOMPARE(GitRepo::getWorkflowType(nullptr), GitRepo::WorkflowType::GitFlow);
}

QTEST_GUILESS_MAIN(GitFlowsTest)

#include "tst_gitflows.moc"
