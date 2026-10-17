// Tests for lib/GitRepo, see docs/GIT_TEST_PLAN.md. Repositories are created
// with GitRepo::initRepository and put into the states under test with the
// git executable. The git configuration of the user running the tests is not
// used, see GitTest::IsolatedConfig.

#include "GitTestHelpers.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>
#include <thread>

using namespace GitTest;
using GitRepo::GitError;

Q_DECLARE_METATYPE(GitRepo::GitError)
Q_DECLARE_METATYPE(GitRepo::ResetType)
Q_DECLARE_METATYPE(GitRepo::BranchType)
Q_DECLARE_METATYPE(GitRepo::BisectAction)

class GitRepoTest : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void init();
  void cleanup();

  // Repository lifecycle.
  void initCreatesInitialCommitOnDefaultBranch_data();
  void initCreatesInitialCommitOnDefaultBranch();
  void initRefusesExistingRepository();
  void initIntoUnusualPaths_data();
  void initIntoUnusualPaths();
  void initHonoursGlobalDefaultBranch();
  void initWithoutParentDirectoryFails();
  void initWithoutAuthorFails();
  void openRejectsNonRepository_data();
  void openRejectsNonRepository();
  void openPathSpellings();
  void openUnbornHeadCreatesInitialCommit();
  void openBareRepository();
  void openLinkedWorktreeAndGitFile();
  void closeNullIsSafe();
  void lastErrorIsPerThread();

  // Status.
  void statusCategories();
  void statusTypeChange();
  void statusUntrackedDirectories();
  void statusIgnorePatterns();
  void statusLineEndings_data();
  void statusLineEndings();
  void statusUnusualFileNames_data();
  void statusUnusualFileNames();
  void statusLongPaths();
  void statusCaseOnlyRename();
  void statusCancelled();
  void statusManyFiles();
  void statusSubmoduleWithNewCommit();

  // Staging.
  void stageUnstageRoundTrip();
  void stagePathSpellings_data();
  void stagePathSpellings();
  void stageDirectory();
  void stageDeletedAndMissingFiles();
  void stagePartiallyStagedFile();
  void stageIgnoredFile();
  void stageNothing();
  void stageResolvesConflict();
  void stageNormalizesLineEndings();
  void unstageNewFileMakesItUntracked();

  // Commit and amend.
  void commitOnlyStagedFiles();
  void commitWithoutStagedFiles();
  void commitMessageComposition_data();
  void commitMessageComposition();
  void commitSignoffIsNotDuplicated();
  void commitWithoutAuthorFails();
  void commitMergeHasTwoParents();
  void commitWithoutCleanupKeepsMergeState();
  void commitOnDetachedHead();
  void amendMessageOnly();
  void amendWithStagedFiles();
  void amendKeepsAuthorAndParents();
  void amendRootCommit();

  // Discard, reset, checkout and patches.
  void discardModifiedAndDeletedFiles();
  void discardStagedChanges();
  void discardStagedNewFileKeepsIt();
  void discardUnusualPaths();
  void resetTypes_data();
  void resetTypes();
  void resetToUnknownRevisionFails();
  void resetClearsMergeState();
  void checkoutCommitDetaches();
  void checkoutCommitRefusesToOverwrite();
  void checkoutCommitInvalidIds_data();
  void checkoutCommitInvalidIds();
  void patchRoundTrip();
  void patchAddsAndDeletesFiles();
  void patchWithoutNewlineAtEnd();
  void patchContextMismatchLeavesFilesAlone();
  void patchGarbageFails_data();
  void patchGarbageFails();
  void patchOfUnknownCommit();

  // Diff, blame and file sizes.
  void diffModifiedAndNewFiles();
  void diffDeletedFile();
  void diffCommittedEmptyFile();
  void diffBinaryFile();
  void diffNoNewlineAtEnd();
  void diffUnicodeAndCrlf();
  void diffRespectsMaxFileSize();
  void diffOnlyRequestedFiles();
  void diffFromCommit();
  void diffFromRootAndMergeCommits();
  void diffFromInvalidCommit_data();
  void diffFromInvalidCommit();
  void blameCommittedAndUncommittedLines();
  void blameMultipleAuthors();
  void blameEdgeCases();
  void fileSizes();

  // History.
  void historyPaging();
  void historyAllBranches();
  void historyRefTipsAndLabels();
  void historyBranchTypes();
  void historyPageZeroRefreshesCursor();
  void historyCancelled();
  void historyMergeParents();
  void commitStatusFields();
  void commitStatusRootMergeAndRename();
  void commitStatusOfUnknownCommit_data();
  void commitStatusOfUnknownCommit();
  void commitBranchesTagsAndHeads();

  // Branches.
  void createBranchFromCommit();
  void createBranchUpstream();
  void createBranchInvalidNames_data();
  void createBranchInvalidNames();
  void branchNamesWithSlashesAndUnicode_data();
  void branchNamesWithSlashesAndUnicode();
  void checkoutLocalBranch();
  void checkoutRemoteBranchCreatesTracking();
  void checkoutNestedRemoteBranch();
  void checkoutRemoteBranchWhenLocalExists();
  void checkoutTagDetaches();
  void checkoutMissingBranchFails_data();
  void checkoutMissingBranchFails();
  void checkoutWithDirtyWorkdir();
  void renameBranch();
  void renameBranchInvalidNames_data();
  void renameBranchInvalidNames();
  void deleteBranch();
  void activeBranchQueries();
  void listLocalAndRemoteBranches();
  void remoteBranchStatus();

  // Tags.
  void createTagIsAnnotated();
  void createTagFailures_data();
  void createTagFailures();
  void renameTagKeepsKind_data();
  void renameTagKeepsKind();
  void renameTagFailures();
  void deleteTag();
  void listTags();

  // Merge, squash, rebase, cherry-pick and revert.
  void mergeFastForward();
  void mergeFastForwardKeepsLocalChanges();
  void mergeCreatesMergeCommit();
  void mergeAlreadyUpToDate();
  void mergeUnrelatedHistories();
  void mergeConflictAndAbort();
  void mergeConflictResolveAndCommit();
  void mergeConflictKinds_data();
  void mergeConflictKinds();
  void mergeWithDirtyWorkdirFails();
  void mergeIntoOtherBranchChecksItOut();
  void squashBranches();
  void squashInvalidArguments_data();
  void squashInvalidArguments();
  void squashBusyRepository();
  void squashConflicts();
  void squashNothingToSquash();
  void rebaseLinear();
  void rebaseAlreadyUpToDate();
  void rebaseConflictAbortRestores();
  void rebaseConflictsAtSeveralSteps();
  void rebaseResolvedToBaseDropsStep();
  void rebaseFlattensMergeCommits();
  void rebaseWithDirtyWorkdirFails();
  void rebaseInProgressBlocksOtherOperations();
  void cherryPick();
  void cherryPickConflict();
  void cherryPickMergeCommitFails();
  void cherryPickAlreadyApplied();
  void revertCommit();
  void revertConflict();
  void revertRootCommit();
  void pickAndRevertUnknownCommits_data();
  void pickAndRevertUnknownCommits();

  // Stashes.
  void stashAndList();
  void stashWithoutIdentityFails();
  void stashIncludesUntrackedFiles();
  void stashCleanWorkdirFails();
  void applyStashKeepsEntry();
  void applyStashConflict();
  void applyStashWithDirtyWorkdir();
  void stashInvalidIndexes_data();
  void stashInvalidIndexes();
  void deleteStashReindexes();
  void deleteAllStashes();
  void stashStagedAndUnstagedSameFile();
  void stashBinaryFile();

  // Worktrees.
  void addWorktreeNewAndExistingBranch();
  void addWorktreeFailures_data();
  void addWorktreeFailures();
  void addWorktreeFromBaseBranch();
  void addWorktreeUnusualPaths_data();
  void addWorktreeUnusualPaths();
  void listWorktreesFields();
  void listWorktreesMissingDirectory();
  void lockAndUnlockWorktree();
  void removeWorktree();
  void removeDirtyWorktreeNeedsForce();
  void commitInWorktreeVisibleInMain();

  // Bisect.
  void bisectFindsFirstBadCommit();
  void bisectWaitingStates_data();
  void bisectWaitingStates();
  void bisectSkipAmbiguous();
  void bisectInvalidRange();
  void bisectResetRestoresHead();
  void bisectBusyRepository();
  void bisectInvalidArguments_data();
  void bisectInvalidArguments();
  void bisectCliCompatibility();
  void bisectMarksInHistory();

  // Remotes, targets and upstreams.
  void addRemoteWithPushUrl();
  void addRemoteNames_data();
  void addRemoteNames();
  void listRemotesSorted();
  void defaultRemoteFallbacks();
  void setDefaultRemoteMissingFails();
  void renameRemoteMovesRefsAndUpstream();
  void updateRemoteUrlsOnly();
  void removeRemote();
  void targetsAddEditDeleteDefault();
  void legacyTargetsAreMigrated();
  void upstreamSetAndGet();
  void upstreamAdversarialConfig();
  void upstreamFromLinkedWorktree();
  void configEditsPreserveComments();

  // Test hooks.
  void commercialPlanSwitch();

  // Existing scenarios.
  void defaultRemote();
  void commitRebaseContinuesAfterConflict();

private:
  QString git(const QStringList &args, bool *ok = nullptr) const {
    return gitIn(_config, _root, args, ok);
  }
  QString file(const QString &name) const { return _root + '/' + name; }
  QString head() const {
    return git({QStringLiteral("rev-parse"), QStringLiteral("HEAD")});
  }
  // Stages every change with GitRepo and commits it, returns the new HEAD.
  QString commitAll(const QString &summary);
  // Same with the git executable, for setting up states.
  QString gitCommitAll(const QString &summary) const;
  GitRepo::GitStatus status() { return GitRepo::getStatus(_repo.get(), _quit); }
  void reopen();
  // main and feature both change a.txt, `git merge feature` stops.
  void makeMergeConflict();
  // main and feature both change a.txt, main is checked out.
  void makeConflictingBranches();
  // Full id of a revision, empty if it doesn't resolve.
  QString revParse(const QString &revision) const;
  // Short names of the local branches, sorted.
  QStringList localBranches() const;
  // A commit on top of `parent` with the same tree, made without touching the
  // working copy.
  QString emptyCommit(const QString &parent, const QString &summary) const;
  // A commit on a detached HEAD that writes `name`, back on the branch
  // afterwards. Only the returned id refers to it.
  QString detachedCommit(const QString &name, const QByteArray &content,
                         const QString &summary);
  // gitCommitAll with kOtherAuthor as the author.
  QString gitCommitAllAsOther(const QString &summary) const;
  // Commits C0..C<count - 1>, each writing its number to v.txt. Oldest first.
  QStringList makeLinearHistory(int count);
  // user.name and user.email in the repository configuration, for stashes.
  void setIdentity() const;
  QStringList remoteNames() const;
  GitRepo::RemoteInfo remote(const QString &name) const;
  // "name|fetch|push" per target, separated by ';'.
  static QString targets(const GitRepo::RemoteInfo &remote);

  IsolatedConfig _config;
  std::unique_ptr<QTemporaryDir> _tmp;
  QString _base;
  QString _root;
  RepoPtr _repo;
  bool _quit = false;
};

QString GitRepoTest::commitAll(const QString &summary) {
  const GitRepo::GitStatus st = status();
  const QStringList files =
      st.newFiles + st.modifiedFiles + st.deletedFiles + st.typeChangedFiles;
  if (!files.isEmpty() && !GitRepo::stageFiles(_repo.get(), files)) {
    qWarning() << "stageFiles failed:" << lastErrorMessage();
    return {};
  }
  if (!GitRepo::commitStaged(_repo.get(), summary, QString(), kAuthor, kEmail,
                             true, false, false)) {
    qWarning() << "commitStaged failed:" << lastErrorMessage();
    return {};
  }
  return head();
}

QString GitRepoTest::gitCommitAll(const QString &summary) const {
  git({QStringLiteral("add"), QStringLiteral("-A")});
  git({QStringLiteral("commit"), QStringLiteral("-q"),
       QStringLiteral("--allow-empty"), QStringLiteral("-m"), summary});
  return head();
}

void GitRepoTest::reopen() {
  _repo.reset();
  _repo.reset(GitRepo::openRepository(_root, kAuthor, kEmail));
  QVERIFY2(_repo, qPrintable(lastErrorMessage()));
}

void GitRepoTest::makeMergeConflict() {
  makeConflictingBranches();
  bool ok = true;
  git({QStringLiteral("merge"), QStringLiteral("-q"),
       QStringLiteral("feature")},
      &ok);
  QVERIFY(!ok);
}

void GitRepoTest::makeConflictingBranches() {
  QVERIFY(writeFile(file("a.txt"), "base\n"));
  gitCommitAll(QStringLiteral("Base"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("feature")});
  QVERIFY(writeFile(file("a.txt"), "feature\n"));
  gitCommitAll(QStringLiteral("Feature"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"),
       QStringLiteral("main")});
  QVERIFY(writeFile(file("a.txt"), "main\n"));
  gitCommitAll(QStringLiteral("Main"));
}

QString GitRepoTest::revParse(const QString &revision) const {
  bool ok = false;
  const QString id = git({QStringLiteral("rev-parse"), QStringLiteral("--verify"),
                          QStringLiteral("-q"), revision},
                         &ok);
  return ok ? id : QString();
}

QStringList GitRepoTest::localBranches() const {
  return git({QStringLiteral("for-each-ref"),
              QStringLiteral("--format=%(refname:short)"),
              QStringLiteral("refs/heads/")})
      .split('\n', Qt::SkipEmptyParts);
}

QString GitRepoTest::emptyCommit(const QString &parent,
                                 const QString &summary) const {
  return git({QStringLiteral("commit-tree"),
              revParse(parent + QStringLiteral("^{tree}")), QStringLiteral("-p"),
              parent, QStringLiteral("-m"), summary});
}

QString GitRepoTest::detachedCommit(const QString &name,
                                    const QByteArray &content,
                                    const QString &summary) {
  git({QStringLiteral("checkout"), QStringLiteral("-q"),
       QStringLiteral("--detach")});
  if (!writeFile(file(name), content)) {
    return {};
  }
  const QString id = gitCommitAll(summary);
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-")});
  return id;
}

QString GitRepoTest::gitCommitAllAsOther(const QString &summary) const {
  git({QStringLiteral("add"), QStringLiteral("-A")});
  git({QStringLiteral("commit"), QStringLiteral("-q"),
       QStringLiteral("--allow-empty"),
       QStringLiteral("--author=%1 <%2>").arg(kOtherAuthor, kOtherEmail),
       QStringLiteral("-m"), summary});
  return head();
}

QStringList GitRepoTest::makeLinearHistory(int count) {
  QStringList commits;
  for (int i = 0; i < count; ++i) {
    if (!writeFile(file("v.txt"), QByteArray::number(i) + '\n')) {
      return {};
    }
    commits += gitCommitAll(QStringLiteral("C%1").arg(i));
  }
  return commits;
}

void GitRepoTest::setIdentity() const {
  git({QStringLiteral("config"), QStringLiteral("user.name"), kAuthor});
  git({QStringLiteral("config"), QStringLiteral("user.email"), kEmail});
}

QStringList GitRepoTest::remoteNames() const {
  QStringList names;
  for (const GitRepo::RemoteInfo &info : GitRepo::listRemotes(_repo.get())) {
    names += info.name;
  }
  return names;
}

GitRepo::RemoteInfo GitRepoTest::remote(const QString &name) const {
  for (const GitRepo::RemoteInfo &info : GitRepo::listRemotes(_repo.get())) {
    if (info.name == name) {
      return info;
    }
  }
  return {};
}

QString GitRepoTest::targets(const GitRepo::RemoteInfo &remote) {
  QStringList result;
  for (const GitRepo::RemoteTarget &target : remote.targets) {
    result += target.name + '|' + target.fetchUrl + '|' + target.pushUrl;
  }
  return result.join(';');
}

void GitRepoTest::initTestCase() {
  if (QStandardPaths::findExecutable(QStringLiteral("git")).isEmpty()) {
    QSKIP("git was not found");
  }
  QStandardPaths::setTestModeEnabled(true);
  GitRepo::initForCurrentThread();
}

void GitRepoTest::init() {
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

void GitRepoTest::cleanup() {
  _repo.reset();
  if (_tmp) {
    makeWritable(_tmp->path());
  }
  _tmp.reset();
  _quit = false;
}

// Repository lifecycle.

void GitRepoTest::initCreatesInitialCommitOnDefaultBranch_data() {
  QTest::addColumn<QString>("branch");
  QTest::newRow("main") << QStringLiteral("main");
  QTest::newRow("trunk") << QStringLiteral("trunk");
  QTest::newRow("slash") << QStringLiteral("feature/x");
  QTest::newRow("unicode") << QString::fromUtf16(u"ветка");
}

void GitRepoTest::initCreatesInitialCommitOnDefaultBranch() {
  QFETCH(QString, branch);

  QVERIFY2(GitRepo::initRepository(QStringLiteral("other"), _base, branch,
                                   kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  const QString dir = _base + QStringLiteral("/other");
  QVERIFY(QFileInfo(dir + QStringLiteral("/.git")).isDir());
  QCOMPARE(gitIn(_config, dir, {QStringLiteral("symbolic-ref"),
                                QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/") + branch);
  QCOMPARE(gitIn(_config, dir, {QStringLiteral("log"),
                                QStringLiteral("--format=%s|%an|%ae")}),
           QStringLiteral("Initial commit|Tester|tester@example.com"));
  // The initial commit is empty and nothing else is left over.
  QCOMPARE(gitIn(_config, dir, {QStringLiteral("ls-tree"),
                                QStringLiteral("HEAD")}),
           QString());
  QCOMPARE(gitIn(_config, dir,
                 {QStringLiteral("branch"), QStringLiteral("--format=%(refname:short)")}),
           branch);

  RepoPtr repo(GitRepo::openRepository(dir, kAuthor, kEmail));
  QVERIFY(repo);
  QVERIFY(!GitRepo::getStatus(repo.get(), _quit).hasChanges());
  QCOMPARE(GitRepo::getActiveBranch(repo.get(), GitRepo::BranchType::Local,
                                    _quit),
           branch);
}

void GitRepoTest::initRefusesExistingRepository() {
  const QString before = head();
  QVERIFY(!GitRepo::initRepository(QStringLiteral("repo"), _base,
                                   QStringLiteral("other"), kAuthor, kEmail));
  QCOMPARE(lastError(), GitError::InitRepositoryFailedToInitializeRepository);
  // The existing repository is untouched.
  QCOMPARE(head(), before);
  QCOMPARE(git({QStringLiteral("branch"),
                QStringLiteral("--format=%(refname:short)")}),
           QStringLiteral("main"));
}

void GitRepoTest::initIntoUnusualPaths_data() {
  QTest::addColumn<QString>("name");
  QTest::newRow("spaces") << QStringLiteral("my repo");
  QTest::newRow("unicode") << QString::fromUtf16(u"répo ünï");
  // Beyond MAX_PATH on Windows once the files inside are counted.
  QTest::newRow("long") << QStringLiteral("d").repeated(120) + '/' +
                               QStringLiteral("e").repeated(100);
}

void GitRepoTest::initIntoUnusualPaths() {
  QFETCH(QString, name);

  // Only the repository directory itself is created.
  const QString dir = _base + '/' + name;
  QVERIFY(QDir().mkpath(QFileInfo(dir).absolutePath()));
  const bool created = GitRepo::initRepository(name, _base,
                                               QStringLiteral("main"), kAuthor,
                                               kEmail);
#ifdef Q_OS_WIN
  QEXPECT_FAIL("long", "libgit2 refuses a .git path longer than MAX_PATH",
               Abort);
#endif
  QVERIFY2(created, qPrintable(lastErrorMessage()));
  RepoPtr repo(GitRepo::openRepository(dir, kAuthor, kEmail));
  QVERIFY2(repo, qPrintable(lastErrorMessage()));

  QVERIFY(writeFile(dir + QStringLiteral("/sub dir/file.txt"), "x\n"));
  QCOMPARE(GitRepo::getStatus(repo.get(), _quit).newFiles,
           QStringList{QStringLiteral("sub dir/file.txt")});
  QVERIFY2(GitRepo::stageFiles(repo.get(), {QStringLiteral("sub dir/file.txt")}),
           qPrintable(lastErrorMessage()));
  QVERIFY2(GitRepo::commitStaged(repo.get(), QStringLiteral("Add"), QString(),
                                 kAuthor, kEmail, true, false, false),
           qPrintable(lastErrorMessage()));
  QVERIFY(!GitRepo::getStatus(repo.get(), _quit).hasChanges());
}

void GitRepoTest::initHonoursGlobalDefaultBranch() {
  // libgit2 names the first branch after init.defaultBranch, the initial
  // commit must still end up on the requested branch.
  QVERIFY(writeFile(_config.globalConfig(),
                    "[init]\n\tdefaultBranch = develop\n"));
  const bool created = GitRepo::initRepository(
      QStringLiteral("other"), _base, QStringLiteral("main"), kAuthor, kEmail);
  QVERIFY2(created, qPrintable(lastErrorMessage()));
  QCOMPARE(gitIn(_config, _base + QStringLiteral("/other"),
                 {QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/main"));
}

void GitRepoTest::initWithoutParentDirectoryFails() {
  // GIT_REPOSITORY_INIT_MKDIR creates the last directory only.
  QVERIFY(!GitRepo::initRepository(QStringLiteral("a/b/c"), _base,
                                   QStringLiteral("main"), kAuthor, kEmail));
  QCOMPARE(lastError(), GitError::InitRepositoryFailedToInitializeRepository);
  QVERIFY(!QFileInfo::exists(_base + QStringLiteral("/a")));
}

void GitRepoTest::initWithoutAuthorFails() {
  QVERIFY(!GitRepo::initRepository(QStringLiteral("other"), _base,
                                   QStringLiteral("main"), QString(), kEmail));
  QCOMPARE(lastError(), GitError::CreateInitialCommitFailedToCreateSignature);
  QVERIFY(!GitRepo::initRepository(QStringLiteral("other2"), _base,
                                   QStringLiteral("main"), kAuthor, QString()));
  QCOMPARE(lastError(), GitError::CreateInitialCommitFailedToCreateSignature);

  // A failed init should not leave a half made repository behind, otherwise
  // trying again with an author fails because the repository exists.
  const bool retried = GitRepo::initRepository(
      QStringLiteral("other"), _base, QStringLiteral("main"), kAuthor, kEmail);
  QVERIFY2(retried, qPrintable(lastErrorMessage()));
}

void GitRepoTest::openRejectsNonRepository_data() {
  QTest::addColumn<QString>("kind");
  QTest::newRow("empty directory") << QStringLiteral("empty");
  QTest::newRow("missing path") << QStringLiteral("missing");
  QTest::newRow("file") << QStringLiteral("file");
  QTest::newRow("empty path") << QStringLiteral("blank");
}

void GitRepoTest::openRejectsNonRepository() {
  QFETCH(QString, kind);

  QString path = _base + '/' + kind;
  if (kind == QStringLiteral("empty")) {
    QVERIFY(QDir().mkpath(path));
  } else if (kind == QStringLiteral("file")) {
    QVERIFY(writeFile(path, "x"));
  } else if (kind == QStringLiteral("blank")) {
    path.clear();
  }

  RepoPtr repo(GitRepo::openRepository(path, kAuthor, kEmail));
  QVERIFY(!repo);
  QCOMPARE(lastError(), GitError::OpenRepositoryFailedToOpenRepository);
  QVERIFY(!lastErrorMessage().isEmpty());
}

void GitRepoTest::openPathSpellings() {
  // libgit2 does not search parent directories, unlike the git executable.
  QVERIFY(QDir().mkpath(file("sub")));
  RepoPtr sub(GitRepo::openRepository(file("sub"), kAuthor, kEmail));
  QVERIFY(!sub);
  QCOMPARE(lastError(), GitError::OpenRepositoryFailedToOpenRepository);

  for (const QString &spelling :
       {_root + '/', QDir::toNativeSeparators(_root),
        QDir::toNativeSeparators(_root) + '\\', _root + QStringLiteral("/.git"),
        _root + QStringLiteral("/sub/..")}) {
    RepoPtr repo(GitRepo::openRepository(spelling, kAuthor, kEmail));
    QVERIFY2(repo, qPrintable(spelling));
    QCOMPARE(QDir::cleanPath(QString::fromUtf8(git_repository_workdir(repo.get()))),
             _root);
  }
}

void GitRepoTest::openUnbornHeadCreatesInitialCommit() {
  const QString dir = _base + QStringLiteral("/fresh");
  QVERIFY(QDir().mkpath(dir));
  bool ok = false;
  gitIn(_config, dir, {QStringLiteral("init"), QStringLiteral("-q")}, &ok);
  QVERIFY(ok);
  // Files that are already there are not committed.
  QVERIFY(writeFile(dir + QStringLiteral("/existing.txt"), "x\n"));

  {
    RepoPtr repo(GitRepo::openRepository(dir, kAuthor, kEmail));
    QVERIFY(repo);
    QCOMPARE(GitRepo::getStatus(repo.get(), _quit).newFiles,
             QStringList{QStringLiteral("existing.txt")});
  }
  QCOMPARE(gitIn(_config, dir, {QStringLiteral("log"),
                                QStringLiteral("--format=%s|%an")}),
           QStringLiteral("Initial commit|Tester"));
  QCOMPARE(gitIn(_config, dir, {QStringLiteral("ls-tree"),
                                QStringLiteral("HEAD")}),
           QString());

  // Opening again does not add another commit.
  RepoPtr again(GitRepo::openRepository(dir, kAuthor, kEmail));
  QVERIFY(again);
  QCOMPARE(gitIn(_config, dir, {QStringLiteral("rev-list"),
                                QStringLiteral("--count"),
                                QStringLiteral("HEAD")}),
           QStringLiteral("1"));

  // Without an author the repository still opens, just without the commit.
  const QString dir2 = _base + QStringLiteral("/fresh2");
  QVERIFY(QDir().mkpath(dir2));
  gitIn(_config, dir2, {QStringLiteral("init"), QStringLiteral("-q")}, &ok);
  QVERIFY(ok);
  RepoPtr anonymous(GitRepo::openRepository(dir2, QString(), QString()));
  QVERIFY(anonymous);
  QVERIFY(git_repository_head_unborn(anonymous.get()) == 1);
}

void GitRepoTest::openBareRepository() {
  const QString dir = _base + QStringLiteral("/bare.git");
  bool ok = false;
  gitIn(_config, _base,
        {QStringLiteral("init"), QStringLiteral("-q"), QStringLiteral("--bare"),
         dir},
        &ok);
  QVERIFY(ok);

  RepoPtr repo(GitRepo::openRepository(dir, kAuthor, kEmail));
  QVERIFY(repo);
  QVERIFY(git_repository_is_bare(repo.get()));
  // A bare repository has no working copy to report on.
  const GitRepo::GitStatus st = GitRepo::getStatus(repo.get(), _quit);
  QVERIFY(!st.hasChanges());
  QCOMPARE(lastError(), GitError::GetStatusFailedToCreateStatusList);
}

void GitRepoTest::openLinkedWorktreeAndGitFile() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("A"));

  const QString worktree = _base + QStringLiteral("/linked");
  git({QStringLiteral("worktree"), QStringLiteral("add"), QStringLiteral("-q"),
       QStringLiteral("-b"), QStringLiteral("side"), worktree});
  QVERIFY(QFileInfo(worktree + QStringLiteral("/.git")).isFile());

  RepoPtr linked(GitRepo::openRepository(worktree, kAuthor, kEmail));
  QVERIFY2(linked, qPrintable(lastErrorMessage()));
  QVERIFY(git_repository_is_worktree(linked.get()));
  QVERIFY(!GitRepo::getStatus(linked.get(), _quit).hasChanges());
  QCOMPARE(GitRepo::getActiveBranch(linked.get(), GitRepo::BranchType::Local,
                                    _quit),
           QStringLiteral("side"));

  // A working copy whose .git is a file pointing elsewhere.
  const QString separate = _base + QStringLiteral("/separate");
  const QString gitDir = _base + QStringLiteral("/separate-git");
  bool ok = false;
  gitIn(_config, _base,
        {QStringLiteral("init"), QStringLiteral("-q"),
         QStringLiteral("--separate-git-dir"), gitDir, separate},
        &ok);
  QVERIFY(ok);
  RepoPtr repo(GitRepo::openRepository(separate, kAuthor, kEmail));
  QVERIFY2(repo, qPrintable(lastErrorMessage()));
  QVERIFY(writeFile(separate + QStringLiteral("/b.txt"), "b\n"));
  QCOMPARE(GitRepo::getStatus(repo.get(), _quit).newFiles,
           QStringList{QStringLiteral("b.txt")});
}

void GitRepoTest::closeNullIsSafe() { GitRepo::closeRepository(nullptr); }

void GitRepoTest::lastErrorIsPerThread() {
  RepoPtr missing(GitRepo::openRepository(_base + QStringLiteral("/missing"),
                                          kAuthor, kEmail));
  QVERIFY(!missing);
  QCOMPARE(lastError(), GitError::OpenRepositoryFailedToOpenRepository);

  GitError otherThreadError = GitError::Last;
  GitError otherThreadAfterFailure = GitError::Last;
  std::thread worker([&] {
    GitRepo::initForCurrentThread();
    otherThreadError = GitRepo::getLastError().error;
    RepoPtr repo(GitRepo::openRepository(_root, kAuthor, kEmail));
    GitRepo::resetToCommit(repo.get(), QStringLiteral("no-such-revision"),
                           GitRepo::ResetType::Hard);
    otherThreadAfterFailure = GitRepo::getLastError().error;
    repo.reset();
    GitRepo::shutdownForCurrentThread();
  });
  worker.join();

  QCOMPARE(otherThreadError, GitError::Unknown);
  QCOMPARE(otherThreadAfterFailure,
           GitError::ResetToCommitFailedToResolveRevision);
  // The error of this thread is unchanged.
  QCOMPARE(lastError(), GitError::OpenRepositoryFailedToOpenRepository);
  // libgit2 is still usable here after the other thread shut down.
  QVERIFY(!status().hasChanges());
}

// Status.

void GitRepoTest::statusCategories() {
  QVERIFY(writeFile(file("modified.txt"), "m\n"));
  QVERIFY(writeFile(file("deleted.txt"), "d\n"));
  QVERIFY(writeFile(file("staged-modified.txt"), "sm\n"));
  QVERIFY(writeFile(file("staged-deleted.txt"), "sd\n"));
  QVERIFY(writeFile(file("renamed-old.txt"), "rename me\nwith content\n"));
  QVERIFY(writeFile(file(".gitignore"), "*.log\n"));
  gitCommitAll(QStringLiteral("Base"));

  QVERIFY(writeFile(file("new.txt"), "n\n"));
  QVERIFY(writeFile(file("modified.txt"), "m2\n"));
  QVERIFY(QFile::remove(file("deleted.txt")));
  QVERIFY(writeFile(file("staged-new.txt"), "sn\n"));
  QVERIFY(writeFile(file("staged-modified.txt"), "sm2\n"));
  QVERIFY(writeFile(file("ignored.log"), "i\n"));
  git({QStringLiteral("add"), QStringLiteral("staged-new.txt"),
       QStringLiteral("staged-modified.txt")});
  git({QStringLiteral("rm"), QStringLiteral("-q"),
       QStringLiteral("staged-deleted.txt")});
  git({QStringLiteral("mv"), QStringLiteral("renamed-old.txt"),
       QStringLiteral("renamed-new.txt")});

  const GitRepo::GitStatus st = status();
  QCOMPARE(st.newFiles, QStringList{QStringLiteral("new.txt")});
  QCOMPARE(st.modifiedFiles, QStringList{QStringLiteral("modified.txt")});
  QCOMPARE(st.deletedFiles, QStringList{QStringLiteral("deleted.txt")});
  QCOMPARE(st.newStagedFiles, QStringList{QStringLiteral("staged-new.txt")});
  QCOMPARE(st.modifiedStagedFiles,
           QStringList{QStringLiteral("staged-modified.txt")});
  QCOMPARE(st.deletedStagedFiles,
           QStringList{QStringLiteral("staged-deleted.txt")});
  QCOMPARE(st.renamedStagedFiles,
           QStringList{QStringLiteral("renamed-new.txt")});
  QCOMPARE(st.ignoredFiles, QStringList{QStringLiteral("ignored.log")});
  QVERIFY(st.conflictedFiles.isEmpty());
  QVERIFY(st.hasChanges());

  // Only ignored files is not a change.
  git({QStringLiteral("reset"), QStringLiteral("-q"), QStringLiteral("--hard")});
  git({QStringLiteral("clean"), QStringLiteral("-fdq")});
  const GitRepo::GitStatus clean = status();
  QCOMPARE(clean.ignoredFiles, QStringList{QStringLiteral("ignored.log")});
  QVERIFY(!clean.hasChanges());

  // Conflicts.
  makeMergeConflict();
  const GitRepo::GitStatus conflicted = status();
  QCOMPARE(conflicted.conflictedFiles, QStringList{QStringLiteral("a.txt")});
  QVERIFY(conflicted.hasChanges());
  QVERIFY(GitRepo::hasConflicts(_repo.get()));
  QVERIFY(GitRepo::getIsMergeInProgress(_repo.get()));
}

void GitRepoTest::statusTypeChange() {
#ifdef Q_OS_WIN
  QSKIP("Symbolic links need extra privileges on Windows");
#else
  QVERIFY(writeFile(file("target.txt"), "t\n"));
  QVERIFY(writeFile(file("link"), "plain file\n"));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY(QFile::remove(file("link")));
  QVERIFY(QFile::link(QStringLiteral("target.txt"), file("link")));
  QCOMPARE(status().typeChangedFiles, QStringList{QStringLiteral("link")});
  git({QStringLiteral("add"), QStringLiteral("link")});
  QCOMPARE(status().typeChangedStagedFiles, QStringList{QStringLiteral("link")});
#endif
}

void GitRepoTest::statusUntrackedDirectories() {
  QVERIFY(writeFile(file("dir/a.txt"), "a\n"));
  QVERIFY(writeFile(file("dir/deeper/b.txt"), "b\n"));
  QVERIFY(QDir().mkpath(file("empty/nested")));

  // Untracked directories are listed file by file, empty ones not at all.
  QCOMPARE(status().newFiles,
           (QStringList{QStringLiteral("dir/a.txt"),
                        QStringLiteral("dir/deeper/b.txt")}));
}

void GitRepoTest::statusIgnorePatterns() {
  QVERIFY(writeFile(file(".gitignore"), "*.log\n!keep.log\nbuild/\n"));
  QVERIFY(writeFile(file("drop.log"), "x"));
  QVERIFY(writeFile(file("keep.log"), "x"));
  QVERIFY(writeFile(file("build/out.o"), "x"));
  QVERIFY(writeFile(file("src/build.txt"), "x"));
  QVERIFY(writeFile(file(".git/info/exclude"), "secret.txt\n"));
  QVERIFY(writeFile(file("secret.txt"), "x"));

  const GitRepo::GitStatus st = status();
  QCOMPARE(st.newFiles,
           (QStringList{QStringLiteral(".gitignore"), QStringLiteral("keep.log"),
                        QStringLiteral("src/build.txt")}));
  QVERIFY(st.ignoredFiles.contains(QStringLiteral("drop.log")));
  QVERIFY(st.ignoredFiles.contains(QStringLiteral("secret.txt")));
  QVERIFY(st.ignoredFiles.contains(QStringLiteral("build")) ||
          st.ignoredFiles.contains(QStringLiteral("build/out.o")));
}

void GitRepoTest::statusLineEndings_data() {
  QTest::addColumn<QString>("autocrlf");
  QTest::addColumn<QByteArray>("attributes");
  QTest::addColumn<bool>("changedLines");

  QTest::newRow("autocrlf false") << QStringLiteral("false") << QByteArray()
                                  << true;
  QTest::newRow("autocrlf true") << QStringLiteral("true") << QByteArray()
                                 << false;
  QTest::newRow("autocrlf input") << QStringLiteral("input") << QByteArray()
                                  << false;
  QTest::newRow("text=auto") << QStringLiteral("false")
                             << QByteArray("* text=auto\n") << false;
  QTest::newRow("-text") << QStringLiteral("true") << QByteArray("* -text\n")
                         << true;
}

void GitRepoTest::statusLineEndings() {
  QFETCH(QString, autocrlf);
  QFETCH(QByteArray, attributes);
  QFETCH(bool, changedLines);

  if (!attributes.isEmpty()) {
    QVERIFY(writeFile(file(".gitattributes"), attributes));
  }
  QVERIFY(writeFile(file("text.txt"), "one\ntwo\n"));
  gitCommitAll(QStringLiteral("LF"));
  git({QStringLiteral("config"), QStringLiteral("core.autocrlf"), autocrlf});
  reopen();

  // Only the line endings change in the working copy. Like `git status`, the
  // file is listed as modified (its size changed), but with conversion there
  // is nothing to show in the diff.
  QVERIFY(writeFile(file("text.txt"), "one\r\ntwo\r\n"));
  QCOMPARE(status().modifiedFiles, QStringList{QStringLiteral("text.txt")});
  const GitRepo::FilesDiff diff =
      GitRepo::diffFiles(_repo.get(), {QStringLiteral("text.txt")});
  QVERIFY(diff.valid);
  QStringList lines;
  for (const GitRepo::FilesDiffDelta &delta : diff.deltas) {
    lines += GitTest::changedLines(delta);
  }
  QCOMPARE(!lines.isEmpty(), changedLines);

  // A real change is always seen.
  QVERIFY(writeFile(file("text.txt"), "one\r\nthree\r\n"));
  QCOMPARE(status().modifiedFiles, QStringList{QStringLiteral("text.txt")});
}

void GitRepoTest::statusUnusualFileNames_data() {
  QTest::addColumn<QString>("name");
  QTest::newRow("spaces") << QStringLiteral("a b  c.txt");
  QTest::newRow("latin") << QString::fromUtf16(u"ünïçödé.txt");
  QTest::newRow("cjk") << QString::fromUtf16(u"日本語.txt");
  QTest::newRow("emoji") << QString::fromUtf16(u"\U0001F600.txt");
  QTest::newRow("leading dash") << QStringLiteral("-x.txt");
  QTest::newRow("hash and percent") << QStringLiteral("#50%.txt");
  QTest::newRow("brackets") << QStringLiteral("[a]{b}(c).txt");
  // Not ending in a dot, Windows drops trailing dots.
  QTest::newRow("dots") << QStringLiteral("..double..dot..txt");
  QTest::newRow("deep") << QStringLiteral("x/y/z/w/v/u/t.txt");
}

void GitRepoTest::statusLongPaths() {
#ifndef Q_OS_WIN
  QSKIP("MAX_PATH is a Windows limit");
#else
  // Beyond MAX_PATH together with the temporary directory.
  const QString name = QStringLiteral("p").repeated(100) + '/' +
                       QStringLiteral("q").repeated(100) + '/' +
                       QStringLiteral("r").repeated(60) + QStringLiteral(".txt");
  QVERIFY(writeFile(QStringLiteral("\\\\?\\") +
                        QDir::toNativeSeparators(file(name)),
                    "x\n"));

  // Without core.longpaths libgit2 does not see the file at all, `git status`
  // at least reports the directory.
  QEXPECT_FAIL("", "Files beyond MAX_PATH are silently left out of the status "
                   "unless core.longpaths is set",
               Continue);
  QVERIFY(status().hasChanges());

  git({QStringLiteral("config"), QStringLiteral("core.longpaths"),
       QStringLiteral("true")});
  reopen();
  QCOMPARE(status().newFiles, QStringList{name});
  const bool staged = GitRepo::stageFiles(_repo.get(), {name});
  QVERIFY2(staged, qPrintable(lastErrorMessage()));
  QVERIFY2(GitRepo::commitStaged(_repo.get(), QStringLiteral("Long"), QString(),
                                 kAuthor, kEmail, true, false, false),
           qPrintable(lastErrorMessage()));
  QVERIFY(!status().hasChanges());
#endif
}

void GitRepoTest::statusUnusualFileNames() {
  QFETCH(QString, name);

  QVERIFY(writeFile(file(name), "x\n"));
  QCOMPARE(status().newFiles, QStringList{name});

  QVERIFY2(GitRepo::stageFiles(_repo.get(), {name}),
           qPrintable(lastErrorMessage()));
  QCOMPARE(status().newStagedFiles, QStringList{name});
  QVERIFY2(GitRepo::commitStaged(_repo.get(), QStringLiteral("Add"), QString(),
                                 kAuthor, kEmail, true, false, false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("-c"), QStringLiteral("core.quotepath=off"),
                QStringLiteral("ls-files")}),
           name);

  QVERIFY(writeFile(file(name), "y\n"));
  QCOMPARE(status().modifiedFiles, QStringList{name});
  QVERIFY(GitRepo::discardFiles(_repo.get(), {name}));
  QCOMPARE(readFile(file(name)), QByteArray("x\n"));
}

void GitRepoTest::statusCaseOnlyRename() {
  QVERIFY(writeFile(file("File.txt"), "x\n"));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY(QFile::rename(file("File.txt"), file("tmp.txt")));
  QVERIFY(QFile::rename(file("tmp.txt"), file("file.txt")));

  const GitRepo::GitStatus st = status();
  bool ignoreCase = false;
  {
    git_config *config = nullptr;
    QCOMPARE(git_repository_config_snapshot(&config, _repo.get()), 0);
    int value = 0;
    if (git_config_get_bool(&value, config, "core.ignorecase") == 0) {
      ignoreCase = value != 0;
    }
    git_config_free(config);
  }
  if (ignoreCase) {
    // A case insensitive file system: the same file.
    QVERIFY2(!st.hasChanges(), qPrintable(st.newFiles.join(',')));
  } else {
    QCOMPARE(st.newFiles, QStringList{QStringLiteral("file.txt")});
    QCOMPARE(st.deletedFiles, QStringList{QStringLiteral("File.txt")});
  }
}

void GitRepoTest::statusCancelled() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  _quit = true;
  QVERIFY(!status().hasChanges());
  _quit = false;
  QCOMPARE(status().newFiles, QStringList{QStringLiteral("a.txt")});
}

void GitRepoTest::statusManyFiles() {
  for (int i = 0; i < 3000; ++i) {
    QVERIFY(writeFile(file(QStringLiteral("d%1/f%2.txt").arg(i % 30).arg(i)),
                      QByteArray::number(i)));
  }
  QElapsedTimer timer;
  timer.start();
  const GitRepo::GitStatus st = status();
  QCOMPARE(st.newFiles.size(), 3000);
  QVERIFY2(timer.elapsed() < 20000, qPrintable(QString::number(timer.elapsed())));
  QVERIFY(std::is_sorted(st.newFiles.cbegin(), st.newFiles.cend()));
}

void GitRepoTest::statusSubmoduleWithNewCommit() {
  const QString subSource = _base + QStringLiteral("/subsource");
  QVERIFY(QDir().mkpath(subSource));
  bool ok = false;
  gitIn(_config, subSource, {QStringLiteral("init"), QStringLiteral("-q")}, &ok);
  QVERIFY(ok);
  QVERIFY(writeFile(subSource + QStringLiteral("/s.txt"), "s\n"));
  gitIn(_config, subSource, {QStringLiteral("add"), QStringLiteral("-A")});
  gitIn(_config, subSource,
        {QStringLiteral("commit"), QStringLiteral("-qm"), QStringLiteral("S")});

  git({QStringLiteral("submodule"), QStringLiteral("add"), QStringLiteral("-q"),
       subSource, QStringLiteral("sub")},
      &ok);
  QVERIFY(ok);
  git({QStringLiteral("commit"), QStringLiteral("-qm"), QStringLiteral("Sub")});
  reopen();
  QVERIFY(!status().hasChanges());

  // A new commit inside the submodule.
  QVERIFY(writeFile(file("sub/s.txt"), "s2\n"));
  gitIn(_config, file("sub"),
        {QStringLiteral("commit"), QStringLiteral("-qam"), QStringLiteral("S2")});
  QCOMPARE(status().modifiedFiles, QStringList{QStringLiteral("sub")});

  // Staging the submodule records its new commit.
  QVERIFY2(GitRepo::stageFiles(_repo.get(), {QStringLiteral("sub")}),
           qPrintable(lastErrorMessage()));
  QCOMPARE(status().modifiedStagedFiles, QStringList{QStringLiteral("sub")});
  QCOMPARE(git({QStringLiteral("rev-parse"), QStringLiteral(":sub")}),
           gitIn(_config, file("sub"),
                 {QStringLiteral("rev-parse"), QStringLiteral("HEAD")}));
}

// Staging.

void GitRepoTest::stageUnstageRoundTrip() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QCOMPARE(status().newFiles, QStringList{QStringLiteral("a.txt")});

  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  GitRepo::GitStatus st = status();
  QVERIFY(st.newFiles.isEmpty());
  QCOMPARE(st.newStagedFiles, QStringList{QStringLiteral("a.txt")});

  QVERIFY(GitRepo::unstageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  st = status();
  QCOMPARE(st.newFiles, QStringList{QStringLiteral("a.txt")});
  QVERIFY(st.newStagedFiles.isEmpty());

  // Staging twice is harmless.
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  QCOMPARE(status().newStagedFiles, QStringList{QStringLiteral("a.txt")});
  // Unstaging something that is not staged too.
  QVERIFY(GitRepo::unstageFiles(_repo.get(), {QStringLiteral("other.txt")}));
  QCOMPARE(status().newStagedFiles, QStringList{QStringLiteral("a.txt")});
}

void GitRepoTest::stagePathSpellings_data() {
  QTest::addColumn<QString>("spelling");
  QTest::newRow("relative") << QStringLiteral("dir/a.txt");
  QTest::newRow("backslashes") << QStringLiteral("dir\\a.txt");
  QTest::newRow("absolute") << QStringLiteral("<root>/dir/a.txt");
  QTest::newRow("absolute native") << QStringLiteral("<native>");
  QTest::newRow("dot segments") << QStringLiteral("./dir/../dir/a.txt");
}

void GitRepoTest::stagePathSpellings() {
  QFETCH(QString, spelling);
  spelling.replace(QStringLiteral("<root>"), _root);
  spelling.replace(QStringLiteral("<native>"),
                   QDir::toNativeSeparators(file("dir/a.txt")));

  QVERIFY(writeFile(file("dir/a.txt"), "a\n"));
  QVERIFY2(GitRepo::stageFiles(_repo.get(), {spelling}),
           qPrintable(lastErrorMessage()));
  QCOMPARE(status().newStagedFiles, QStringList{QStringLiteral("dir/a.txt")});
  QCOMPARE(git({QStringLiteral("ls-files")}), QStringLiteral("dir/a.txt"));
}

void GitRepoTest::stageDirectory() {
  QVERIFY(writeFile(file("dir/a.txt"), "a\n"));
  QVERIFY(writeFile(file("dir/sub/b.txt"), "b\n"));

  // The Mercurial backend stages a directory recursively.
  const bool staged = GitRepo::stageFiles(_repo.get(), {QStringLiteral("dir")});
  QVERIFY2(staged, qPrintable(lastErrorMessage()));
  QCOMPARE(status().newStagedFiles,
           (QStringList{QStringLiteral("dir/a.txt"),
                        QStringLiteral("dir/sub/b.txt")}));
}

void GitRepoTest::stageDeletedAndMissingFiles() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("A"));

  QVERIFY(QFile::remove(file("a.txt")));
  QCOMPARE(status().deletedFiles, QStringList{QStringLiteral("a.txt")});
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  QCOMPARE(status().deletedStagedFiles, QStringList{QStringLiteral("a.txt")});
  QVERIFY(GitRepo::unstageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  QCOMPARE(status().deletedFiles, QStringList{QStringLiteral("a.txt")});

  // A path that never existed is accepted and changes nothing.
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("never.txt")}));
  const GitRepo::GitStatus st = status();
  QCOMPARE(st.deletedFiles, QStringList{QStringLiteral("a.txt")});
  QVERIFY(st.newStagedFiles.isEmpty());
}

void GitRepoTest::stagePartiallyStagedFile() {
  QVERIFY(writeFile(file("a.txt"), "1\n"));
  gitCommitAll(QStringLiteral("A"));
  QVERIFY(writeFile(file("a.txt"), "2\n"));
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  QVERIFY(writeFile(file("a.txt"), "3\n"));

  // Staged and unstaged changes of one file.
  GitRepo::GitStatus st = status();
  QCOMPARE(st.modifiedStagedFiles, QStringList{QStringLiteral("a.txt")});
  QCOMPARE(st.modifiedFiles, QStringList{QStringLiteral("a.txt")});
  QCOMPARE(git({QStringLiteral("show"), QStringLiteral(":a.txt")}),
           QStringLiteral("2"));

  // Committing takes the staged version only.
  QVERIFY(GitRepo::commitStaged(_repo.get(), QStringLiteral("Two"), QString(),
                                kAuthor, kEmail, true, false, false));
  QCOMPARE(git({QStringLiteral("show"), QStringLiteral("HEAD:a.txt")}),
           QStringLiteral("2"));
  st = status();
  QCOMPARE(st.modifiedFiles, QStringList{QStringLiteral("a.txt")});
  QVERIFY(st.modifiedStagedFiles.isEmpty());
  QCOMPARE(readFile(file("a.txt")), QByteArray("3\n"));
}

void GitRepoTest::stageIgnoredFile() {
  QVERIFY(writeFile(file(".gitignore"), "*.log\n"));
  gitCommitAll(QStringLiteral("Ignore"));
  QVERIFY(writeFile(file("debug.log"), "x\n"));

  // Staging an ignored file by name adds it, like `git add -f`.
  QVERIFY2(GitRepo::stageFiles(_repo.get(), {QStringLiteral("debug.log")}),
           qPrintable(lastErrorMessage()));
  QCOMPARE(status().newStagedFiles, QStringList{QStringLiteral("debug.log")});
}

void GitRepoTest::stageNothing() {
  QVERIFY(GitRepo::stageFiles(_repo.get(), {}));
  const bool unstaged = GitRepo::unstageFiles(_repo.get(), {});
  QVERIFY(unstaged);
  QVERIFY(!status().hasChanges());
}

void GitRepoTest::stageResolvesConflict() {
  makeMergeConflict();
  QCOMPARE(status().conflictedFiles, QStringList{QStringLiteral("a.txt")});
  const QString markers = GitRepo::listConflictsForAFile(_repo.get(),
                                                         QStringLiteral("a.txt"));
  QVERIFY2(markers.contains(QStringLiteral("<<<<<<<")) &&
               markers.contains(QStringLiteral("=======")) &&
               markers.contains(QStringLiteral(">>>>>>>")),
           qPrintable(markers));
  QVERIFY(markers.contains(QStringLiteral("main")));
  QVERIFY(markers.contains(QStringLiteral("feature")));
  QVERIFY(GitRepo::listConflictsForAFile(_repo.get(), QStringLiteral("b.txt"))
              .isEmpty());

  QVERIFY(writeFile(file("a.txt"), "resolved\n"));
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  QVERIFY(!GitRepo::hasConflicts(_repo.get()));
  QVERIFY(status().conflictedFiles.isEmpty());
  QVERIFY(GitRepo::getIsMergeInProgress(_repo.get()));
}

void GitRepoTest::stageNormalizesLineEndings() {
  git({QStringLiteral("config"), QStringLiteral("core.autocrlf"),
       QStringLiteral("true")});
  reopen();
  QVERIFY(writeFile(file("crlf.txt"), "a\r\nb\r\n"));
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("crlf.txt")}));
  bool ok = false;
  const QString blob = git({QStringLiteral("cat-file"), QStringLiteral("-p"),
                            QStringLiteral(":crlf.txt")},
                           &ok);
  QVERIFY(ok);
  QVERIFY2(!blob.contains('\r'), qPrintable(blob));
}

void GitRepoTest::unstageNewFileMakesItUntracked() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  // HEAD is the empty initial commit, unstaging removes the index entry.
  QVERIFY(GitRepo::unstageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  QCOMPARE(status().newFiles, QStringList{QStringLiteral("a.txt")});
  QCOMPARE(git({QStringLiteral("ls-files")}), QString());
  QVERIFY(QFileInfo::exists(file("a.txt")));
}

// Commit and amend.

void GitRepoTest::commitOnlyStagedFiles() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(writeFile(file("b.txt"), "b\n"));
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  const QString before = head();
  QVERIFY(GitRepo::commitStaged(_repo.get(), QStringLiteral("Only a"),
                                QString(), kAuthor, kEmail, true, false,
                                false));

  QCOMPARE(git({QStringLiteral("rev-parse"), QStringLiteral("HEAD~1")}), before);
  QCOMPARE(git({QStringLiteral("show"), QStringLiteral("--name-only"),
                QStringLiteral("--format=%s|%an|%ae|%cn|%ce")}),
           QStringLiteral("Only a|Tester|tester@example.com|Tester|"
                          "tester@example.com\n\na.txt"));
  QCOMPARE(status().newFiles, QStringList{QStringLiteral("b.txt")});
  // The branch moved, HEAD is still attached.
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/main"));
}

void GitRepoTest::commitWithoutStagedFiles() {
  // Unlike the Mercurial backend, committing with nothing staged creates an
  // empty commit.
  const QString before = head();
  QVERIFY(GitRepo::commitStaged(_repo.get(), QStringLiteral("Empty"),
                                QString(), kAuthor, kEmail, true, false,
                                false));
  QCOMPARE(git({QStringLiteral("rev-parse"), QStringLiteral("HEAD~1")}), before);
  QCOMPARE(git({QStringLiteral("rev-parse"), QStringLiteral("HEAD^{tree}")}),
           git({QStringLiteral("rev-parse"), QStringLiteral("HEAD~1^{tree}")}));
}

void GitRepoTest::commitMessageComposition_data() {
  QTest::addColumn<QString>("summary");
  QTest::addColumn<QString>("message");
  QTest::addColumn<bool>("signoff");
  QTest::addColumn<QString>("expected");

  const QString signoff =
      QStringLiteral("Signed-off-by: Tester <tester@example.com>");
  QTest::newRow("summary") << QStringLiteral("Summary") << QString() << false
                           << QStringLiteral("Summary");
  QTest::newRow("body") << QStringLiteral("Summary")
                        << QStringLiteral("Line 1\nLine 2") << false
                        << QStringLiteral("Summary\n\nLine 1\nLine 2");
  QTest::newRow("signoff") << QStringLiteral("Summary") << QString() << true
                           << QStringLiteral("Summary\n\n") + signoff;
  QTest::newRow("body and signoff")
      << QStringLiteral("Summary") << QStringLiteral("Body\n\n\n") << true
      << QStringLiteral("Summary\n\nBody\n\n") + signoff;
  QTest::newRow("crlf body") << QStringLiteral("Summary")
                             << QStringLiteral("Body\r\n") << true
                             << QStringLiteral("Summary\n\nBody\n\n") + signoff;
  QTest::newRow("unicode")
      << QString::fromUtf16(u"Übersicht 日本 \U0001F600")
      << QString::fromUtf16(u"Körper") << false
      << QString::fromUtf16(u"Übersicht 日本 \U0001F600\n\nKörper");
  QTest::newRow("hash lines kept")
      << QStringLiteral("Summary") << QStringLiteral("# not a comment") << false
      << QStringLiteral("Summary\n\n# not a comment");
  QTest::newRow("long summary") << QStringLiteral("x").repeated(200) << QString()
                                << false << QStringLiteral("x").repeated(200);
}

void GitRepoTest::commitMessageComposition() {
  QFETCH(QString, summary);
  QFETCH(QString, message);
  QFETCH(bool, signoff);
  QFETCH(QString, expected);

  QVERIFY(GitRepo::commitStaged(_repo.get(), summary, message, kAuthor, kEmail,
                                true, signoff, false));
  QString stored = commitMessage(_repo.get(), QStringLiteral("HEAD"));
  if (QTest::currentDataTag() == QByteArray("crlf body")) {
    stored.remove('\r');
  }
  QCOMPARE(stored, expected);
}

void GitRepoTest::commitSignoffIsNotDuplicated() {
  const QString signoff =
      QStringLiteral("Signed-off-by: Tester <tester@example.com>");
  QVERIFY(GitRepo::commitStaged(_repo.get(), QStringLiteral("Summary"),
                                signoff, kAuthor, kEmail, true, true, false));
  const QString stored = commitMessage(_repo.get(), QStringLiteral("HEAD"));
  QCOMPARE(stored.count(signoff), 1);

  // Amending with sign-off again should not add a second line either.
  QVERIFY(GitRepo::amend(_repo.get(), QStringLiteral("Summary"),
                         QStringLiteral("Body"), kAuthor, kEmail, true, false));
  QCOMPARE(commitMessage(_repo.get(), QStringLiteral("HEAD")).count(signoff), 1);
}

void GitRepoTest::commitWithoutAuthorFails() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  const QString before = head();

  QVERIFY(!GitRepo::commitStaged(_repo.get(), QStringLiteral("S"), QString(),
                                 QString(), kEmail, true, false, false));
  QCOMPARE(lastError(), GitError::CommitStagedFailedToCreateSignature);
  QVERIFY(!GitRepo::commitStaged(_repo.get(), QStringLiteral("S"), QString(),
                                 kAuthor, QString(), true, false, false));
  QCOMPARE(lastError(), GitError::CommitStagedFailedToCreateSignature);

  // Nothing changed, the file is still staged.
  QCOMPARE(head(), before);
  QCOMPARE(status().newStagedFiles, QStringList{QStringLiteral("a.txt")});
}

void GitRepoTest::commitMergeHasTwoParents() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("A"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("feature")});
  QVERIFY(writeFile(file("f.txt"), "f\n"));
  const QString feature = gitCommitAll(QStringLiteral("F"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"),
       QStringLiteral("main")});
  QVERIFY(writeFile(file("m.txt"), "m\n"));
  const QString main = gitCommitAll(QStringLiteral("M"));
  git({QStringLiteral("merge"), QStringLiteral("--no-commit"),
       QStringLiteral("--no-ff"), QStringLiteral("-q"),
       QStringLiteral("feature")});
  QVERIFY(GitRepo::getIsMergeInProgress(_repo.get()));

  QVERIFY2(GitRepo::commitStaged(_repo.get(), QStringLiteral("Merge"),
                                 QString(), kAuthor, kEmail, true, false,
                                 false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("log"), QStringLiteral("-1"),
                QStringLiteral("--format=%P")}),
           main + ' ' + feature);
  QVERIFY(!GitRepo::getIsMergeInProgress(_repo.get()));
  QCOMPARE(git_repository_state(_repo.get()),
           static_cast<int>(GIT_REPOSITORY_STATE_NONE));
  QVERIFY(!QFileInfo::exists(file(".git/MERGE_HEAD")));
  QVERIFY(QFileInfo::exists(file("f.txt")));
}

void GitRepoTest::commitWithoutCleanupKeepsMergeState() {
  makeMergeConflict();
  QVERIFY(writeFile(file("a.txt"), "resolved\n"));
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  QVERIFY(GitRepo::commitStaged(_repo.get(), QStringLiteral("Merge"), QString(),
                                kAuthor, kEmail, false, false, false));
  // The caller asked to keep the state, MERGE_HEAD is still there.
  QVERIFY(GitRepo::getIsMergeInProgress(_repo.get()));
  QCOMPARE(git({QStringLiteral("log"), QStringLiteral("-1"),
                QStringLiteral("--format=%P")})
               .split(' ')
               .size(),
           2);
}

void GitRepoTest::commitOnDetachedHead() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString first = gitCommitAll(QStringLiteral("A"));
  QVERIFY(writeFile(file("a.txt"), "b\n"));
  const QString second = gitCommitAll(QStringLiteral("B"));

  QVERIFY(GitRepo::checkoutCommit(_repo.get(), first));
  QVERIFY(git_repository_head_detached(_repo.get()) == 1);
  QVERIFY(writeFile(file("c.txt"), "c\n"));
  const QString detached = commitAll(QStringLiteral("Detached"));
  QVERIFY(!detached.isEmpty());
  QCOMPARE(git({QStringLiteral("rev-parse"), QStringLiteral("HEAD~1")}), first);
  QVERIFY(git_repository_head_detached(_repo.get()) == 1);
  // main did not move.
  QCOMPARE(git({QStringLiteral("rev-parse"), QStringLiteral("main")}), second);
}

void GitRepoTest::amendMessageOnly() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString original = commitAll(QStringLiteral("Original"));
  const QString parent =
      git({QStringLiteral("rev-parse"), QStringLiteral("HEAD~1")});
  // Unstaged changes are not part of the amend.
  QVERIFY(writeFile(file("a.txt"), "changed\n"));

  QVERIFY(GitRepo::amend(_repo.get(), QStringLiteral("Reworded"),
                         QStringLiteral("Body"), kAuthor, kEmail, false,
                         false));
  QVERIFY(head() != original);
  QCOMPARE(commitMessage(_repo.get(), QStringLiteral("HEAD")),
           QStringLiteral("Reworded\n\nBody"));
  QCOMPARE(git({QStringLiteral("rev-parse"), QStringLiteral("HEAD~1")}), parent);
  QCOMPARE(git({QStringLiteral("rev-parse"), QStringLiteral("HEAD^{tree}")}),
           git({QStringLiteral("rev-parse"), original + QStringLiteral("^{tree}")}));
  QCOMPARE(status().modifiedFiles, QStringList{QStringLiteral("a.txt")});
  QCOMPARE(git({QStringLiteral("rev-list"), QStringLiteral("--count"),
                QStringLiteral("HEAD")}),
           QStringLiteral("2"));
}

void GitRepoTest::amendWithStagedFiles() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  commitAll(QStringLiteral("A"));
  QVERIFY(writeFile(file("b.txt"), "b\n"));
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("b.txt")}));

  QVERIFY(GitRepo::amend(_repo.get(), QStringLiteral("A and B"), QString(),
                         kAuthor, kEmail, true, false));
  QCOMPARE(git({QStringLiteral("show"), QStringLiteral("--name-only"),
                QStringLiteral("--format=%s")}),
           QStringLiteral("A and B\n\na.txt\nb.txt"));
  QVERIFY(commitMessage(_repo.get(), QStringLiteral("HEAD"))
              .endsWith(QStringLiteral(
                  "Signed-off-by: Tester <tester@example.com>")));
  QVERIFY(!status().hasChanges());
}

void GitRepoTest::amendKeepsAuthorAndParents() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("A"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("feature")});
  QVERIFY(writeFile(file("f.txt"), "f\n"));
  gitCommitAll(QStringLiteral("F"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"),
       QStringLiteral("main")});
  QVERIFY(writeFile(file("m.txt"), "m\n"));
  gitCommitAll(QStringLiteral("M"));
  git({QStringLiteral("-c"), QStringLiteral("user.name=") + kOtherAuthor,
       QStringLiteral("-c"), QStringLiteral("user.email=") + kOtherEmail,
       QStringLiteral("merge"), QStringLiteral("-q"), QStringLiteral("--no-ff"),
       QStringLiteral("-m"), QStringLiteral("Merge"),
       QStringLiteral("feature")});
  const QString parents =
      git({QStringLiteral("log"), QStringLiteral("-1"), QStringLiteral("--format=%P")});

  QVERIFY(GitRepo::amend(_repo.get(), QStringLiteral("Merged"), QString(),
                         kAuthor, kEmail, false, false));
  // The merge keeps both parents and its author, the committer is new.
  QCOMPARE(git({QStringLiteral("log"), QStringLiteral("-1"),
                QStringLiteral("--format=%P|%an|%ae|%cn|%s")}),
           parents + QStringLiteral("|Other Author|other@example.com|Tester|Merged"));
}

void GitRepoTest::amendRootCommit() {
  // The initial commit has no parent.
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  QVERIFY(GitRepo::amend(_repo.get(), QStringLiteral("Root"), QString(),
                         kAuthor, kEmail, false, false));
  QCOMPARE(git({QStringLiteral("rev-list"), QStringLiteral("--count"),
                QStringLiteral("HEAD")}),
           QStringLiteral("1"));
  QCOMPARE(git({QStringLiteral("log"), QStringLiteral("--format=%s")}),
           QStringLiteral("Root"));
  QCOMPARE(git({QStringLiteral("ls-tree"), QStringLiteral("--name-only"),
                QStringLiteral("HEAD")}),
           QStringLiteral("a.txt"));
}

// Discard, reset, checkout and patches.

void GitRepoTest::discardModifiedAndDeletedFiles() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(writeFile(file("b.txt"), "b\n"));
  QVERIFY(writeFile(file("dir/c.txt"), "c\n"));
  gitCommitAll(QStringLiteral("Base"));

  QVERIFY(writeFile(file("a.txt"), "changed\n"));
  QVERIFY(writeFile(file("b.txt"), "also changed\n"));
  QVERIFY(QDir(file("dir")).removeRecursively());

  QVERIFY(GitRepo::discardFiles(_repo.get(), {QStringLiteral("a.txt"),
                                              QStringLiteral("dir/c.txt")}));
  QCOMPARE(readFile(file("a.txt")), QByteArray("a\n"));
  // The deleted file and its directory are back.
  QCOMPARE(readFile(file("dir/c.txt")), QByteArray("c\n"));
  // Other files are left alone.
  QCOMPARE(readFile(file("b.txt")), QByteArray("also changed\n"));
  QCOMPARE(status().modifiedFiles, QStringList{QStringLiteral("b.txt")});
}

void GitRepoTest::discardStagedChanges() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(writeFile(file("d.txt"), "d\n"));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY(writeFile(file("a.txt"), "staged\n"));
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  QVERIFY(writeFile(file("a.txt"), "staged and more\n"));
  git({QStringLiteral("rm"), QStringLiteral("-q"), QStringLiteral("d.txt")});

  QVERIFY(GitRepo::discardFiles(_repo.get(), {QStringLiteral("a.txt"),
                                              QStringLiteral("d.txt")}));
  QCOMPARE(readFile(file("a.txt")), QByteArray("a\n"));
  QCOMPARE(readFile(file("d.txt")), QByteArray("d\n"));
  QVERIFY(!status().hasChanges());
}

void GitRepoTest::discardStagedNewFileKeepsIt() {
  QVERIFY(writeFile(file("new.txt"), "precious\n"));
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("new.txt")}));
  GitRepo::discardFiles(_repo.get(), {QStringLiteral("new.txt")});

  // Like the Mercurial backend: the file stays on disk and becomes untracked.
  QCOMPARE(readFile(file("new.txt")), QByteArray("precious\n"));
  QVERIFY(status().newStagedFiles.isEmpty());
  QCOMPARE(status().newFiles, QStringList{QStringLiteral("new.txt")});
}

void GitRepoTest::discardUnusualPaths() {
  const QString name = QString::fromUtf16(u"dir with space/ünï.txt");
  QVERIFY(writeFile(file(name), "a\n"));
  QVERIFY(writeFile(file("x/y.txt"), "y\n"));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY(writeFile(file(name), "b\n"));
  QVERIFY(writeFile(file("x/y.txt"), "z\n"));

  QVERIFY(GitRepo::discardFiles(_repo.get(), {name}));
  QCOMPARE(readFile(file(name)), QByteArray("a\n"));

  // Native separators, as they come from a file dialog.
  QVERIFY(GitRepo::discardFiles(_repo.get(), {QStringLiteral("x\\y.txt")}));
  QCOMPARE(readFile(file("x/y.txt")), QByteArray("y\n"));
}

void GitRepoTest::resetTypes_data() {
  QTest::addColumn<GitRepo::ResetType>("type");
  QTest::addColumn<bool>("indexHasChange");
  QTest::addColumn<bool>("workdirHasChange");
  QTest::newRow("soft") << GitRepo::ResetType::Soft << true << true;
  QTest::newRow("mixed") << GitRepo::ResetType::Mixed << false << true;
  QTest::newRow("hard") << GitRepo::ResetType::Hard << false << false;
}

void GitRepoTest::resetTypes() {
  QFETCH(GitRepo::ResetType, type);
  QFETCH(bool, indexHasChange);
  QFETCH(bool, workdirHasChange);

  QVERIFY(writeFile(file("a.txt"), "1\n"));
  const QString first = gitCommitAll(QStringLiteral("One"));
  QVERIFY(writeFile(file("a.txt"), "2\n"));
  gitCommitAll(QStringLiteral("Two"));
  QVERIFY(writeFile(file("untracked.txt"), "u\n"));

  QVERIFY2(GitRepo::resetToCommit(_repo.get(), first, type),
           qPrintable(lastErrorMessage()));
  QCOMPARE(head(), first);
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/main"));
  QCOMPARE(git({QStringLiteral("show"), QStringLiteral(":a.txt")}) ==
               QStringLiteral("2"),
           indexHasChange);
  QCOMPARE(readFile(file("a.txt")) == QByteArray("2\n"), workdirHasChange);
  // Untracked files survive every reset.
  QCOMPARE(readFile(file("untracked.txt")), QByteArray("u\n"));
}

void GitRepoTest::resetToUnknownRevisionFails() {
  const QString before = head();
  for (const QString &revision :
       {QStringLiteral("no-such-branch"), QString(40, QLatin1Char('a')),
        QStringLiteral("HEAD~5"), QString()}) {
    QVERIFY2(!GitRepo::resetToCommit(_repo.get(), revision,
                                     GitRepo::ResetType::Hard),
             qPrintable(revision));
    QCOMPARE(lastError(), GitError::ResetToCommitFailedToResolveRevision);
  }
  QCOMPARE(head(), before);

  // Branch names and relative revisions work too.
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("A"));
  QVERIFY(GitRepo::resetToCommit(_repo.get(), QStringLiteral("HEAD~1"),
                                 GitRepo::ResetType::Hard));
  QCOMPARE(head(), before);
  QVERIFY(!QFileInfo::exists(file("a.txt")));

  // An unknown reset type should fail.
  const bool reset = GitRepo::resetToCommit(_repo.get(), QStringLiteral("HEAD"),
                                            GitRepo::ResetType::Unknown);
  QVERIFY(!reset);
}

void GitRepoTest::resetClearsMergeState() {
  makeMergeConflict();
  QVERIFY(GitRepo::resetToCommit(_repo.get(), QStringLiteral("HEAD"),
                                 GitRepo::ResetType::Hard));
  QVERIFY(!GitRepo::hasConflicts(_repo.get()));
  QCOMPARE(readFile(file("a.txt")), QByteArray("main\n"));
  // Like `git reset --hard`, MERGE_HEAD is gone.
  QVERIFY(!GitRepo::getIsMergeInProgress(_repo.get()));
  // So there is nothing to abort.
  QVERIFY(!GitRepo::abortMerge(_repo.get()));

  // abortMerge restores the files and HEAD.
  const QString before = head();
  bool ok = true;
  git({QStringLiteral("merge"), QStringLiteral("-q"),
       QStringLiteral("feature")},
      &ok);
  QVERIFY(!ok);
  QVERIFY(GitRepo::getIsMergeInProgress(_repo.get()));
  QVERIFY2(GitRepo::abortMerge(_repo.get()), qPrintable(lastErrorMessage()));
  QVERIFY(!GitRepo::getIsMergeInProgress(_repo.get()));
  QVERIFY(!GitRepo::hasConflicts(_repo.get()));
  QCOMPARE(head(), before);
  QCOMPARE(readFile(file("a.txt")), QByteArray("main\n"));
  QVERIFY(!status().hasChanges());
}

void GitRepoTest::checkoutCommitDetaches() {
  QVERIFY(writeFile(file("a.txt"), "1\n"));
  const QString first = gitCommitAll(QStringLiteral("One"));
  QVERIFY(writeFile(file("a.txt"), "2\n"));
  QVERIFY(writeFile(file("b.txt"), "b\n"));
  const QString second = gitCommitAll(QStringLiteral("Two"));

  QVERIFY2(GitRepo::checkoutCommit(_repo.get(), first),
           qPrintable(lastErrorMessage()));
  QCOMPARE(head(), first);
  QVERIFY(git_repository_head_detached(_repo.get()) == 1);
  QCOMPARE(readFile(file("a.txt")), QByteArray("1\n"));
  QVERIFY(!QFileInfo::exists(file("b.txt")));
  QVERIFY(!status().hasChanges());
  QVERIFY(GitRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local,
                                   _quit) != QStringLiteral("main"));
  QCOMPARE(git({QStringLiteral("rev-parse"), QStringLiteral("main")}), second);

  // Back to the branch.
  QVERIFY(GitRepo::checkoutBranch(_repo.get(), QStringLiteral("main"),
                                  GitRepo::BranchType::Local));
  QCOMPARE(head(), second);
  QVERIFY(git_repository_head_detached(_repo.get()) == 0);
}

void GitRepoTest::checkoutCommitRefusesToOverwrite() {
  QVERIFY(writeFile(file("a.txt"), "1\n"));
  const QString first = gitCommitAll(QStringLiteral("One"));
  QVERIFY(writeFile(file("a.txt"), "2\n"));
  const QString second = gitCommitAll(QStringLiteral("Two"));

  // A local change that the checkout would overwrite.
  QVERIFY(writeFile(file("a.txt"), "local\n"));
  QVERIFY(!GitRepo::checkoutCommit(_repo.get(), first));
  QCOMPARE(lastError(), GitError::CheckoutCommitFailedToCheckoutCommitTree);
  QCOMPARE(readFile(file("a.txt")), QByteArray("local\n"));
  QCOMPARE(head(), second);
  QVERIFY(git_repository_head_detached(_repo.get()) == 0);

  // A local change in another file is carried along.
  QVERIFY(writeFile(file("a.txt"), "2\n"));
  QVERIFY(writeFile(file("other.txt"), "keep\n"));
  QVERIFY(GitRepo::checkoutCommit(_repo.get(), first));
  QCOMPARE(readFile(file("other.txt")), QByteArray("keep\n"));
}

void GitRepoTest::checkoutCommitInvalidIds_data() {
  QTest::addColumn<QString>("id");
  QTest::addColumn<GitError>("error");
  QTest::newRow("empty") << QString()
                         << GitError::CheckoutCommitFailedToParseObjectId;
  QTest::newRow("garbage") << QStringLiteral("not a sha")
                           << GitError::CheckoutCommitFailedToParseObjectId;
  QTest::newRow("unknown") << QString(40, QLatin1Char('a'))
                           << GitError::CheckoutCommitFailedToLookupCommit;
  QTest::newRow("branch name") << QStringLiteral("main")
                               << GitError::CheckoutCommitFailedToParseObjectId;
}

void GitRepoTest::checkoutCommitInvalidIds() {
  QFETCH(QString, id);
  QFETCH(GitError, error);
  const QString before = head();
  QVERIFY(!GitRepo::checkoutCommit(_repo.get(), id));
  QCOMPARE(lastError(), error);
  QCOMPARE(head(), before);
  QVERIFY(git_repository_head_detached(_repo.get()) == 0);
}

void GitRepoTest::patchRoundTrip() {
  QVERIFY(writeFile(file("a.txt"), "1\n2\n3\n4\n5\n"));
  QVERIFY(writeFile(file("b.txt"), "b\n"));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY(writeFile(file("a.txt"), "1\ntwo\n3\n4\nfive\n"));
  QVERIFY(writeFile(file("b.txt"), "b\nmore b\n"));
  const QString changed = gitCommitAll(QStringLiteral("Change"));

  const QString patch = GitRepo::createPatchFromCommit(_repo.get(), changed);
  QVERIFY2(patch.contains(QStringLiteral("diff --git a/a.txt b/a.txt")),
           qPrintable(patch));
  QVERIFY(patch.contains(QStringLiteral("-2\n+two")));
  QVERIFY(patch.contains(QStringLiteral("+more b")));

  QVERIFY(GitRepo::resetToCommit(_repo.get(), QStringLiteral("HEAD~1"),
                                 GitRepo::ResetType::Hard));
  QVERIFY2(GitRepo::applyPatch(_repo.get(), patch),
           qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("a.txt")), QByteArray("1\ntwo\n3\n4\nfive\n"));
  QCOMPARE(readFile(file("b.txt")), QByteArray("b\nmore b\n"));
  // The patch goes to the working copy only.
  const GitRepo::GitStatus st = status();
  QCOMPARE(st.modifiedFiles,
           (QStringList{QStringLiteral("a.txt"), QStringLiteral("b.txt")}));
  QVERIFY(st.modifiedStagedFiles.isEmpty());

  // Applying it again fails, the changes are there already.
  QVERIFY(!GitRepo::applyPatch(_repo.get(), patch));
  QCOMPARE(lastError(), GitError::ApplyPatchFailedToApplyPatch);
  QCOMPARE(readFile(file("a.txt")), QByteArray("1\ntwo\n3\n4\nfive\n"));
}

void GitRepoTest::patchAddsAndDeletesFiles() {
  QVERIFY(writeFile(file("gone.txt"), "bye\n"));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY(QFile::remove(file("gone.txt")));
  QVERIFY(writeFile(file("dir/new.txt"), "hello\nworld\n"));
  const QString changed = gitCommitAll(QStringLiteral("Add and delete"));
  const QString patch = GitRepo::createPatchFromCommit(_repo.get(), changed);
  QVERIFY2(patch.contains(QStringLiteral("new file mode")), qPrintable(patch));
  QVERIFY(patch.contains(QStringLiteral("deleted file mode")));

  QVERIFY(GitRepo::resetToCommit(_repo.get(), QStringLiteral("HEAD~1"),
                                 GitRepo::ResetType::Hard));
  QVERIFY(!QFileInfo::exists(file("dir/new.txt")));
  const bool applied = GitRepo::applyPatch(_repo.get(), patch);
  QVERIFY2(applied, qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("dir/new.txt")), QByteArray("hello\nworld\n"));
  QVERIFY(!QFileInfo::exists(file("gone.txt")));
}

void GitRepoTest::patchWithoutNewlineAtEnd() {
  QVERIFY(writeFile(file("a.txt"), "no newline"));
  gitCommitAll(QStringLiteral("Base"));
  const QString patch =
      QStringLiteral("diff --git a/a.txt b/a.txt\n"
                     "--- a/a.txt\n"
                     "+++ b/a.txt\n"
                     "@@ -1 +1 @@\n"
                     "-no newline\n"
                     "\\ No newline at end of file\n"
                     "+still none\n"
                     "\\ No newline at end of file\n");
  QVERIFY2(GitRepo::applyPatch(_repo.get(), patch),
           qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("a.txt")), QByteArray("still none"));
}

void GitRepoTest::patchContextMismatchLeavesFilesAlone() {
  QVERIFY(writeFile(file("a.txt"), "x\ny\nz\n"));
  QVERIFY(writeFile(file("b.txt"), "b\n"));
  gitCommitAll(QStringLiteral("Base"));
  const QString patch =
      QStringLiteral("diff --git a/b.txt b/b.txt\n"
                     "--- a/b.txt\n"
                     "+++ b/b.txt\n"
                     "@@ -1 +1 @@\n"
                     "-b\n"
                     "+b2\n"
                     "diff --git a/a.txt b/a.txt\n"
                     "--- a/a.txt\n"
                     "+++ b/a.txt\n"
                     "@@ -1,3 +1,3 @@\n"
                     " x\n"
                     "-not there\n"
                     "+new\n"
                     " z\n"
                     "diff --git a/c.txt b/c.txt\n"
                     "new file mode 100644\n"
                     "--- /dev/null\n"
                     "+++ b/c.txt\n"
                     "@@ -0,0 +1 @@\n"
                     "+c\n");
  QVERIFY(!GitRepo::applyPatch(_repo.get(), patch));
  QCOMPARE(lastError(), GitError::ApplyPatchFailedToApplyPatch);
  // Nothing is half applied and no helper files are left.
  QCOMPARE(readFile(file("a.txt")), QByteArray("x\ny\nz\n"));
  QCOMPARE(readFile(file("b.txt")), QByteArray("b\n"));
  QVERIFY(!QFileInfo::exists(file("c.txt")));
  QVERIFY(!status().hasChanges());
}

void GitRepoTest::patchGarbageFails_data() {
  QTest::addColumn<QString>("patch");
  QTest::newRow("empty") << QString();
  QTest::newRow("text") << QStringLiteral("this is not a patch\n");
  QTest::newRow("truncated hunk")
      << QStringLiteral("diff --git a/a.txt b/a.txt\n--- a/a.txt\n+++ b/a.txt\n"
                        "@@ -1,5 +1,5 @@\n-a\n");
  QTest::newRow("missing file")
      << QStringLiteral("diff --git a/none.txt b/none.txt\n--- a/none.txt\n"
                        "+++ b/none.txt\n@@ -1 +1 @@\n-a\n+b\n");
  QTest::newRow("outside repository")
      << QStringLiteral("diff --git a/../evil.txt b/../evil.txt\nnew file mode "
                        "100644\n--- /dev/null\n+++ b/../evil.txt\n@@ -0,0 +1 "
                        "@@\n+evil\n");
}

void GitRepoTest::patchGarbageFails() {
  QFETCH(QString, patch);
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));

  const bool applied = GitRepo::applyPatch(_repo.get(), patch);
  if (QTest::currentDataTag() == QByteArray("empty")) {
    // An empty patch changes nothing and is not an error.
    QVERIFY(applied);
  } else {
    QVERIFY(!applied);
  }
  QCOMPARE(readFile(file("a.txt")), QByteArray("a\n"));
  QVERIFY(!QFileInfo::exists(_base + QStringLiteral("/evil.txt")));
  QVERIFY(!QFileInfo::exists(file("none.txt")));
  QVERIFY(!status().hasChanges());
}

void GitRepoTest::patchOfUnknownCommit() {
  QVERIFY(GitRepo::createPatchFromCommit(_repo.get(), QString(40, 'a')).isEmpty());
  QCOMPARE(lastError(), GitError::CreatePatchFromCommitFailedToLookupCommit);
  QVERIFY(GitRepo::createPatchFromCommit(_repo.get(), QStringLiteral("xyz"))
              .isEmpty());
  QCOMPARE(lastError(), GitError::CreatePatchFromCommitFailedToParseObjectId);

  // The root commit has no parent, its patch adds everything.
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  QVERIFY(GitRepo::amend(_repo.get(), QStringLiteral("Root"), QString(),
                         kAuthor, kEmail, false, false));
  const QString patch = GitRepo::createPatchFromCommit(_repo.get(), head());
  QVERIFY2(patch.contains(QStringLiteral("+++ b/a.txt")), qPrintable(patch));
  QVERIFY(patch.contains(QStringLiteral("+a")));
}

// Diff, blame and file sizes.

void GitRepoTest::diffModifiedAndNewFiles() {
  QVERIFY(writeFile(file("a.txt"), "one\ntwo\nthree\n"));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY(writeFile(file("a.txt"), "one\n2\nthree\n"));
  QVERIFY(writeFile(file("new.txt"), "n1\nn2\n"));

  const GitRepo::FilesDiff diff = GitRepo::diffFiles(
      _repo.get(), {QStringLiteral("a.txt"), QStringLiteral("new.txt")});
  QVERIFY(diff.valid);
  QCOMPARE(diff.deltas.size(), 2);
  QCOMPARE(diff.deltas[0].oldFilePath, QStringLiteral("a.txt"));
  QCOMPARE(changedLines(diff.deltas[0]),
           (QStringList{QStringLiteral("-two"), QStringLiteral("+2")}));
  QCOMPARE(diff.deltas[0].hunks.size(), 1);
  QCOMPARE(diff.deltas[0].hunks[0].oldStart, 1);
  QCOMPARE(diff.deltas[0].hunks[0].oldLines, 3);
  QCOMPARE(diff.deltas[1].newFilePath, QStringLiteral("new.txt"));
  QCOMPARE(changedLines(diff.deltas[1]),
           (QStringList{QStringLiteral("+n1"), QStringLiteral("+n2")}));

  QCOMPARE(diff.repoContent.value(QStringLiteral("a.txt")),
           QStringLiteral("one\ntwo\nthree\n"));
  QCOMPARE(diff.filesContent.value(QStringLiteral("a.txt")),
           QStringLiteral("one\n2\nthree\n"));
  QCOMPARE(diff.repoContent.value(QStringLiteral("new.txt")), QString());
  QCOMPARE(diff.filesContent.value(QStringLiteral("new.txt")),
           QStringLiteral("n1\nn2\n"));
  QVERIFY(diff.diff.contains(QStringLiteral("diff --git a/a.txt b/a.txt")));
  QVERIFY(diff.diff.contains(QStringLiteral("new file mode 100644\n")));
  QVERIFY(diff.diff.contains(QStringLiteral("--- /dev/null\n+++ b/new.txt")));
}

void GitRepoTest::diffDeletedFile() {
  QVERIFY(writeFile(file("a.txt"), "a\nb\n"));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY(QFile::remove(file("a.txt")));

  const GitRepo::FilesDiff diff =
      GitRepo::diffFiles(_repo.get(), {QStringLiteral("a.txt")});
  QVERIFY(diff.valid);
  QCOMPARE(diff.deltas.size(), 1);
  QCOMPARE(changedLines(diff.deltas[0]),
           (QStringList{QStringLiteral("-a"), QStringLiteral("-b")}));
  QCOMPARE(diff.filesContent.value(QStringLiteral("a.txt")), QString());
  QCOMPARE(diff.repoContent.value(QStringLiteral("a.txt")),
           QStringLiteral("a\nb\n"));
}

void GitRepoTest::diffCommittedEmptyFile() {
  QVERIFY(writeFile(file("empty.txt"), QByteArray()));
  gitCommitAll(QStringLiteral("Empty file"));
  QVERIFY(writeFile(file("empty.txt"), "now\n"));

  const GitRepo::FilesDiff diff =
      GitRepo::diffFiles(_repo.get(), {QStringLiteral("empty.txt")});
  QVERIFY(diff.valid);
  // One modification, the file was in the repository already.
  QCOMPARE(diff.deltas.size(), 1);
  QVERIFY(!diff.diff.contains(QStringLiteral("new file mode")));
}

void GitRepoTest::diffBinaryFile() {
  QByteArray binary("\x00\x01\x02\xff\xfe", 5);
  QVERIFY(writeFile(file("bin.dat"), binary));
  gitCommitAll(QStringLiteral("Binary"));
  binary[2] = '\x03';
  QVERIFY(writeFile(file("bin.dat"), binary));

  const GitRepo::FilesDiff diff =
      GitRepo::diffFiles(_repo.get(), {QStringLiteral("bin.dat")});
  QVERIFY(diff.valid);
  QCOMPARE(diff.deltas.size(), 1);
  // Binary files have no text lines.
  QVERIFY(changedLines(diff.deltas[0]).isEmpty());
}

void GitRepoTest::diffNoNewlineAtEnd() {
  QVERIFY(writeFile(file("a.txt"), "a\nb"));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY(writeFile(file("a.txt"), "a\nb\n"));

  const GitRepo::FilesDiff diff =
      GitRepo::diffFiles(_repo.get(), {QStringLiteral("a.txt")});
  QVERIFY(diff.valid);
  QCOMPARE(diff.deltas.size(), 1);
  QCOMPARE(changedLines(diff.deltas[0]),
           (QStringList{QStringLiteral("-b"), QStringLiteral("+b")}));
  QCOMPARE(diff.repoContent.value(QStringLiteral("a.txt")), QStringLiteral("a\nb"));
}

void GitRepoTest::diffUnicodeAndCrlf() {
  const QString name = QString::fromUtf16(u"ünï.txt");
  QVERIFY(writeFile(file(name), QString::fromUtf16(u"日本\n").toUtf8()));
  QVERIFY(writeFile(file("crlf.txt"), "a\r\nb\r\n"));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY(writeFile(file(name),
                    QString::fromUtf16(u"日本語\n").toUtf8()));
  QVERIFY(writeFile(file("crlf.txt"), "a\r\nc\r\n"));

  const GitRepo::FilesDiff diff =
      GitRepo::diffFiles(_repo.get(), {name, QStringLiteral("crlf.txt")});
  QVERIFY(diff.valid);
  QCOMPARE(diff.deltas.size(), 2);
  QStringList all;
  for (const GitRepo::FilesDiffDelta &delta : diff.deltas) {
    all += changedLines(delta);
  }
  QVERIFY(all.contains(QString::fromUtf16(u"+日本語")));
  // The CR stays part of the line content, nothing else is lost.
  QVERIFY(all.contains(QStringLiteral("+c\r")) ||
          all.contains(QStringLiteral("+c")));
  QCOMPARE(diff.filesContent.value(QStringLiteral("crlf.txt")),
           QStringLiteral("a\r\nc\r\n"));
}

void GitRepoTest::diffRespectsMaxFileSize() {
  const ConfigOverride maxSize(ConfigManager::ConfigKey::GitRepoMaxDiffFileSize,
                               10);
  QVERIFY(writeFile(file("big.txt"), QByteArray(100, 'x') + '\n'));
  QVERIFY(writeFile(file("small.txt"), "s\n"));
  const QString commit = gitCommitAll(QStringLiteral("Base"));
  QVERIFY(writeFile(file("big.txt"), QByteArray(100, 'y') + '\n'));
  QVERIFY(writeFile(file("small.txt"), "t\n"));

  const GitRepo::FilesDiff diff = GitRepo::diffFiles(
      _repo.get(), {QStringLiteral("big.txt"), QStringLiteral("small.txt")});
  QVERIFY(diff.valid);
  QCOMPARE(diff.repoContent.value(QStringLiteral("big.txt")),
           QStringLiteral("[[FILE SIZE EXCEEDS LIMIT OF 10 BYTES]]"));
  QCOMPARE(diff.repoContent.value(QStringLiteral("small.txt")),
           QStringLiteral("s\n"));

  const GitRepo::FilesDiff fromCommit = GitRepo::diffFilesFromCommit(
      _repo.get(), {QStringLiteral("big.txt")}, commit);
  QVERIFY(fromCommit.valid);
  QCOMPARE(fromCommit.filesContent.value(QStringLiteral("big.txt")),
           QStringLiteral("[[FILE SIZE EXCEEDS LIMIT OF 10 BYTES]]"));
}

void GitRepoTest::diffOnlyRequestedFiles() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(writeFile(file("b.txt"), "b\n"));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY(writeFile(file("a.txt"), "a2\n"));
  QVERIFY(writeFile(file("b.txt"), "b2\n"));

  const GitRepo::FilesDiff diff =
      GitRepo::diffFiles(_repo.get(), {QStringLiteral("b.txt")});
  QVERIFY(diff.valid);
  QCOMPARE(diff.deltas.size(), 1);
  QCOMPARE(diff.deltas[0].newFilePath, QStringLiteral("b.txt"));
  QCOMPARE(diff.filesContent.keys(), QStringList{QStringLiteral("b.txt")});

  // An unchanged file has no delta.
  QVERIFY(GitRepo::discardFiles(_repo.get(), {QStringLiteral("b.txt")}));
  const GitRepo::FilesDiff unchanged =
      GitRepo::diffFiles(_repo.get(), {QStringLiteral("b.txt")});
  QVERIFY(unchanged.valid);
  QVERIFY(unchanged.deltas.isEmpty());
}

void GitRepoTest::diffFromCommit() {
  QVERIFY(writeFile(file("a.txt"), "1\n"));
  QVERIFY(writeFile(file("b.txt"), "b\n"));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY(writeFile(file("a.txt"), "2\n"));
  QVERIFY(QFile::remove(file("b.txt")));
  QVERIFY(writeFile(file("c.txt"), "c\n"));
  const QString commit = gitCommitAll(QStringLiteral("Change"));
  // Later working copy changes don't matter.
  QVERIFY(writeFile(file("a.txt"), "3\n"));

  const GitRepo::FilesDiff diff = GitRepo::diffFilesFromCommit(
      _repo.get(),
      {QStringLiteral("a.txt"), QStringLiteral("b.txt"), QStringLiteral("c.txt")},
      commit);
  QVERIFY(diff.valid);
  QCOMPARE(diff.deltas.size(), 3);
  QCOMPARE(diff.repoContent.value(QStringLiteral("a.txt")), QStringLiteral("1\n"));
  QCOMPARE(diff.filesContent.value(QStringLiteral("a.txt")),
           QStringLiteral("2\n"));
  QCOMPARE(diff.repoContent.value(QStringLiteral("b.txt")), QStringLiteral("b\n"));
  QCOMPARE(diff.filesContent.value(QStringLiteral("b.txt")), QString());
  QCOMPARE(diff.repoContent.value(QStringLiteral("c.txt")), QString());
  QCOMPARE(diff.filesContent.value(QStringLiteral("c.txt")),
           QStringLiteral("c\n"));
}

void GitRepoTest::diffFromRootAndMergeCommits() {
  // Root commit: everything is added.
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  QVERIFY(GitRepo::amend(_repo.get(), QStringLiteral("Root"), QString(),
                         kAuthor, kEmail, false, false));
  const GitRepo::FilesDiff root =
      GitRepo::diffFilesFromCommit(_repo.get(), {QStringLiteral("a.txt")}, head());
  QVERIFY(root.valid);
  QCOMPARE(root.deltas.size(), 1);
  QCOMPARE(changedLines(root.deltas[0]), QStringList{QStringLiteral("+a")});

  // Merge commit: compared with its first parent.
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("feature")});
  QVERIFY(writeFile(file("f.txt"), "f\n"));
  gitCommitAll(QStringLiteral("F"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"),
       QStringLiteral("main")});
  QVERIFY(writeFile(file("m.txt"), "m\n"));
  gitCommitAll(QStringLiteral("M"));
  git({QStringLiteral("merge"), QStringLiteral("-q"), QStringLiteral("--no-ff"),
       QStringLiteral("-m"), QStringLiteral("Merge"),
       QStringLiteral("feature")});
  const GitRepo::FilesDiff merge = GitRepo::diffFilesFromCommit(
      _repo.get(), {QStringLiteral("f.txt"), QStringLiteral("m.txt")}, head());
  QVERIFY(merge.valid);
  QCOMPARE(merge.deltas.size(), 1);
  QCOMPARE(merge.deltas[0].newFilePath, QStringLiteral("f.txt"));
}

void GitRepoTest::diffFromInvalidCommit_data() {
  QTest::addColumn<QString>("commit");
  QTest::addColumn<GitError>("error");
  QTest::newRow("empty") << QString()
                         << GitError::DiffFilesFromCommitFailedToParseObjectId;
  QTest::newRow("garbage") << QStringLiteral("zzz")
                           << GitError::DiffFilesFromCommitFailedToParseObjectId;
  QTest::newRow("unknown") << QString(40, QLatin1Char('b'))
                           << GitError::DiffFilesFromCommitFailedToLookupCommit;
}

void GitRepoTest::diffFromInvalidCommit() {
  QFETCH(QString, commit);
  QFETCH(GitError, error);
  const GitRepo::FilesDiff diff =
      GitRepo::diffFilesFromCommit(_repo.get(), {QStringLiteral("a.txt")}, commit);
  QVERIFY(!diff.valid);
  QCOMPARE(lastError(), error);
}

void GitRepoTest::blameCommittedAndUncommittedLines() {
  QVERIFY(writeFile(file("a.txt"), "one\ntwo\n"));
  const QString first = commitAll(QStringLiteral("First"));
  QVERIFY(writeFile(file("a.txt"), "one\ntwo\nthree\n"));
  const QString second = commitAll(QStringLiteral("Second"));
  // Same number of lines, the second one changed in the working copy.
  QVERIFY(writeFile(file("a.txt"), "one\nTWO\nthree\n"));

  const GitRepo::BlameReport report = GitRepo::blame(
      _repo.get(), {QStringLiteral("a.txt"), QStringLiteral("missing.txt")});
  QVERIFY(report.valid);
  QCOMPARE(report.fileInfos.size(), 2);

  const GitRepo::BlameFileInfo &a = report.fileInfos[0];
  QVERIFY(a.errorMessage.isEmpty());
  QCOMPARE(a.filePath, QStringLiteral("a.txt"));
  QCOMPARE(a.lineInfos.size(), 3);
  QVERIFY(a.lineInfos.size() >= 3);
  QCOMPARE(a.lineInfos[0].commit, first);
  QCOMPARE(a.lineInfos[0].content, QStringLiteral("one"));
  QCOMPARE(a.lineInfos[0].summary, QStringLiteral("First"));
  QCOMPARE(a.lineInfos[0].author, kAuthor);
  QCOMPARE(a.lineInfos[0].email, kEmail);
  QCOMPARE(a.lineInfos[0].lineNumber, qint64(1));
  QCOMPARE(a.lineInfos[2].commit, second);
  QCOMPARE(a.lineInfos[2].lineNumber, qint64(3));
  // The changed line is not committed yet.
  QCOMPARE(a.lineInfos[1].content, QStringLiteral("TWO"));
  QCOMPARE(a.lineInfos[1].commit, kNullCommit);

  const GitRepo::BlameFileInfo &missing = report.fileInfos[1];
  QVERIFY(!missing.errorMessage.isEmpty());
  QVERIFY(missing.lineInfos.isEmpty());
}

void GitRepoTest::blameMultipleAuthors() {
  QVERIFY(writeFile(file("a.txt"), "mine\n"));
  gitCommitAll(QStringLiteral("Mine"));
  QVERIFY(writeFile(file("a.txt"), "mine\ntheirs\n"));
  git({QStringLiteral("add"), QStringLiteral("a.txt")});
  git({QStringLiteral("-c"), QStringLiteral("user.name=") + kOtherAuthor,
       QStringLiteral("-c"), QStringLiteral("user.email=") + kOtherEmail,
       QStringLiteral("commit"), QStringLiteral("-qm"),
       QStringLiteral("Theirs")});
  // Blame of a file in a subdirectory, given with native separators.
  QVERIFY(writeFile(file("dir/b.txt"), "b\n"));
  gitCommitAll(QStringLiteral("B"));

  const GitRepo::BlameReport report = GitRepo::blame(
      _repo.get(), {QStringLiteral("a.txt"), QStringLiteral("dir\\b.txt")});
  QVERIFY(report.valid);
  QCOMPARE(report.fileInfos[0].lineInfos[0].author, kAuthor);
  QCOMPARE(report.fileInfos[0].lineInfos[1].author, kOtherAuthor);
  QCOMPARE(report.fileInfos[0].lineInfos[1].email, kOtherEmail);
  QCOMPARE(report.fileInfos[0].lineInfos[1].summary, QStringLiteral("Theirs"));
  QCOMPARE(report.fileInfos[1].filePath, QStringLiteral("dir/b.txt"));
  QVERIFY2(report.fileInfos[1].errorMessage.isEmpty(),
           qPrintable(report.fileInfos[1].errorMessage));
  QCOMPARE(report.fileInfos[1].lineInfos[0].summary, QStringLiteral("B"));
}

void GitRepoTest::blameEdgeCases() {
  QVERIFY(writeFile(file("empty.txt"), QByteArray()));
  QVERIFY(writeFile(file("crlf.txt"), "a\r\nb\r\n"));
  QVERIFY(writeFile(file("nonl.txt"), "x\ny"));
  gitCommitAll(QStringLiteral("Base"));

  const GitRepo::BlameReport report = GitRepo::blame(
      _repo.get(), {QStringLiteral("empty.txt"), QStringLiteral("crlf.txt"),
                    QStringLiteral("nonl.txt")});
  QVERIFY(report.valid);
  QVERIFY(report.fileInfos[0].lineInfos.isEmpty());
  QCOMPARE(report.fileInfos[1].lineInfos.size(), 2);
  QCOMPARE(report.fileInfos[1].lineInfos[0].content, QStringLiteral("a"));
  QCOMPARE(report.fileInfos[2].lineInfos.size(), 2);
  QCOMPARE(report.fileInfos[2].lineInfos[1].content, QStringLiteral("y"));
  // The extra line has no commit and an unknown author.
  const GitRepo::BlameLineInfo &extra = report.fileInfos[2].lineInfos.last();
  QVERIFY(extra.commit != kNullCommit);

  const GitRepo::BlameReport none = GitRepo::blame(_repo.get(), {});
  QVERIFY(none.valid);
  QVERIFY(none.fileInfos.isEmpty());
}

void GitRepoTest::fileSizes() {
  QVERIFY(writeFile(file("a.txt"), "12345"));
  const QString first = gitCommitAll(QStringLiteral("Five"));
  QVERIFY(writeFile(file("a.txt"), "1234567890"));
  gitCommitAll(QStringLiteral("Ten"));
  QVERIFY(writeFile(file("a.txt"), "local change"));

  QCOMPARE(GitRepo::getFileSizeFromCommit(_repo.get(), QStringLiteral("a.txt"),
                                          first),
           qint64(5));
  // An empty commit means HEAD, not the working copy.
  QCOMPARE(GitRepo::getFileSizeFromCommit(_repo.get(), QStringLiteral("a.txt"),
                                          QString()),
           qint64(10));
  QCOMPARE(GitRepo::getFileSizeFromCommit(_repo.get(),
                                          QStringLiteral("missing.txt"), first),
           qint64(-1));
  QCOMPARE(lastError(), GitError::GetFileSizeFromCommitFailedToLookupTreeEntryByPath);
  QCOMPARE(GitRepo::getFileSizeFromCommit(_repo.get(), QStringLiteral("a.txt"),
                                          QString(40, QLatin1Char('c'))),
           qint64(-1));
  QCOMPARE(GitRepo::getFileSizeFromCommit(_repo.get(), QStringLiteral("a.txt"),
                                          QStringLiteral("nonsense")),
           qint64(-1));
}

// History.

void GitRepoTest::historyPaging() {
  const ConfigOverride pageSize(ConfigManager::ConfigKey::GitRepoHistoryPageSize,
                                3);
  QStringList shas;
  for (int i = 1; i <= 6; ++i) {
    QVERIFY(writeFile(file("a.txt"), QByteArray::number(i)));
    shas.prepend(commitAll(QStringLiteral("Commit %1").arg(i)));
  }
  // Plus the initial commit.
  shas.append(git({QStringLiteral("rev-list"), QStringLiteral("--max-parents=0"),
                   QStringLiteral("HEAD")}));

  const GitRepo::GitHistory page0 = GitRepo::getBranchHistory(
      _repo.get(), QStringLiteral("main"), GitRepo::BranchType::Local, 0, _quit);
  QVERIFY(page0.valid);
  QCOMPARE(page0.total, qint64(7));
  QVERIFY(page0.hasMore);
  QCOMPARE(page0.commits, (QVector<QString>{shas[0], shas[1], shas[2]}));
  QCOMPARE(page0.headCommit, shas[0]);
  const GitRepo::GitCommitInfo &info = page0.commitInfos[shas[0]];
  QCOMPARE(info.summary, QStringLiteral("Commit 6"));
  QCOMPARE(info.message, QStringLiteral("Commit 6"));
  QCOMPARE(info.authorName, kAuthor);
  QCOMPARE(info.authorEmail, kEmail);
  QCOMPARE(info.parents, QVector<QString>{shas[1]});
  QCOMPARE(QString::fromLatin1(git_oid_tostr_s(&info.oid)), shas[0]);
  QVERIFY(!info.signedCommit);
  QVERIFY(qAbs(info.timeStamp - QDateTime::currentMSecsSinceEpoch()) <
          48LL * 3600 * 1000);

  const GitRepo::GitHistory page1 = GitRepo::getBranchHistory(
      _repo.get(), QStringLiteral("main"), GitRepo::BranchType::Local, 1, _quit);
  QCOMPARE(page1.commits, (QVector<QString>{shas[3], shas[4], shas[5]}));
  QVERIFY(page1.hasMore);
  // Ref tips and the head are filled on page 0 only.
  QVERIFY(page1.refTips.isEmpty());
  QVERIFY(page1.headCommit.isEmpty());

  const GitRepo::GitHistory page2 = GitRepo::getBranchHistory(
      _repo.get(), QStringLiteral("main"), GitRepo::BranchType::Local, 2, _quit);
  QCOMPARE(page2.commits, QVector<QString>{shas[6]});
  QVERIFY(!page2.hasMore);
  QVERIFY(page2.commitInfos[shas[6]].parents.isEmpty());

  // Past the end.
  const GitRepo::GitHistory page9 = GitRepo::getBranchHistory(
      _repo.get(), QStringLiteral("main"), GitRepo::BranchType::Local, 9, _quit);
  QVERIFY(page9.valid);
  QVERIFY(page9.commits.isEmpty());
  QVERIFY(!page9.hasMore);
  QCOMPARE(page9.total, qint64(7));

  // Page -1 is everything.
  const GitRepo::GitHistory all = GitRepo::getBranchHistory(
      _repo.get(), QStringLiteral("main"), GitRepo::BranchType::Local, -1, _quit);
  QCOMPARE(all.commits, QVector<QString>(shas.cbegin(), shas.cend()));
  QVERIFY(!all.hasMore);
}

void GitRepoTest::historyAllBranches() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString base = gitCommitAll(QStringLiteral("Base"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("feature")});
  QVERIFY(writeFile(file("f.txt"), "f\n"));
  const QString feature = gitCommitAll(QStringLiteral("Feature"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"),
       QStringLiteral("main")});
  QVERIFY(writeFile(file("m.txt"), "m\n"));
  const QString main = gitCommitAll(QStringLiteral("Main"));
  // A commit only reachable from a tag.
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("--detach"),
       base});
  QVERIFY(writeFile(file("t.txt"), "t\n"));
  const QString tagged = gitCommitAll(QStringLiteral("Tagged"));
  git({QStringLiteral("tag"), QStringLiteral("only-tag")});
  git({QStringLiteral("checkout"), QStringLiteral("-q"),
       QStringLiteral("main")});

  const GitRepo::GitHistory mainOnly = GitRepo::getBranchHistory(
      _repo.get(), QStringLiteral("main"), GitRepo::BranchType::Local, -1, _quit);
  QVERIFY(!mainOnly.commits.contains(feature));
  QVERIFY(!mainOnly.commits.contains(tagged));

  const GitRepo::GitHistory all = GitRepo::getBranchHistory(
      _repo.get(), QStringLiteral(ALL_BRANCHES), GitRepo::BranchType::Local, -1,
      _quit);
  QVERIFY(all.valid);
  QCOMPARE(all.total, qint64(5));
  for (const QString &sha : {base, feature, main, tagged}) {
    QVERIFY2(all.commits.contains(sha), qPrintable(sha));
  }
  // No duplicates and children come before their parents.
  QCOMPARE(QSet<QString>(all.commits.cbegin(), all.commits.cend()).size(),
           all.commits.size());
  QVERIFY(all.commits.indexOf(feature) < all.commits.indexOf(base));
  QVERIFY(all.commits.indexOf(main) < all.commits.indexOf(base));
  QVERIFY(all.commits.indexOf(tagged) < all.commits.indexOf(base));

  const GitRepo::GitHistory viaType = GitRepo::getBranchHistory(
      _repo.get(), QString(), GitRepo::BranchType::All, -1, _quit);
  QCOMPARE(viaType.total, all.total);
}

void GitRepoTest::historyRefTipsAndLabels() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString tip = gitCommitAll(QStringLiteral("A"));
  git({QStringLiteral("branch"), QStringLiteral("feature/x")});
  git({QStringLiteral("tag"), QStringLiteral("light")});
  git({QStringLiteral("tag"), QStringLiteral("-a"), QStringLiteral("-m"),
       QStringLiteral("Annotated"), QStringLiteral("v1.0")});
  // A fake remote tracking branch and its symbolic HEAD.
  git({QStringLiteral("update-ref"), QStringLiteral("refs/remotes/origin/main"),
       tip});
  git({QStringLiteral("symbolic-ref"), QStringLiteral("refs/remotes/origin/HEAD"),
       QStringLiteral("refs/remotes/origin/main")});

  const GitRepo::GitHistory history = GitRepo::getBranchHistory(
      _repo.get(), QStringLiteral("main"), GitRepo::BranchType::Local, 0, _quit);
  QVERIFY(history.valid);
  QCOMPARE(history.headCommit, tip);
  QCOMPARE(history.refTips.value(tip),
           (QStringList{QStringLiteral("feature/x"), QStringLiteral("main"),
                        QStringLiteral("light"), QStringLiteral("v1.0")}));
  QCOMPARE(history.refLabels.value(tip),
           (QStringList{QStringLiteral("refs/heads/feature/x"),
                        QStringLiteral("refs/heads/main"),
                        QStringLiteral("refs/remotes/origin/main"),
                        QStringLiteral("refs/tags/light"),
                        QStringLiteral("refs/tags/v1.0")}));
  QVERIFY(history.bisectMarks.isEmpty());
}

void GitRepoTest::historyBranchTypes() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString tip = gitCommitAll(QStringLiteral("A"));
  git({QStringLiteral("tag"), QStringLiteral("v1")});
  git({QStringLiteral("update-ref"), QStringLiteral("refs/remotes/origin/main"),
       tip});

  struct Case {
    QString name;
    GitRepo::BranchType type;
    bool valid;
    GitError error;
  };
  const QVector<Case> cases = {
      {QStringLiteral("main"), GitRepo::BranchType::Local, true, GitError::Unknown},
      {QStringLiteral("HEAD"), GitRepo::BranchType::Local, true, GitError::Unknown},
      {QStringLiteral("origin/main"), GitRepo::BranchType::Remote, true,
       GitError::Unknown},
      {QStringLiteral(ALL_BRANCHES), GitRepo::BranchType::Remote, true,
       GitError::Unknown},
      {QStringLiteral("v1"), GitRepo::BranchType::Tag, true, GitError::Unknown},
      {QStringLiteral("nope"), GitRepo::BranchType::Local, false,
       GitError::GetBranchHistoryFailedToAddLocalBranchToRevwalk},
      {QStringLiteral("origin/nope"), GitRepo::BranchType::Remote, false,
       GitError::GetBranchHistoryFailedToAddRemoteBranchToRevwalk},
      {QStringLiteral("nope"), GitRepo::BranchType::Tag, false,
       GitError::GetBranchHistoryFailedToAddTagToRevwalk},
      {QString(), GitRepo::BranchType::Local, false, GitError::Unknown},
      {QStringLiteral("main"), GitRepo::BranchType::Unknown, false,
       GitError::Unknown},
  };
  for (const Case &c : cases) {
    const GitRepo::GitHistory history = GitRepo::getBranchHistory(
        _repo.get(), c.name, c.type, 0, _quit);
    const QByteArray label =
        (c.name + '/' + QString::number(static_cast<int>(c.type))).toUtf8();
    QVERIFY2(history.valid == c.valid, label.constData());
    if (c.valid) {
      QVERIFY2(history.commits.contains(tip), label.constData());
    } else if (c.error != GitError::Unknown) {
      QVERIFY2(lastError() == c.error, label.constData());
    }
  }
}

void GitRepoTest::historyPageZeroRefreshesCursor() {
  const ConfigOverride pageSize(ConfigManager::ConfigKey::GitRepoHistoryPageSize,
                                2);
  for (int i = 0; i < 3; ++i) {
    QVERIFY(writeFile(file("a.txt"), QByteArray::number(i)));
    commitAll(QStringLiteral("C%1").arg(i));
  }
  const GitRepo::GitHistory before = GitRepo::getBranchHistory(
      _repo.get(), QStringLiteral("main"), GitRepo::BranchType::Local, 0, _quit);
  QCOMPARE(before.total, qint64(4));

  QVERIFY(writeFile(file("a.txt"), "new"));
  const QString newest = commitAll(QStringLiteral("Newest"));

  // Page 1 continues the history page 0 started.
  const GitRepo::GitHistory page1 = GitRepo::getBranchHistory(
      _repo.get(), QStringLiteral("main"), GitRepo::BranchType::Local, 1, _quit);
  QCOMPARE(page1.total, qint64(4));

  // Page 0 starts over and sees the new commit.
  const GitRepo::GitHistory after = GitRepo::getBranchHistory(
      _repo.get(), QStringLiteral("main"), GitRepo::BranchType::Local, 0, _quit);
  QCOMPARE(after.total, qint64(5));
  QCOMPARE(after.commits.first(), newest);

  // Another branch's page 1 does not reuse main's cursor.
  git({QStringLiteral("branch"), QStringLiteral("old"), QStringLiteral("HEAD~3")});
  const GitRepo::GitHistory other = GitRepo::getBranchHistory(
      _repo.get(), QStringLiteral("old"), GitRepo::BranchType::Local, 1, _quit);
  QVERIFY(other.valid);
  QCOMPARE(other.total, qint64(2));
  QVERIFY(other.commits.isEmpty());
}

void GitRepoTest::historyCancelled() {
  _quit = true;
  const GitRepo::GitHistory history = GitRepo::getBranchHistory(
      _repo.get(), QStringLiteral("main"), GitRepo::BranchType::Local, 0, _quit);
  QVERIFY(!history.valid);
  QVERIFY(GitRepo::getCommitBranches(_repo.get(), head(), _quit).isEmpty());
  _quit = false;
  QVERIFY(GitRepo::getBranchHistory(_repo.get(), QStringLiteral("main"),
                                    GitRepo::BranchType::Local, 0, _quit)
              .valid);
}

void GitRepoTest::historyMergeParents() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("A"));
  QStringList tips;
  for (const QString &branch :
       {QStringLiteral("b1"), QStringLiteral("b2"), QStringLiteral("b3")}) {
    git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
         branch, QStringLiteral("main")});
    QVERIFY(writeFile(file(branch + QStringLiteral(".txt")), "x\n"));
    tips += gitCommitAll(branch);
  }
  git({QStringLiteral("checkout"), QStringLiteral("-q"),
       QStringLiteral("main")});
  const QString mainTip = head();
  git({QStringLiteral("merge"), QStringLiteral("-q"), QStringLiteral("--no-ff"),
       QStringLiteral("-m"), QStringLiteral("Octopus"), QStringLiteral("b1"),
       QStringLiteral("b2"), QStringLiteral("b3")});

  const GitRepo::GitHistory history = GitRepo::getBranchHistory(
      _repo.get(), QStringLiteral("main"), GitRepo::BranchType::Local, 0, _quit);
  QVERIFY(history.valid);
  const GitRepo::GitCommitInfo octopus = history.commitInfos[history.commits[0]];
  QCOMPARE(octopus.parents,
           (QVector<QString>{mainTip, tips[0], tips[1], tips[2]}));

  const GitRepo::GitCommitStatus status =
      GitRepo::getCommitStatus(_repo.get(), history.commits[0]);
  QCOMPARE(status.parentShas, (QStringList{mainTip, tips[0], tips[1], tips[2]}));
}

void GitRepoTest::commitStatusFields() {
  QVERIFY(writeFile(file("modified.txt"), "1\n"));
  QVERIFY(writeFile(file("deleted.txt"), "d\n"));
  const QString parent = gitCommitAll(QStringLiteral("Base"));
  QVERIFY(writeFile(file("modified.txt"), "2\n"));
  QVERIFY(QFile::remove(file("deleted.txt")));
  QVERIFY(writeFile(file("dir/new.txt"), "n\n"));
  git({QStringLiteral("add"), QStringLiteral("-A")});
  git({QStringLiteral("-c"), QStringLiteral("user.name=") + kOtherAuthor,
       QStringLiteral("-c"), QStringLiteral("user.email=") + kOtherEmail,
       QStringLiteral("commit"), QStringLiteral("-qm"),
       QStringLiteral("Change\n\nWith a body")});
  const QString commit = head();

  const GitRepo::GitCommitStatus status =
      GitRepo::getCommitStatus(_repo.get(), commit);
  QCOMPARE(status.commit, commit);
  QCOMPARE(status.summary, QStringLiteral("Change"));
  QCOMPARE(status.authorName, kOtherAuthor);
  QCOMPARE(status.authorEmail, kOtherEmail);
  QCOMPARE(status.parentShas, QStringList{parent});
  QVERIFY(!status.committerDate.isEmpty());
  QCOMPARE(status.newFiles, QStringList{QStringLiteral("dir/new.txt")});
  QCOMPARE(status.modifiedFiles, QStringList{QStringLiteral("modified.txt")});
  QCOMPARE(status.deletedFiles, QStringList{QStringLiteral("deleted.txt")});
  QVERIFY(status.renamedFiles.isEmpty());
}

void GitRepoTest::commitStatusRootMergeAndRename() {
  const QString root = head();
  const GitRepo::GitCommitStatus rootStatus =
      GitRepo::getCommitStatus(_repo.get(), root);
  QCOMPARE(rootStatus.commit, root);
  QVERIFY(rootStatus.parentShas.isEmpty());
  QCOMPARE(rootStatus.summary, QStringLiteral("Initial commit"));

  QVERIFY(writeFile(file("old.txt"), "same content\nfor rename detection\n"));
  gitCommitAll(QStringLiteral("Old"));
  git({QStringLiteral("mv"), QStringLiteral("old.txt"), QStringLiteral("new.txt")});
  const QString renamed = gitCommitAll(QStringLiteral("Rename"));
  const GitRepo::GitCommitStatus renameStatus =
      GitRepo::getCommitStatus(_repo.get(), renamed);
  QCOMPARE(renameStatus.renamedFiles, QStringList{QStringLiteral("new.txt")});
  QVERIFY(renameStatus.newFiles.isEmpty());
  QVERIFY(renameStatus.deletedFiles.isEmpty());

  // A merge commit lists what changed against its first parent.
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("feature")});
  QVERIFY(writeFile(file("f.txt"), "f\n"));
  gitCommitAll(QStringLiteral("F"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"),
       QStringLiteral("main")});
  QVERIFY(writeFile(file("m.txt"), "m\n"));
  gitCommitAll(QStringLiteral("M"));
  git({QStringLiteral("merge"), QStringLiteral("-q"), QStringLiteral("--no-ff"),
       QStringLiteral("-m"), QStringLiteral("Merge"),
       QStringLiteral("feature")});
  const GitRepo::GitCommitStatus mergeStatus =
      GitRepo::getCommitStatus(_repo.get(), head());
  QCOMPARE(mergeStatus.parentShas.size(), 2);
  QCOMPARE(mergeStatus.newFiles, QStringList{QStringLiteral("f.txt")});
}

void GitRepoTest::commitStatusOfUnknownCommit_data() {
  QTest::addColumn<QString>("commit");
  QTest::newRow("unknown") << QString(40, QLatin1Char('d'));
  QTest::newRow("garbage") << QStringLiteral("garbage");
  QTest::newRow("empty") << QString();
}

void GitRepoTest::commitStatusOfUnknownCommit() {
  QFETCH(QString, commit);
  const GitRepo::GitCommitStatus status =
      GitRepo::getCommitStatus(_repo.get(), commit);
  QVERIFY(status.commit.isEmpty());
  QVERIFY(status.parentShas.isEmpty());
  QCOMPARE(lastError(), GitError::GetCommitStatusFailedToLookupCommit);
}

void GitRepoTest::commitBranchesTagsAndHeads() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString root = gitCommitAll(QStringLiteral("Root"));
  QVERIFY(writeFile(file("a.txt"), "a2\n"));
  const QString tip = gitCommitAll(QStringLiteral("Tip"));
  git({QStringLiteral("branch"), QStringLiteral("other"), root});
  git({QStringLiteral("tag"), QStringLiteral("light"), root});
  git({QStringLiteral("tag"), QStringLiteral("-a"), QStringLiteral("-m"),
       QStringLiteral("Annotated"), QStringLiteral("annotated"), root});

  // A branch tip is a head, a commit below it isn't.
  QCOMPARE(GitRepo::getCommitHeads(_repo.get(), tip, _quit),
           QStringList{QStringLiteral("main")});
  QCOMPARE(GitRepo::getCommitHeads(_repo.get(), root, _quit),
           QStringList{QStringLiteral("other")});
  QVERIFY(GitRepo::getCommitIsHead(_repo.get(), tip, _quit));
  QVERIFY(GitRepo::getCommitIsHead(_repo.get(), root, _quit));

  // A branch contains its tip as well as the commits below it.
  QCOMPARE(GitRepo::getCommitBranches(_repo.get(), tip, _quit),
           QStringList{QStringLiteral("main")});
  QCOMPARE(GitRepo::getCommitBranches(_repo.get(), root, _quit),
           (QStringList{QStringLiteral("main"), QStringLiteral("other")}));

  // Lightweight and annotated tags.
  QStringList tags = GitRepo::getCommitTags(_repo.get(), root, _quit);
  tags.sort();
  QCOMPARE(tags, (QStringList{QStringLiteral("annotated"), QStringLiteral("light")}));
  QVERIFY(GitRepo::getCommitTags(_repo.get(), tip, _quit).isEmpty());

  git({QStringLiteral("branch"), QStringLiteral("-D"), QStringLiteral("other")});
  QVERIFY(!GitRepo::getCommitIsHead(_repo.get(), root, _quit));
  QVERIFY(GitRepo::getCommitHeads(_repo.get(), root, _quit).isEmpty());

  // Unknown commits belong nowhere.
  const QString unknown(40, QLatin1Char('e'));
  QVERIFY(GitRepo::getCommitBranches(_repo.get(), unknown, _quit).isEmpty());
  QVERIFY(GitRepo::getCommitHeads(_repo.get(), unknown, _quit).isEmpty());
  QVERIFY(!GitRepo::getCommitIsHead(_repo.get(), unknown, _quit));
}

// Branches.

void GitRepoTest::createBranchFromCommit() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString root = gitCommitAll(QStringLiteral("Root"));
  QVERIFY(writeFile(file("a.txt"), "a2\n"));
  const QString tip = gitCommitAll(QStringLiteral("Tip"));

  QVERIFY2(GitRepo::createBranchFromCommit(_repo.get(), root,
                                           QStringLiteral("old")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(revParse(QStringLiteral("refs/heads/old")), root);
  // The new branch is not checked out.
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/main"));
  QCOMPARE(head(), tip);
  QCOMPARE(readFile(file("a.txt")), QByteArray("a2\n"));

  // An existing branch, also the current one, is not moved.
  QVERIFY(!GitRepo::createBranchFromCommit(_repo.get(), tip,
                                           QStringLiteral("old")));
  QCOMPARE(lastError(), GitError::CreateBranchFromCommitFailedToCreateBranch);
  QCOMPARE(revParse(QStringLiteral("refs/heads/old")), root);
  QVERIFY(!GitRepo::createBranchFromCommit(_repo.get(), root,
                                           QStringLiteral("main")));
  QCOMPARE(lastError(), GitError::CreateBranchFromCommitFailedToCreateBranch);
  QCOMPARE(head(), tip);

  // Only full commit ids are accepted.
  QVERIFY(!GitRepo::createBranchFromCommit(_repo.get(), root.left(7),
                                           QStringLiteral("short")));
  QCOMPARE(lastError(), GitError::CreateBranchFromCommitFailedToParseObjectId);
  QVERIFY(!GitRepo::createBranchFromCommit(
      _repo.get(), QStringLiteral("main"), QStringLiteral("byname")));
  QCOMPARE(lastError(), GitError::CreateBranchFromCommitFailedToParseObjectId);
  QVERIFY(!GitRepo::createBranchFromCommit(_repo.get(),
                                           QString(40, QLatin1Char('d')),
                                           QStringLiteral("unknown")));
  QCOMPARE(lastError(), GitError::CreateBranchFromCommitFailedToLookupCommit);
  // A tree is not a commit.
  QVERIFY(!GitRepo::createBranchFromCommit(
      _repo.get(), revParse(tip + QStringLiteral("^{tree}")),
      QStringLiteral("tree")));
  QCOMPARE(lastError(), GitError::CreateBranchFromCommitFailedToLookupCommit);

  QCOMPARE(localBranches(),
           (QStringList{QStringLiteral("main"), QStringLiteral("old")}));
}

void GitRepoTest::createBranchUpstream() {
  // Without any remote there is nothing to track.
  QVERIFY(GitRepo::createBranchFromCommit(_repo.get(), head(),
                                          QStringLiteral("alone")));
  bool ok = false;
  const QString remote =
      git({QStringLiteral("config"), QStringLiteral("branch.alone.remote")},
          &ok);
  QVERIFY2(!ok, qPrintable(remote));

  // With a remote, the branch tracks the remote branch of the same name, which
  // the first push creates.
  git({QStringLiteral("remote"), QStringLiteral("add"), QStringLiteral("origin"),
       QStringLiteral("https://example.invalid/repo.git")});
  QVERIFY(GitRepo::createBranchFromCommit(_repo.get(), head(),
                                          QStringLiteral("feature/new")));
  QCOMPARE(git({QStringLiteral("config"),
                QStringLiteral("branch.feature/new.remote")}),
           QStringLiteral("origin"));
  QCOMPARE(git({QStringLiteral("config"),
                QStringLiteral("branch.feature/new.merge")}),
           QStringLiteral("refs/heads/feature/new"));
  const auto [found, remoteName, remoteBranch] =
      GitRepo::getUpstream(_repo.get(), QStringLiteral("feature/new"));
  QVERIFY(found);
  QCOMPARE(remoteName, QStringLiteral("origin"));
  QCOMPARE(remoteBranch, QStringLiteral("feature/new"));
  // The configuration written by hand is still valid for git.
  git({QStringLiteral("config"), QStringLiteral("--list")}, &ok);
  QVERIFY(ok);
}

void GitRepoTest::createBranchInvalidNames_data() {
  QTest::addColumn<QString>("name");
  QTest::newRow("dot dot") << QStringLiteral("a..b");
  QTest::newRow("space") << QStringLiteral("a b");
  QTest::newRow("leading dash") << QStringLiteral("-x");
  QTest::newRow("lock suffix") << QStringLiteral("x.lock");
  QTest::newRow("trailing slash") << QStringLiteral("a/");
  QTest::newRow("HEAD") << QStringLiteral("HEAD");
  QTest::newRow("empty") << QString();
  QTest::newRow("special characters") << QStringLiteral("~^:");
  QTest::newRow("leading dot") << QStringLiteral(".hidden");
  QTest::newRow("dot component") << QStringLiteral("a/.b");
  QTest::newRow("trailing dot") << QStringLiteral("a.");
  QTest::newRow("at brace") << QStringLiteral("a@{b");
  QTest::newRow("asterisk") << QStringLiteral("a*");
  QTest::newRow("question mark") << QStringLiteral("a?");
  QTest::newRow("backslash") << QStringLiteral("a\\b");
  QTest::newRow("tab") << QStringLiteral("a\tb");
}

void GitRepoTest::createBranchInvalidNames() {
  QFETCH(QString, name);
  QVERIFY(!GitRepo::createBranchFromCommit(_repo.get(), head(), name));
  QCOMPARE(lastError(), GitError::CreateBranchFromCommitFailedToCreateBranch);
  QCOMPARE(git({QStringLiteral("for-each-ref"),
                QStringLiteral("--format=%(refname)")}),
           QStringLiteral("refs/heads/main"));
}

void GitRepoTest::branchNamesWithSlashesAndUnicode_data() {
  QTest::addColumn<QString>("name");
  QTest::newRow("nested") << QStringLiteral("feature/a/b");
  QTest::newRow("unicode") << QString::fromUtf16(u"ünï");
  QTest::newRow("cjk") << QString::fromUtf16(u"日本");
  QTest::newRow("dots and dashes") << QStringLiteral("release-1.2_rc");
  QTest::newRow("upper case") << QStringLiteral("Feature");
}

void GitRepoTest::branchNamesWithSlashesAndUnicode() {
  QFETCH(QString, name);
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString root = gitCommitAll(QStringLiteral("Root"));
  QVERIFY(writeFile(file("a.txt"), "a2\n"));
  gitCommitAll(QStringLiteral("Tip"));

  QVERIFY2(GitRepo::createBranchFromCommit(_repo.get(), root, name),
           qPrintable(lastErrorMessage()));
  QVERIFY(GitRepo::listLocalBranches(_repo.get(), _quit).contains(name));
  QCOMPARE(revParse(QStringLiteral("refs/heads/") + name), root);

  QVERIFY2(GitRepo::checkoutBranch(_repo.get(), name, GitRepo::BranchType::Local),
           qPrintable(lastErrorMessage()));
  QCOMPARE(GitRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local,
                                    _quit),
           name);
  QCOMPARE(readFile(file("a.txt")), QByteArray("a\n"));
  const GitRepo::GitHistory history = GitRepo::getBranchHistory(
      _repo.get(), name, GitRepo::BranchType::Local, 0, _quit);
  QVERIFY(history.valid);
  QCOMPARE(history.commits.first(), root);

  // Renamed while checked out, HEAD follows.
  const QString renamed = name + QStringLiteral("-renamed");
  QVERIFY2(GitRepo::renameBranch(_repo.get(), name, renamed),
           qPrintable(lastErrorMessage()));
  QCOMPARE(GitRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local,
                                    _quit),
           renamed);

  QVERIFY(GitRepo::checkoutBranch(_repo.get(), QStringLiteral("main"),
                                  GitRepo::BranchType::Local));
  QVERIFY2(GitRepo::deleteBranch(_repo.get(), renamed),
           qPrintable(lastErrorMessage()));
  QCOMPARE(localBranches(), QStringList{QStringLiteral("main")});
}

void GitRepoTest::checkoutLocalBranch() {
  QVERIFY(writeFile(file("a.txt"), "root\n"));
  const QString root = gitCommitAll(QStringLiteral("Root"));
  git({QStringLiteral("branch"), QStringLiteral("other")});
  QVERIFY(writeFile(file("a.txt"), "main\n"));
  QVERIFY(writeFile(file("sub/m.txt"), "m\n"));
  const QString mainTip = gitCommitAll(QStringLiteral("Main"));

  QVERIFY2(GitRepo::checkoutBranch(_repo.get(), QStringLiteral("other"),
                                   GitRepo::BranchType::Local),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/other"));
  QCOMPARE(head(), root);
  QCOMPARE(readFile(file("a.txt")), QByteArray("root\n"));
  QVERIFY(!QFileInfo::exists(file("sub/m.txt")));
  QVERIFY(!status().hasChanges());
  QCOMPARE(GitRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local,
                                    _quit),
           QStringLiteral("other"));

  // Checking out the current branch changes nothing.
  QVERIFY(GitRepo::checkoutBranch(_repo.get(), QStringLiteral("other"),
                                  GitRepo::BranchType::Local));
  QCOMPARE(head(), root);

  QVERIFY(GitRepo::checkoutBranch(_repo.get(), QStringLiteral("main"),
                                  GitRepo::BranchType::Local));
  QCOMPARE(head(), mainTip);
  QCOMPARE(readFile(file("sub/m.txt")), QByteArray("m\n"));

  // Only local, remote and tag checkouts are supported.
  QVERIFY(!GitRepo::checkoutBranch(_repo.get(), QStringLiteral("other"),
                                   GitRepo::BranchType::All));
  QVERIFY(!GitRepo::checkoutBranch(_repo.get(), QStringLiteral("other"),
                                   GitRepo::BranchType::Unknown));
  QCOMPARE(head(), mainTip);
}

void GitRepoTest::checkoutRemoteBranchCreatesTracking() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  const QString remoteTip = detachedCommit(QStringLiteral("r.txt"), "r\n",
                                           QStringLiteral("Remote"));
  git({QStringLiteral("remote"), QStringLiteral("add"), QStringLiteral("origin"),
       QStringLiteral("https://example.invalid/repo.git")});
  git({QStringLiteral("update-ref"), QStringLiteral("refs/remotes/origin/feature"),
       remoteTip});

  QVERIFY2(GitRepo::checkoutBranch(_repo.get(), QStringLiteral("origin/feature"),
                                   GitRepo::BranchType::Remote),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/feature"));
  QCOMPARE(head(), remoteTip);
  QCOMPARE(readFile(file("r.txt")), QByteArray("r\n"));
  QVERIFY(!status().hasChanges());
  QCOMPARE(git({QStringLiteral("rev-parse"), QStringLiteral("--abbrev-ref"),
                QStringLiteral("feature@{upstream}")}),
           QStringLiteral("origin/feature"));
  const auto [found, remoteName, remoteBranch] =
      GitRepo::getUpstream(_repo.get(), QStringLiteral("feature"));
  QVERIFY(found);
  QCOMPARE(remoteName, QStringLiteral("origin"));
  QCOMPARE(remoteBranch, QStringLiteral("feature"));
  QCOMPARE(GitRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local,
                                    _quit),
           QStringLiteral("feature"));
}

void GitRepoTest::checkoutNestedRemoteBranch() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  const QString remoteTip = detachedCommit(QStringLiteral("r.txt"), "r\n",
                                           QStringLiteral("Remote"));
  git({QStringLiteral("remote"), QStringLiteral("add"), QStringLiteral("origin"),
       QStringLiteral("https://example.invalid/repo.git")});
  git({QStringLiteral("update-ref"),
       QStringLiteral("refs/remotes/origin/feature/x"), remoteTip});

  QVERIFY2(GitRepo::checkoutBranch(_repo.get(),
                                   QStringLiteral("origin/feature/x"),
                                   GitRepo::BranchType::Remote),
           qPrintable(lastErrorMessage()));
  QCOMPARE(head(), remoteTip);
  QCOMPARE(git({QStringLiteral("rev-parse"), QStringLiteral("--abbrev-ref"),
                QStringLiteral("HEAD@{upstream}")}),
           QStringLiteral("origin/feature/x"));
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/feature/x"));
}

void GitRepoTest::checkoutRemoteBranchWhenLocalExists() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString base = gitCommitAll(QStringLiteral("Base"));
  const QString remoteTip = detachedCommit(QStringLiteral("r.txt"), "r\n",
                                           QStringLiteral("Remote"));
  git({QStringLiteral("remote"), QStringLiteral("add"), QStringLiteral("origin"),
       QStringLiteral("https://example.invalid/repo.git")});
  git({QStringLiteral("update-ref"), QStringLiteral("refs/remotes/origin/feature"),
       remoteTip});

  // The local branch is at the remote commit already.
  git({QStringLiteral("branch"), QStringLiteral("feature"), remoteTip});
  QVERIFY2(GitRepo::checkoutBranch(_repo.get(), QStringLiteral("origin/feature"),
                                   GitRepo::BranchType::Remote),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/feature"));
  QCOMPARE(head(), remoteTip);
  QCOMPARE(git({QStringLiteral("status"), QStringLiteral("--porcelain")}),
           QString());
  QCOMPARE(git({QStringLiteral("rev-parse"), QStringLiteral("--abbrev-ref"),
                QStringLiteral("feature@{upstream}")}),
           QStringLiteral("origin/feature"));

  // The local branch is behind the remote one. It is not moved.
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("main")});
  git({QStringLiteral("branch"), QStringLiteral("-f"), QStringLiteral("feature"),
       base});
  QVERIFY2(GitRepo::checkoutBranch(_repo.get(), QStringLiteral("origin/feature"),
                                   GitRepo::BranchType::Remote),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/feature"));
  QCOMPARE(head(), base);
  QCOMPARE(git({QStringLiteral("status"), QStringLiteral("--porcelain")}),
           QString());

  // An upstream the local branch has already is kept.
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("main")});
  git({QStringLiteral("update-ref"), QStringLiteral("refs/remotes/origin/other"),
       base});
  git({QStringLiteral("branch"), QStringLiteral("-q"), QStringLiteral("-u"),
       QStringLiteral("origin/other"), QStringLiteral("feature")});
  QVERIFY(GitRepo::checkoutBranch(_repo.get(), QStringLiteral("origin/feature"),
                                  GitRepo::BranchType::Remote));
  QCOMPARE(git({QStringLiteral("rev-parse"), QStringLiteral("--abbrev-ref"),
                QStringLiteral("feature@{upstream}")}),
           QStringLiteral("origin/other"));
}

void GitRepoTest::checkoutTagDetaches() {
  QVERIFY(writeFile(file("a.txt"), "1\n"));
  const QString root = gitCommitAll(QStringLiteral("Root"));
  git({QStringLiteral("tag"), QStringLiteral("light")});
  QVERIFY(writeFile(file("a.txt"), "2\n"));
  const QString middle = gitCommitAll(QStringLiteral("Middle"));
  QVERIFY(GitRepo::createTagFromCommit(_repo.get(), middle, QStringLiteral("v1"),
                                       kAuthor, kEmail,
                                       QStringLiteral("Release")));
  QVERIFY(writeFile(file("a.txt"), "3\n"));
  const QString tip = gitCommitAll(QStringLiteral("Tip"));

  QVERIFY2(GitRepo::checkoutBranch(_repo.get(), QStringLiteral("light"),
                                   GitRepo::BranchType::Tag),
           qPrintable(lastErrorMessage()));
  QCOMPARE(head(), root);
  bool attached = true;
  git({QStringLiteral("symbolic-ref"), QStringLiteral("-q"), QStringLiteral("HEAD")},
      &attached);
  QVERIFY(!attached);
  QCOMPARE(readFile(file("a.txt")), QByteArray("1\n"));
  QVERIFY(!status().hasChanges());
  QCOMPARE(GitRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Tag, _quit),
           QStringLiteral("light"));
  QCOMPARE(GitRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local,
                                    _quit),
           QStringLiteral("HEAD"));

  // An annotated tag detaches at the commit it points to.
  QVERIFY2(GitRepo::checkoutBranch(_repo.get(), QStringLiteral("v1"),
                                   GitRepo::BranchType::Tag),
           qPrintable(lastErrorMessage()));
  QCOMPARE(head(), middle);
  QCOMPARE(readFile(file("a.txt")), QByteArray("2\n"));
  QCOMPARE(GitRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Tag, _quit),
           QStringLiteral("v1"));

  QVERIFY(GitRepo::checkoutBranch(_repo.get(), QStringLiteral("main"),
                                  GitRepo::BranchType::Local));
  QCOMPARE(head(), tip);
  QCOMPARE(GitRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Tag, _quit),
           QString());
}

void GitRepoTest::checkoutMissingBranchFails_data() {
  QTest::addColumn<GitRepo::BranchType>("type");
  QTest::addColumn<QString>("name");
  QTest::addColumn<GitRepo::GitError>("error");
  QTest::newRow("local") << GitRepo::BranchType::Local << QStringLiteral("nope")
                         << GitError::CheckoutBranchFailedToLookupBranch;
  QTest::newRow("local empty") << GitRepo::BranchType::Local << QString()
                               << GitError::CheckoutBranchFailedToLookupBranch;
  QTest::newRow("local remote name")
      << GitRepo::BranchType::Local << QStringLiteral("origin/main")
      << GitError::CheckoutBranchFailedToLookupBranch;
  QTest::newRow("remote") << GitRepo::BranchType::Remote
                          << QStringLiteral("origin/nope")
                          << GitError::CheckoutBranchFailedToLookupReference;
  QTest::newRow("remote local name")
      << GitRepo::BranchType::Remote << QStringLiteral("main")
      << GitError::CheckoutBranchFailedToLookupReference;
  QTest::newRow("tag") << GitRepo::BranchType::Tag << QStringLiteral("nope")
                       << GitError::CheckoutBranchFailedToResolveRevision;
}

void GitRepoTest::checkoutMissingBranchFails() {
  QFETCH(GitRepo::BranchType, type);
  QFETCH(QString, name);
  QFETCH(GitRepo::GitError, error);

  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString before = gitCommitAll(QStringLiteral("Base"));
  git({QStringLiteral("update-ref"), QStringLiteral("refs/remotes/origin/main"),
       before});

  QVERIFY(!GitRepo::checkoutBranch(_repo.get(), name, type));
  QCOMPARE(lastError(), error);
  QCOMPARE(head(), before);
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/main"));
  QCOMPARE(localBranches(), QStringList{QStringLiteral("main")});
}

void GitRepoTest::checkoutWithDirtyWorkdir() {
  QVERIFY(writeFile(file("a.txt"), "base\n"));
  QVERIFY(writeFile(file("n.txt"), "n\n"));
  gitCommitAll(QStringLiteral("Base"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("other")});
  QVERIFY(writeFile(file("a.txt"), "other\n"));
  QVERIFY(writeFile(file("o.txt"), "o\n"));
  gitCommitAll(QStringLiteral("Other"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("main")});

  // A local change to a file that differs between the branches blocks the
  // checkout and nothing is touched.
  QVERIFY(writeFile(file("a.txt"), "local\n"));
  QVERIFY(!GitRepo::checkoutBranch(_repo.get(), QStringLiteral("other"),
                                   GitRepo::BranchType::Local));
  QCOMPARE(lastError(), GitError::CheckoutBranchFailedToCheckoutLocalBranchTree);
  QCOMPARE(readFile(file("a.txt")), QByteArray("local\n"));
  QVERIFY(!QFileInfo::exists(file("o.txt")));
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/main"));

  // Other local changes are carried over to the other branch.
  QVERIFY(writeFile(file("a.txt"), "base\n"));
  QVERIFY(writeFile(file("n.txt"), "dirty\n"));
  QVERIFY(writeFile(file("u.txt"), "untracked\n"));
  QVERIFY2(GitRepo::checkoutBranch(_repo.get(), QStringLiteral("other"),
                                   GitRepo::BranchType::Local),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/other"));
  QCOMPARE(readFile(file("a.txt")), QByteArray("other\n"));
  QCOMPARE(readFile(file("o.txt")), QByteArray("o\n"));
  QCOMPARE(readFile(file("n.txt")), QByteArray("dirty\n"));
  QCOMPARE(readFile(file("u.txt")), QByteArray("untracked\n"));
  const GitRepo::GitStatus st = status();
  QCOMPARE(st.modifiedFiles, QStringList{QStringLiteral("n.txt")});
  QCOMPARE(st.newFiles, QStringList{QStringLiteral("u.txt")});
}

void GitRepoTest::renameBranch() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Root"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("topic")});
  git({QStringLiteral("config"), QStringLiteral("branch.topic.remote"),
       QStringLiteral("origin")});
  git({QStringLiteral("config"), QStringLiteral("branch.topic.merge"),
       QStringLiteral("refs/heads/topic")});

  // The current branch: HEAD and the branch configuration follow.
  QVERIFY2(GitRepo::renameBranch(_repo.get(), QStringLiteral("topic"),
                                 QStringLiteral("renamed")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/renamed"));
  QCOMPARE(git({QStringLiteral("config"), QStringLiteral("branch.renamed.remote")}),
           QStringLiteral("origin"));
  QCOMPARE(git({QStringLiteral("config"), QStringLiteral("branch.renamed.merge")}),
           QStringLiteral("refs/heads/topic"));
  bool ok = true;
  git({QStringLiteral("config"), QStringLiteral("branch.topic.remote")}, &ok);
  QVERIFY(!ok);
  QCOMPARE(localBranches(),
           (QStringList{QStringLiteral("main"), QStringLiteral("renamed")}));

  // Onto an existing branch.
  QVERIFY(!GitRepo::renameBranch(_repo.get(), QStringLiteral("renamed"),
                                 QStringLiteral("main")));
  QCOMPARE(lastError(), GitError::RenameBranchFailedToRenameBranch);
  QCOMPARE(localBranches(),
           (QStringList{QStringLiteral("main"), QStringLiteral("renamed")}));
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/renamed"));

  // A branch that doesn't exist.
  QVERIFY(!GitRepo::renameBranch(_repo.get(), QStringLiteral("nope"),
                                 QStringLiteral("other")));
  QCOMPARE(lastError(), GitError::RenameBranchFailedToLookupBranch);

  // A branch that is not checked out.
  QVERIFY2(GitRepo::renameBranch(_repo.get(), QStringLiteral("main"),
                                 QStringLiteral("trunk")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(localBranches(),
           (QStringList{QStringLiteral("renamed"), QStringLiteral("trunk")}));
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/renamed"));
}

void GitRepoTest::renameBranchInvalidNames_data() {
  QTest::addColumn<QString>("name");
  QTest::newRow("HEAD") << QStringLiteral("HEAD");
  QTest::newRow("leading dash") << QStringLiteral("-x");
  QTest::newRow("dot dot") << QStringLiteral("a..b");
  QTest::newRow("empty") << QString();
  QTest::newRow("space") << QStringLiteral("a b");
  QTest::newRow("lock suffix") << QStringLiteral("x.lock");
}

void GitRepoTest::renameBranchInvalidNames() {
  QFETCH(QString, name);
  git({QStringLiteral("branch"), QStringLiteral("topic")});

  const bool renamed =
      GitRepo::renameBranch(_repo.get(), QStringLiteral("topic"), name);
  QVERIFY(!renamed);
  QCOMPARE(lastError(), GitError::RenameBranchInvalidName);
  QCOMPARE(localBranches(),
           (QStringList{QStringLiteral("main"), QStringLiteral("topic")}));
}

void GitRepoTest::deleteBranch() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString base = gitCommitAll(QStringLiteral("Root"));
  git({QStringLiteral("branch"), QStringLiteral("merged")});
  git({QStringLiteral("config"), QStringLiteral("branch.merged.remote"),
       QStringLiteral("origin")});
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("unmerged")});
  QVERIFY(writeFile(file("u.txt"), "u\n"));
  gitCommitAll(QStringLiteral("Unmerged"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("main")});

  // Not the current branch.
  QVERIFY(!GitRepo::deleteBranch(_repo.get(), QStringLiteral("main")));
  QCOMPARE(lastError(), GitError::DeleteBranchFailedToDeleteBranch);
  QCOMPARE(head(), base);

  // Its configuration goes with it.
  QVERIFY2(GitRepo::deleteBranch(_repo.get(), QStringLiteral("merged")),
           qPrintable(lastErrorMessage()));
  bool ok = true;
  git({QStringLiteral("config"), QStringLiteral("branch.merged.remote")}, &ok);
  QVERIFY(!ok);

  // Like `git branch -d`, a branch with commits nowhere else needs force.
  QVERIFY(!GitRepo::deleteBranch(_repo.get(), QStringLiteral("unmerged")));
  QCOMPARE(lastError(), GitError::DeleteBranchNotFullyMerged);
  QCOMPARE(localBranches(),
           (QStringList{QStringLiteral("main"), QStringLiteral("unmerged")}));
  // Merged into its upstream is enough, even when HEAD doesn't have it.
  git({QStringLiteral("remote"), QStringLiteral("add"), QStringLiteral("origin"),
       QStringLiteral("https://example.invalid/repo.git")});
  git({QStringLiteral("update-ref"), QStringLiteral("refs/remotes/origin/unmerged"),
       QStringLiteral("unmerged")});
  git({QStringLiteral("config"), QStringLiteral("branch.unmerged.remote"),
       QStringLiteral("origin")});
  git({QStringLiteral("config"), QStringLiteral("branch.unmerged.merge"),
       QStringLiteral("refs/heads/unmerged")});
  git({QStringLiteral("branch"), QStringLiteral("pushed"), QStringLiteral("unmerged")});
  git({QStringLiteral("config"), QStringLiteral("branch.pushed.remote"),
       QStringLiteral("origin")});
  git({QStringLiteral("config"), QStringLiteral("branch.pushed.merge"),
       QStringLiteral("refs/heads/unmerged")});
  QVERIFY2(GitRepo::deleteBranch(_repo.get(), QStringLiteral("pushed")),
           qPrintable(lastErrorMessage()));
  git({QStringLiteral("update-ref"), QStringLiteral("-d"),
       QStringLiteral("refs/remotes/origin/unmerged")});
  QVERIFY2(GitRepo::deleteBranch(_repo.get(), QStringLiteral("unmerged"), true),
           qPrintable(lastErrorMessage()));
  QCOMPARE(localBranches(), QStringList{QStringLiteral("main")});

  QVERIFY(!GitRepo::deleteBranch(_repo.get(), QStringLiteral("nope")));
  QCOMPARE(lastError(), GitError::DeleteBranchFailedToLookupBranch);

  // Remote-tracking branches are not local branches.
  git({QStringLiteral("update-ref"), QStringLiteral("refs/remotes/origin/main"),
       base});
  QVERIFY(!GitRepo::deleteBranch(_repo.get(), QStringLiteral("origin/main")));
  QCOMPARE(lastError(), GitError::DeleteBranchFailedToLookupBranch);
  QCOMPARE(revParse(QStringLiteral("refs/remotes/origin/main")), base);

  // A branch checked out in a linked worktree stays.
  git({QStringLiteral("worktree"), QStringLiteral("add"), QStringLiteral("-q"),
       QStringLiteral("-b"), QStringLiteral("wtbranch"),
       _base + QStringLiteral("/wt")});
  QVERIFY(!GitRepo::deleteBranch(_repo.get(), QStringLiteral("wtbranch")));
  QCOMPARE(lastError(), GitError::DeleteBranchFailedToDeleteBranch);
  QCOMPARE(localBranches(),
           (QStringList{QStringLiteral("main"), QStringLiteral("wtbranch")}));
}

void GitRepoTest::activeBranchQueries() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString root = gitCommitAll(QStringLiteral("Root"));
  QVERIFY(writeFile(file("a.txt"), "a2\n"));
  gitCommitAll(QStringLiteral("Tip"));

  QCOMPARE(GitRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local,
                                    _quit),
           QStringLiteral("main"));
  // The active remote branch is the upstream of the current branch.
  git({QStringLiteral("remote"), QStringLiteral("add"), QStringLiteral("origin"),
       QStringLiteral("https://example.invalid/repo.git")});
  git({QStringLiteral("update-ref"), QStringLiteral("refs/remotes/origin/main"),
       QStringLiteral("HEAD")});
  git({QStringLiteral("branch"), QStringLiteral("-q"), QStringLiteral("-u"),
       QStringLiteral("origin/main")});
  QCOMPARE(GitRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Remote,
                                    _quit),
           QStringLiteral("origin/main"));
  QCOMPARE(GitRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::All,
                                    _quit),
           QString());
  QCOMPARE(GitRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Unknown,
                                    _quit),
           QString());
  QCOMPARE(GitRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Tag, _quit),
           QString());

  // A tag on the tip of the current branch isn't checked out either.
  git({QStringLiteral("tag"), QStringLiteral("tiptag")});
  QCOMPARE(GitRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Tag, _quit),
           QString());

  // Detached HEAD.
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("--detach"),
       root});
  QCOMPARE(GitRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Remote,
                                    _quit),
           QString());
  QCOMPARE(GitRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local,
                                    _quit),
           QStringLiteral("HEAD"));
  QCOMPARE(GitRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Tag, _quit),
           QString());
  git({QStringLiteral("tag"), QStringLiteral("roottag"), root});
  QCOMPARE(GitRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Tag, _quit),
           QStringLiteral("roottag"));
}

void GitRepoTest::listLocalAndRemoteBranches() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString base = gitCommitAll(QStringLiteral("Root"));
  for (const QString &name :
       {QStringLiteral("zeta"), QStringLiteral("Alpha"), QStringLiteral("beta/x"),
        QString::fromUtf16(u"ünï")}) {
    git({QStringLiteral("branch"), name});
  }

  // Sorted by the bytes of the name.
  const QStringList expected{QStringLiteral("Alpha"), QStringLiteral("beta/x"),
                             QStringLiteral("main"), QStringLiteral("zeta"),
                             QString::fromUtf16(u"ünï")};
  QCOMPARE(GitRepo::listLocalBranches(_repo.get(), _quit), expected);
  // Packed references are listed the same way.
  git({QStringLiteral("pack-refs"), QStringLiteral("--all")});
  QCOMPARE(GitRepo::listLocalBranches(_repo.get(), _quit), expected);

  // A detached HEAD is listed first.
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("--detach")});
  QCOMPARE(GitRepo::listLocalBranches(_repo.get(), _quit),
           QStringList{QStringLiteral("HEAD")} + expected);
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("main")});

  QVERIFY(GitRepo::listRemoteBranches(_repo.get(), _quit).isEmpty());
  git({QStringLiteral("remote"), QStringLiteral("add"), QStringLiteral("origin"),
       QStringLiteral("https://example.invalid/repo.git")});
  for (const QString &ref : {QStringLiteral("refs/remotes/origin/main"),
                             QStringLiteral("refs/remotes/origin/zeta"),
                             QStringLiteral("refs/remotes/upstream/main")}) {
    git({QStringLiteral("update-ref"), ref, base});
  }
  git({QStringLiteral("symbolic-ref"), QStringLiteral("refs/remotes/origin/HEAD"),
       QStringLiteral("refs/remotes/origin/main")});
  // origin/HEAD only points to a branch, like `git branch -r` shows.
  QCOMPARE(GitRepo::listRemoteBranches(_repo.get(), _quit),
           (QStringList{QStringLiteral("origin/main"), QStringLiteral("origin/zeta"),
                        QStringLiteral("upstream/main")}));
  // Loose and packed references together are sorted as well.
  git({QStringLiteral("branch"), QStringLiteral("loose")});
  QStringList withLoose = expected;
  withLoose.insert(2, QStringLiteral("loose"));
  QCOMPARE(GitRepo::listLocalBranches(_repo.get(), _quit), withLoose);
  git({QStringLiteral("branch"), QStringLiteral("-D"), QStringLiteral("loose")});
  QCOMPARE(GitRepo::listLocalBranches(_repo.get(), _quit), expected);

  // Cancelled.
  _quit = true;
  QVERIFY(GitRepo::listLocalBranches(_repo.get(), _quit).isEmpty());
  QVERIFY(GitRepo::listRemoteBranchesWithStatus(_repo.get(), _quit).isEmpty());
}

void GitRepoTest::remoteBranchStatus() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString c0 = gitCommitAll(QStringLiteral("Base"));
  const QString c1 = emptyCommit(c0, QStringLiteral("1"));
  const QString c2 = emptyCommit(c1, QStringLiteral("2"));
  const QString c3 = emptyCommit(c2, QStringLiteral("3"));
  const QString x1 = emptyCommit(c0, QStringLiteral("x1"));
  git({QStringLiteral("remote"), QStringLiteral("add"), QStringLiteral("origin"),
       QStringLiteral("https://example.invalid/repo.git")});
  const auto setRef = [this](const QString &ref, const QString &id) {
    git({QStringLiteral("update-ref"), ref, id});
  };
  setRef(QStringLiteral("refs/heads/same"), c0);
  setRef(QStringLiteral("refs/remotes/origin/same"), c0);
  setRef(QStringLiteral("refs/heads/ahead"), c2);
  setRef(QStringLiteral("refs/remotes/origin/ahead"), c0);
  setRef(QStringLiteral("refs/heads/behind"), c0);
  setRef(QStringLiteral("refs/remotes/origin/behind"), c3);
  setRef(QStringLiteral("refs/heads/diverged"), x1);
  setRef(QStringLiteral("refs/remotes/origin/diverged"), c2);
  setRef(QStringLiteral("refs/remotes/origin/lonely"), c0);
  setRef(QStringLiteral("refs/heads/feature/x"), c0);
  setRef(QStringLiteral("refs/remotes/origin/feature/x"), c0);
  git({QStringLiteral("symbolic-ref"), QStringLiteral("refs/remotes/origin/HEAD"),
       QStringLiteral("refs/remotes/origin/same")});
  // A local branch with another name that tracks the remote branch.
  setRef(QStringLiteral("refs/heads/mine"), c1);
  setRef(QStringLiteral("refs/remotes/origin/tracked"), c0);
  git({QStringLiteral("branch"), QStringLiteral("-q"), QStringLiteral("-u"),
       QStringLiteral("origin/tracked"), QStringLiteral("mine")});

  const QVector<GitRepo::BranchInfo> infos =
      GitRepo::listRemoteBranchesWithStatus(_repo.get(), _quit);
  const auto statusOf = [&infos](const QString &name) {
    for (const GitRepo::BranchInfo &info : infos) {
      if (info.name == name) {
        return QStringLiteral("%1 %2 %3")
            .arg(static_cast<int>(info.statusInfo.status))
            .arg(info.statusInfo.aheadCount)
            .arg(info.statusInfo.behindCount);
      }
    }
    return QStringLiteral("missing");
  };
  const auto expected = [](GitRepo::BranchStatusType type, int ahead,
                           int behind) {
    return QStringLiteral("%1 %2 %3")
        .arg(static_cast<int>(type))
        .arg(ahead)
        .arg(behind);
  };
  QCOMPARE(infos.size(), 7);
  QCOMPARE(statusOf(QStringLiteral("origin/tracked")),
           expected(GitRepo::BranchStatusType::Ahead, 1, 0));
  QCOMPARE(statusOf(QStringLiteral("origin/same")),
           expected(GitRepo::BranchStatusType::UpToDate, 0, 0));
  QCOMPARE(statusOf(QStringLiteral("origin/ahead")),
           expected(GitRepo::BranchStatusType::Ahead, 2, 0));
  QCOMPARE(statusOf(QStringLiteral("origin/behind")),
           expected(GitRepo::BranchStatusType::Behind, 0, 3));
  QCOMPARE(statusOf(QStringLiteral("origin/diverged")),
           expected(GitRepo::BranchStatusType::Diverged, 1, 2));
  QCOMPARE(statusOf(QStringLiteral("origin/lonely")),
           expected(GitRepo::BranchStatusType::NoUpstream, 0, 0));
  // A symbolic reference is not a branch.
  QCOMPARE(statusOf(QStringLiteral("origin/HEAD")), QStringLiteral("missing"));
  QCOMPARE(statusOf(QStringLiteral("origin/feature/x")),
           expected(GitRepo::BranchStatusType::UpToDate, 0, 0));
}

// Tags.

void GitRepoTest::createTagIsAnnotated() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString root = gitCommitAll(QStringLiteral("Root"));
  QVERIFY(writeFile(file("a.txt"), "a2\n"));
  const QString tip = gitCommitAll(QStringLiteral("Tip"));

  QVERIFY2(GitRepo::createTagFromCommit(_repo.get(), root,
                                        QStringLiteral("v1.0"), kOtherAuthor,
                                        kOtherEmail,
                                        QStringLiteral("Release 1.0\n\nNotes")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("cat-file"), QStringLiteral("-t"),
                QStringLiteral("v1.0")}),
           QStringLiteral("tag"));
  QCOMPARE(git({QStringLiteral("for-each-ref"),
                QStringLiteral("--format=%(taggername)|%(taggeremail)|%(contents:"
                               "subject)|%(*objectname)"),
                QStringLiteral("refs/tags/v1.0")}),
           QStringLiteral("Other Author|<other@example.com>|Release 1.0|") + root);
  QCOMPARE(git({QStringLiteral("tag"), QStringLiteral("-l"),
                QStringLiteral("--format=%(contents:body)"),
                QStringLiteral("v1.0")}),
           QStringLiteral("Notes"));
  QCOMPARE(GitRepo::listTags(_repo.get()), QStringList{QStringLiteral("v1.0")});
  QCOMPARE(GitRepo::getCommitTags(_repo.get(), root, _quit),
           QStringList{QStringLiteral("v1.0")});
  // Tagging moves nothing.
  QCOMPARE(head(), tip);
  QVERIFY(!status().hasChanges());

  // A unicode name and an empty message.
  const QString unicode = QString::fromUtf16(u"версия");
  QVERIFY2(GitRepo::createTagFromCommit(_repo.get(), tip, unicode, kAuthor,
                                        kEmail, QString()),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("cat-file"), QStringLiteral("-t"), unicode}),
           QStringLiteral("tag"));
  QCOMPARE(revParse(unicode + QStringLiteral("^{commit}")), tip);
}

void GitRepoTest::createTagFailures_data() {
  QTest::addColumn<QString>("name");
  QTest::addColumn<QString>("commit");
  QTest::addColumn<GitRepo::GitError>("error");
  const GitError invalid = GitError::CreateTagFromCommitFailedToPeelCommitForTag;
  const QString atHead = QStringLiteral("head");
  QTest::newRow("duplicate") << QStringLiteral("dup") << atHead << invalid;
  QTest::newRow("dot dot") << QStringLiteral("a..b") << atHead << invalid;
  QTest::newRow("space") << QStringLiteral("a b") << atHead << invalid;
  QTest::newRow("lock suffix") << QStringLiteral("x.lock") << atHead << invalid;
  QTest::newRow("empty") << QString() << atHead << invalid;
  QTest::newRow("tilde") << QStringLiteral("a~1") << atHead << invalid;
  QTest::newRow("colon") << QStringLiteral("a:b") << atHead << invalid;
  QTest::newRow("trailing slash") << QStringLiteral("a/") << atHead << invalid;
  QTest::newRow("short id") << QStringLiteral("ok") << QStringLiteral("short")
                            << GitError::CreateTagFromCommitFailedToParseObjectId;
  QTest::newRow("branch name")
      << QStringLiteral("ok") << QStringLiteral("main")
      << GitError::CreateTagFromCommitFailedToParseObjectId;
  QTest::newRow("unknown commit")
      << QStringLiteral("ok") << QString(40, QLatin1Char('d'))
      << GitError::CreateTagFromCommitFailedToLookupCommit;
  QTest::newRow("tree") << QStringLiteral("ok") << QStringLiteral("tree")
                        << GitError::CreateTagFromCommitFailedToLookupCommit;
}

void GitRepoTest::createTagFailures() {
  QFETCH(QString, name);
  QFETCH(QString, commit);
  QFETCH(GitRepo::GitError, error);

  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString tip = gitCommitAll(QStringLiteral("Tip"));
  git({QStringLiteral("tag"), QStringLiteral("dup")});
  if (commit == QStringLiteral("head")) {
    commit = tip;
  } else if (commit == QStringLiteral("short")) {
    commit = tip.left(7);
  } else if (commit == QStringLiteral("tree")) {
    commit = revParse(QStringLiteral("HEAD^{tree}"));
  }

  QVERIFY(!GitRepo::createTagFromCommit(_repo.get(), commit, name, kAuthor,
                                        kEmail, QStringLiteral("Message")));
  QCOMPARE(lastError(), error);
  QCOMPARE(git({QStringLiteral("tag"), QStringLiteral("-l")}),
           QStringLiteral("dup"));
  QCOMPARE(git({QStringLiteral("cat-file"), QStringLiteral("-t"),
                QStringLiteral("dup")}),
           QStringLiteral("commit"));
}

void GitRepoTest::renameTagKeepsKind_data() {
  QTest::addColumn<bool>("annotated");
  QTest::newRow("annotated") << true;
  QTest::newRow("lightweight") << false;
}

void GitRepoTest::renameTagKeepsKind() {
  QFETCH(bool, annotated);
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString root = gitCommitAll(QStringLiteral("Root"));
  QVERIFY(writeFile(file("a.txt"), "a2\n"));
  gitCommitAll(QStringLiteral("Tip"));
  if (annotated) {
    QVERIFY(GitRepo::createTagFromCommit(_repo.get(), root, QStringLiteral("old"),
                                         kOtherAuthor, kOtherEmail,
                                         QStringLiteral("Message")));
  } else {
    git({QStringLiteral("tag"), QStringLiteral("old"), root});
  }

  QVERIFY2(GitRepo::renameTag(_repo.get(), QStringLiteral("old"),
                              QStringLiteral("new")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("tag"), QStringLiteral("-l")}),
           QStringLiteral("new"));
  QCOMPARE(git({QStringLiteral("cat-file"), QStringLiteral("-t"),
                QStringLiteral("new")}),
           annotated ? QStringLiteral("tag") : QStringLiteral("commit"));
  QCOMPARE(revParse(QStringLiteral("new^{commit}")), root);
  QCOMPARE(GitRepo::getCommitTags(_repo.get(), root, _quit),
           QStringList{QStringLiteral("new")});
  if (annotated) {
    // Tagger and message are kept, the tag object carries the new name.
    QCOMPARE(git({QStringLiteral("for-each-ref"),
                  QStringLiteral("--format=%(taggername)|%(contents:subject)"),
                  QStringLiteral("refs/tags/new")}),
             QStringLiteral("Other Author|Message"));
    QVERIFY(git({QStringLiteral("cat-file"), QStringLiteral("-p"),
                 QStringLiteral("new")})
                .contains(QStringLiteral("\ntag new\n")));
  }
}

void GitRepoTest::renameTagFailures() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString tip = gitCommitAll(QStringLiteral("Tip"));
  git({QStringLiteral("tag"), QStringLiteral("light")});
  QVERIFY(GitRepo::createTagFromCommit(_repo.get(), tip, QStringLiteral("annot"),
                                       kAuthor, kEmail,
                                       QStringLiteral("Message")));

  QVERIFY(!GitRepo::renameTag(_repo.get(), QStringLiteral("nope"),
                              QStringLiteral("x")));
  QCOMPARE(lastError(), GitError::RenameTagFailedToLookupReference);
  QVERIFY(!GitRepo::renameTag(_repo.get(), QStringLiteral("light"),
                              QStringLiteral("annot")));
  QCOMPARE(lastError(), GitError::RenameTagFailedToCreateLightweightTag);
  QVERIFY(!GitRepo::renameTag(_repo.get(), QStringLiteral("annot"),
                              QStringLiteral("light")));
  QCOMPARE(lastError(), GitError::RenameTagFailedToCreateAnnotatedTag);
  QVERIFY(!GitRepo::renameTag(_repo.get(), QStringLiteral("light"),
                              QStringLiteral("light")));
  QCOMPARE(lastError(), GitError::RenameTagFailedToCreateLightweightTag);
  QVERIFY(!GitRepo::renameTag(_repo.get(), QStringLiteral("light"),
                              QStringLiteral("a..b")));
  QCOMPARE(lastError(), GitError::RenameTagFailedToCreateLightweightTag);
  QVERIFY(!GitRepo::renameTag(_repo.get(), QStringLiteral("annot"),
                              QString()));
  QCOMPARE(lastError(), GitError::RenameTagFailedToCreateAnnotatedTag);

  QCOMPARE(git({QStringLiteral("tag"), QStringLiteral("-l")}),
           QStringLiteral("annot\nlight"));
  QCOMPARE(git({QStringLiteral("cat-file"), QStringLiteral("-t"),
                QStringLiteral("annot")}),
           QStringLiteral("tag"));
}

void GitRepoTest::deleteTag() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString tip = gitCommitAll(QStringLiteral("Tip"));
  git({QStringLiteral("tag"), QStringLiteral("light")});
  QVERIFY(GitRepo::createTagFromCommit(_repo.get(), tip, QStringLiteral("annot"),
                                       kAuthor, kEmail,
                                       QStringLiteral("Message")));
  git({QStringLiteral("branch"), QStringLiteral("same")});

  QVERIFY2(GitRepo::deleteTag(_repo.get(), QStringLiteral("light")),
           qPrintable(lastErrorMessage()));
  QVERIFY2(GitRepo::deleteTag(_repo.get(), QStringLiteral("annot")),
           qPrintable(lastErrorMessage()));
  QVERIFY(GitRepo::listTags(_repo.get()).isEmpty());
  QCOMPARE(head(), tip);

  QVERIFY(!GitRepo::deleteTag(_repo.get(), QStringLiteral("light")));
  QCOMPARE(lastError(), GitError::DeleteTagFailedToDeleteTag);
  QVERIFY(!GitRepo::deleteTag(_repo.get(), QString()));
  QCOMPARE(lastError(), GitError::DeleteTagFailedToDeleteTag);
  // A branch is not a tag.
  QVERIFY(!GitRepo::deleteTag(_repo.get(), QStringLiteral("same")));
  QCOMPARE(lastError(), GitError::DeleteTagFailedToDeleteTag);
  QCOMPARE(localBranches(),
           (QStringList{QStringLiteral("main"), QStringLiteral("same")}));
}

void GitRepoTest::listTags() {
  QVERIFY(GitRepo::listTags(_repo.get()).isEmpty());
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Tip"));
  for (const QString &name :
       {QStringLiteral("v10"), QStringLiteral("v9"), QStringLiteral("Alpha"),
        QStringLiteral("beta/x"), QString::fromUtf16(u"ünï")}) {
    git({QStringLiteral("tag"), name});
  }

  QStringList expected{QStringLiteral("Alpha"), QStringLiteral("beta/x"),
                       QStringLiteral("v10"), QStringLiteral("v9"),
                       QString::fromUtf16(u"ünï")};
  // Loose tags are sorted by the bytes of the name, not by version.
  QCOMPARE(GitRepo::listTags(_repo.get()), expected);

  // Loose and packed tags together are sorted as well.
  git({QStringLiteral("pack-refs"), QStringLiteral("--all")});
  git({QStringLiteral("tag"), QStringLiteral("loose")});
  expected.insert(2, QStringLiteral("loose"));
  QCOMPARE(GitRepo::listTags(_repo.get()), expected);
}

// Merge, squash, rebase, cherry-pick and revert.

void GitRepoTest::mergeFastForward() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("feature")});
  QVERIFY(writeFile(file("f.txt"), "f\n"));
  gitCommitAll(QStringLiteral("F1"));
  QVERIFY(writeFile(file("g.txt"), "g\n"));
  const QString featureTip = gitCommitAll(QStringLiteral("F2"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("main")});

  QVERIFY2(GitRepo::merge(_repo.get(), QStringLiteral("feature"),
                          QStringLiteral("main"), kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QCOMPARE(head(), featureTip);
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/main"));
  QCOMPARE(git({QStringLiteral("rev-list"), QStringLiteral("--merges"),
                QStringLiteral("--count"), QStringLiteral("HEAD")}),
           QStringLiteral("0"));
  QCOMPARE(readFile(file("g.txt")), QByteArray("g\n"));
  QCOMPARE(git({QStringLiteral("status"), QStringLiteral("--porcelain")}),
           QString());
  QCOMPARE(git_repository_state(_repo.get()),
           static_cast<int>(GIT_REPOSITORY_STATE_NONE));
}

void GitRepoTest::mergeFastForwardKeepsLocalChanges() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(writeFile(file("n.txt"), "n\n"));
  gitCommitAll(QStringLiteral("Base"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("feature")});
  QVERIFY(writeFile(file("f.txt"), "f\n"));
  const QString featureTip = gitCommitAll(QStringLiteral("Feature"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("main")});

  // Local changes the merge doesn't touch, like `git merge` keeps them.
  QVERIFY(writeFile(file("n.txt"), "dirty\n"));
  QVERIFY(writeFile(file("u.txt"), "untracked\n"));
  QVERIFY2(GitRepo::merge(_repo.get(), QStringLiteral("feature"),
                          QStringLiteral("main"), kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QCOMPARE(head(), featureTip);
  QCOMPARE(readFile(file("u.txt")), QByteArray("untracked\n"));
  QCOMPARE(readFile(file("n.txt")), QByteArray("dirty\n"));
  QCOMPARE(readFile(file("f.txt")), QByteArray("f\n"));

  // A local change to a file the fast-forward changes stops it, nothing moves.
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("more")});
  QVERIFY(writeFile(file("a.txt"), "more\n"));
  git({QStringLiteral("commit"), QStringLiteral("-qm"), QStringLiteral("More"),
       QStringLiteral("a.txt")});
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("main")});
  QVERIFY(writeFile(file("a.txt"), "local\n"));
  QVERIFY(!GitRepo::merge(_repo.get(), QStringLiteral("more"),
                          QStringLiteral("main"), kAuthor, kEmail));
  QCOMPARE(lastError(), GitError::MergeFailedToCheckoutDestinationTree);
  QCOMPARE(head(), featureTip);
  QCOMPARE(readFile(file("a.txt")), QByteArray("local\n"));
}

void GitRepoTest::mergeCreatesMergeCommit() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("feature")});
  QVERIFY(writeFile(file("f.txt"), "f\n"));
  const QString featureTip = gitCommitAll(QStringLiteral("Feature"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("main")});
  QVERIFY(writeFile(file("m.txt"), "m\n"));
  const QString mainTip = gitCommitAll(QStringLiteral("Main"));

  QVERIFY2(GitRepo::merge(_repo.get(), QStringLiteral("feature"),
                          QStringLiteral("main"), kOtherAuthor, kOtherEmail),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("log"), QStringLiteral("-1"),
                QStringLiteral("--format=%P")}),
           mainTip + ' ' + featureTip);
  QCOMPARE(git({QStringLiteral("log"), QStringLiteral("-1"),
                QStringLiteral("--format=%B|%an|%ae|%cn")}),
           QStringLiteral("Merge branch 'feature' into 'main'|Other Author|"
                          "other@example.com|Other Author"));
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/main"));
  QCOMPARE(readFile(file("f.txt")), QByteArray("f\n"));
  QCOMPARE(readFile(file("m.txt")), QByteArray("m\n"));
  QCOMPARE(git({QStringLiteral("status"), QStringLiteral("--porcelain")}),
           QString());
  QCOMPARE(git_repository_state(_repo.get()),
           static_cast<int>(GIT_REPOSITORY_STATE_NONE));
  QVERIFY(!GitRepo::getIsMergeInProgress(_repo.get()));
}

void GitRepoTest::mergeAlreadyUpToDate() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  git({QStringLiteral("branch"), QStringLiteral("old")});
  QVERIFY(writeFile(file("a.txt"), "a2\n"));
  const QString tip = gitCommitAll(QStringLiteral("Tip"));

  QVERIFY2(GitRepo::merge(_repo.get(), QStringLiteral("old"),
                          QStringLiteral("main"), kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QCOMPARE(head(), tip);
  // A branch merged into itself.
  QVERIFY2(GitRepo::merge(_repo.get(), QStringLiteral("main"),
                          QStringLiteral("main"), kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QCOMPARE(head(), tip);
  QCOMPARE(git_repository_state(_repo.get()),
           static_cast<int>(GIT_REPOSITORY_STATE_NONE));
  QCOMPARE(git({QStringLiteral("status"), QStringLiteral("--porcelain")}),
           QString());
}

void GitRepoTest::mergeUnrelatedHistories() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString mainTip = gitCommitAll(QStringLiteral("Main"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("--orphan"),
       QStringLiteral("other")});
  bool ok = false;
  git({QStringLiteral("rm"), QStringLiteral("-rfq"), QStringLiteral(".")}, &ok);
  QVERIFY(writeFile(file("o.txt"), "o\n"));
  const QString otherTip = gitCommitAll(QStringLiteral("Other root"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("main")});

  // Like git, only with --allow-unrelated-histories.
  QVERIFY(!GitRepo::merge(_repo.get(), QStringLiteral("other"),
                          QStringLiteral("main"), kAuthor, kEmail));
  QCOMPARE(lastError(), GitError::MergeUnrelatedHistories);
  QCOMPARE(head(), mainTip);
  QVERIFY(!QFileInfo::exists(file("o.txt")));
  QVERIFY(!GitRepo::getIsMergeInProgress(_repo.get()));

  QVERIFY2(GitRepo::merge(_repo.get(), QStringLiteral("other"),
                          QStringLiteral("main"), kAuthor, kEmail, true),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("log"), QStringLiteral("-1"),
                QStringLiteral("--format=%P")}),
           mainTip + ' ' + otherTip);
  QCOMPARE(readFile(file("a.txt")), QByteArray("a\n"));
  QCOMPARE(readFile(file("o.txt")), QByteArray("o\n"));
}

void GitRepoTest::mergeConflictAndAbort() {
  makeConflictingBranches();
  const QString before = head();

  // The merge stops and is left for the user to resolve.
  QVERIFY(!GitRepo::merge(_repo.get(), QStringLiteral("feature"),
                          QStringLiteral("main"), kAuthor, kEmail));
  QCOMPARE(lastError(), GitError::MergeFailedWithConflicts);
  QVERIFY(GitRepo::hasConflicts(_repo.get()));
  QVERIFY(GitRepo::getIsMergeInProgress(_repo.get()));
  QCOMPARE(status().conflictedFiles, QStringList{QStringLiteral("a.txt")});
  const QString markers =
      GitRepo::listConflictsForAFile(_repo.get(), QStringLiteral("a.txt"));
  QVERIFY2(markers.contains(QStringLiteral("<<<<<<<")) &&
               markers.contains(QStringLiteral(">>>>>>>")) &&
               markers.contains(QStringLiteral("main")) &&
               markers.contains(QStringLiteral("feature")),
           qPrintable(markers));
  QCOMPARE(head(), before);

  QVERIFY2(GitRepo::abortMerge(_repo.get()), qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("a.txt")), QByteArray("main\n"));
  QCOMPARE(head(), before);
  QVERIFY(!GitRepo::hasConflicts(_repo.get()));
  QVERIFY(!GitRepo::getIsMergeInProgress(_repo.get()));
  QCOMPARE(git({QStringLiteral("status"), QStringLiteral("--porcelain")}),
           QString());

  // Nothing left to abort.
  QVERIFY(!GitRepo::abortMerge(_repo.get()));
}

void GitRepoTest::mergeConflictResolveAndCommit() {
  makeConflictingBranches();
  const QString mainTip = head();
  const QString featureTip = revParse(QStringLiteral("feature"));

  QVERIFY(!GitRepo::merge(_repo.get(), QStringLiteral("feature"),
                          QStringLiteral("main"), kAuthor, kEmail));
  QVERIFY(writeFile(file("a.txt"), "resolved\n"));
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  QVERIFY2(GitRepo::commitStaged(_repo.get(), QStringLiteral("Merged"),
                                 QString(), kAuthor, kEmail, true, false,
                                 false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("log"), QStringLiteral("-1"),
                QStringLiteral("--format=%P|%s")}),
           mainTip + ' ' + featureTip + QStringLiteral("|Merged"));
  QCOMPARE(git_repository_state(_repo.get()),
           static_cast<int>(GIT_REPOSITORY_STATE_NONE));
  QCOMPARE(git({QStringLiteral("status"), QStringLiteral("--porcelain")}),
           QString());
  QCOMPARE(readFile(file("a.txt")), QByteArray("resolved\n"));
}

void GitRepoTest::mergeConflictKinds_data() {
  QTest::addColumn<QString>("kind");
  QTest::addColumn<QStringList>("conflicted");
  QTest::newRow("both modified") << QStringLiteral("both modified")
                                 << QStringList{QStringLiteral("a.txt")};
  QTest::newRow("modify/delete") << QStringLiteral("modify/delete")
                                 << QStringList{QStringLiteral("a.txt")};
  QTest::newRow("delete/modify") << QStringLiteral("delete/modify")
                                 << QStringList{QStringLiteral("a.txt")};
  QTest::newRow("add/add") << QStringLiteral("add/add")
                           << QStringList{QStringLiteral("n.txt")};
  QTest::newRow("binary") << QStringLiteral("binary")
                          << QStringList{QStringLiteral("bin.dat")};
  QTest::newRow("rename/rename") << QStringLiteral("rename/rename")
                                 << QStringList();
}

void GitRepoTest::mergeConflictKinds() {
  QFETCH(QString, kind);
  QFETCH(QStringList, conflicted);

  QVERIFY(writeFile(file("a.txt"), "line 1\nline 2\nline 3\n"));
  QVERIFY(writeFile(file("bin.dat"), QByteArray("\0\1\2base\0", 8)));
  gitCommitAll(QStringLiteral("Base"));

  const auto change = [&](const QByteArray &side) {
    if (kind == QStringLiteral("both modified")) {
      QVERIFY(writeFile(file("a.txt"), "line 1\n" + side + "\nline 3\n"));
    } else if (kind == QStringLiteral("modify/delete")) {
      if (side == "main") {
        QVERIFY(writeFile(file("a.txt"), "line 1\nmain\nline 3\n"));
      } else {
        QVERIFY(QFile::remove(file("a.txt")));
      }
    } else if (kind == QStringLiteral("delete/modify")) {
      if (side == "main") {
        QVERIFY(QFile::remove(file("a.txt")));
      } else {
        QVERIFY(writeFile(file("a.txt"), "line 1\nfeature\nline 3\n"));
      }
    } else if (kind == QStringLiteral("add/add")) {
      QVERIFY(writeFile(file("n.txt"), side + '\n'));
    } else if (kind == QStringLiteral("binary")) {
      QVERIFY(writeFile(file("bin.dat"), QByteArray("\0\1\2", 3) + side + '\0'));
    } else if (kind == QStringLiteral("rename/rename")) {
      QVERIFY(QFile::rename(file("a.txt"), file(QString::fromLatin1(side) +
                                                QStringLiteral(".txt"))));
    }
  };
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("feature")});
  change("feature");
  gitCommitAll(QStringLiteral("Feature"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("main")});
  change("main");
  const QString mainTip = gitCommitAll(QStringLiteral("Main"));

  QVERIFY(!GitRepo::merge(_repo.get(), QStringLiteral("feature"),
                          QStringLiteral("main"), kAuthor, kEmail));
  QVERIFY(GitRepo::hasConflicts(_repo.get()));
  QVERIFY(GitRepo::getIsMergeInProgress(_repo.get()));
  const GitRepo::GitStatus st = status();
  if (conflicted.isEmpty()) {
    QVERIFY(!st.conflictedFiles.isEmpty());
  } else {
    QCOMPARE(st.conflictedFiles, conflicted);
  }

  QVERIFY2(GitRepo::abortMerge(_repo.get()), qPrintable(lastErrorMessage()));
  QCOMPARE(head(), mainTip);
  QVERIFY(!GitRepo::hasConflicts(_repo.get()));
  QCOMPARE(git({QStringLiteral("status"), QStringLiteral("--porcelain")}),
           QString());
}

void GitRepoTest::mergeWithDirtyWorkdirFails() {
  QVERIFY(writeFile(file("a.txt"), "base\n"));
  gitCommitAll(QStringLiteral("Base"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("feature")});
  QVERIFY(writeFile(file("a.txt"), "feature\n"));
  gitCommitAll(QStringLiteral("Feature"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("main")});
  QVERIFY(writeFile(file("m.txt"), "m\n"));
  const QString mainTip = gitCommitAll(QStringLiteral("Main"));

  // A local change to a file the merge changes.
  QVERIFY(writeFile(file("a.txt"), "local\n"));
  QVERIFY(!GitRepo::merge(_repo.get(), QStringLiteral("feature"),
                          QStringLiteral("main"), kAuthor, kEmail));
  QCOMPARE(lastError(), GitError::MergeFailedToMerge);
  QCOMPARE(readFile(file("a.txt")), QByteArray("local\n"));
  QCOMPARE(head(), mainTip);
  QVERIFY(!GitRepo::getIsMergeInProgress(_repo.get()));
  QCOMPARE(git_repository_state(_repo.get()),
           static_cast<int>(GIT_REPOSITORY_STATE_NONE));
}

void GitRepoTest::mergeIntoOtherBranchChecksItOut() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString base = gitCommitAll(QStringLiteral("Base"));
  git({QStringLiteral("branch"), QStringLiteral("other")});
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("feature")});
  QVERIFY(writeFile(file("f.txt"), "f\n"));
  const QString featureTip = gitCommitAll(QStringLiteral("Feature"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("main")});
  QVERIFY(writeFile(file("m.txt"), "m\n"));
  const QString mainTip = gitCommitAll(QStringLiteral("Main"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("other")});

  // Missing branches are refused first.
  QVERIFY(!GitRepo::merge(_repo.get(), QStringLiteral("nope"),
                          QStringLiteral("main"), kAuthor, kEmail));
  QCOMPARE(lastError(), GitError::MergeFailedToLookupBranch);
  QVERIFY(!GitRepo::merge(_repo.get(), QStringLiteral("feature"),
                          QStringLiteral("nope"), kAuthor, kEmail));
  QCOMPARE(lastError(), GitError::MergeFailedToLookupDestinationBranch);
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/other"));

  // The destination is checked out and the merge commit made there.
  QVERIFY2(GitRepo::merge(_repo.get(), QStringLiteral("feature"),
                          QStringLiteral("main"), kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/main"));
  QCOMPARE(git({QStringLiteral("log"), QStringLiteral("-1"),
                QStringLiteral("--format=%P")}),
           mainTip + ' ' + featureTip);
  QCOMPARE(revParse(QStringLiteral("other")), base);
  QCOMPARE(readFile(file("f.txt")), QByteArray("f\n"));
  QCOMPARE(readFile(file("m.txt")), QByteArray("m\n"));
}

void GitRepoTest::squashBranches() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("feature")});
  QVERIFY(writeFile(file("f1.txt"), "1\n"));
  gitCommitAll(QStringLiteral("F1"));
  QVERIFY(writeFile(file("f2.txt"), "2\n"));
  gitCommitAll(QStringLiteral("F2"));
  QVERIFY(writeFile(file("a.txt"), "a3\n"));
  const QString featureTip = gitCommitAll(QStringLiteral("F3"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("main")});
  QVERIFY(writeFile(file("m.txt"), "m\n"));
  const QString mainTip = gitCommitAll(QStringLiteral("Main"));
  // Started from the source branch.
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("feature")});

  QVERIFY2(GitRepo::squashBranches(_repo.get(), QStringLiteral("feature"),
                                   QStringLiteral("main"), kOtherAuthor,
                                   kOtherEmail, false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/main"));
  // One commit with a single parent, listing the squashed commits.
  QCOMPARE(git({QStringLiteral("log"), QStringLiteral("-1"),
                QStringLiteral("--format=%P|%s|%an")}),
           mainTip + QStringLiteral("|Squash merge branch 'feature' into "
                                    "'main'|Other Author"));
  QCOMPARE(git({QStringLiteral("log"), QStringLiteral("-1"),
                QStringLiteral("--format=%b")}),
           QStringLiteral("* F3\n* F2\n* F1"));
  QCOMPARE(revParse(QStringLiteral("feature")), featureTip);
  QCOMPARE(readFile(file("f1.txt")), QByteArray("1\n"));
  QCOMPARE(readFile(file("f2.txt")), QByteArray("2\n"));
  QCOMPARE(readFile(file("a.txt")), QByteArray("a3\n"));
  QCOMPARE(readFile(file("m.txt")), QByteArray("m\n"));
  QCOMPARE(git({QStringLiteral("status"), QStringLiteral("--porcelain")}),
           QString());
  QCOMPARE(git_repository_state(_repo.get()),
           static_cast<int>(GIT_REPOSITORY_STATE_NONE));
}

void GitRepoTest::squashInvalidArguments_data() {
  QTest::addColumn<QString>("source");
  QTest::addColumn<QString>("destination");
  QTest::addColumn<GitRepo::GitError>("error");
  QTest::newRow("empty source") << QString() << QStringLiteral("main")
                                << GitError::SquashBranchesInvalidArguments;
  QTest::newRow("empty destination")
      << QStringLiteral("feature") << QString()
      << GitError::SquashBranchesInvalidArguments;
  QTest::newRow("same branch") << QStringLiteral("main") << QStringLiteral("main")
                               << GitError::SquashBranchesInvalidArguments;
  QTest::newRow("missing source")
      << QStringLiteral("nope") << QStringLiteral("main")
      << GitError::SquashBranchesFailedToLookupBranch;
  QTest::newRow("missing destination")
      << QStringLiteral("feature") << QStringLiteral("nope")
      << GitError::SquashBranchesFailedToLookupDestinationBranch;
}

void GitRepoTest::squashInvalidArguments() {
  QFETCH(QString, source);
  QFETCH(QString, destination);
  QFETCH(GitRepo::GitError, error);
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString before = gitCommitAll(QStringLiteral("Base"));
  git({QStringLiteral("branch"), QStringLiteral("feature")});

  QVERIFY(!GitRepo::squashBranches(_repo.get(), source, destination, kAuthor,
                                   kEmail, false));
  QCOMPARE(lastError(), error);
  QCOMPARE(head(), before);
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/main"));
}

void GitRepoTest::squashBusyRepository() {
  makeMergeConflict();
  QVERIFY(!GitRepo::squashBranches(_repo.get(), QStringLiteral("feature"),
                                   QStringLiteral("main"), kAuthor, kEmail,
                                   false));
  QCOMPARE(lastError(), GitError::SquashBranchesRepositoryBusy);
  QVERIFY(GitRepo::getIsMergeInProgress(_repo.get()));
}

void GitRepoTest::squashConflicts() {
  makeConflictingBranches();
  const QString mainTip = head();

  QVERIFY(!GitRepo::squashBranches(_repo.get(), QStringLiteral("feature"),
                                   QStringLiteral("main"), kAuthor, kEmail,
                                   false));
  QCOMPARE(lastError(), GitError::SquashBranchesFailedWithConflicts);
  // Like `git merge --squash`: conflicts in the working copy, no merge state.
  QVERIFY(readFile(file("a.txt")).contains("<<<<<<<"));
  QVERIFY(GitRepo::hasConflicts(_repo.get()));
  QCOMPARE(git_repository_state(_repo.get()),
           static_cast<int>(GIT_REPOSITORY_STATE_NONE));
  QCOMPARE(head(), mainTip);

  QVERIFY(writeFile(file("a.txt"), "resolved\n"));
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  QVERIFY2(GitRepo::commitStaged(_repo.get(), QStringLiteral("Squashed"),
                                 QString(), kAuthor, kEmail, true, false,
                                 false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("log"), QStringLiteral("-1"),
                QStringLiteral("--format=%P")}),
           mainTip);
}

void GitRepoTest::squashNothingToSquash() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  git({QStringLiteral("branch"), QStringLiteral("behind")});
  QVERIFY(writeFile(file("a.txt"), "a2\n"));
  const QString tip = gitCommitAll(QStringLiteral("Tip"));

  // The source is already merged.
  QVERIFY2(GitRepo::squashBranches(_repo.get(), QStringLiteral("behind"),
                                   QStringLiteral("main"), kAuthor, kEmail,
                                   false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(head(), tip);

  // The same change on both sides: merged, the tree doesn't change.
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("same")});
  QVERIFY(writeFile(file("c.txt"), "c\n"));
  gitCommitAll(QStringLiteral("Same on feature"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("main")});
  QVERIFY(writeFile(file("c.txt"), "c\n"));
  const QString mainTip = gitCommitAll(QStringLiteral("Same on main"));
  QVERIFY2(GitRepo::squashBranches(_repo.get(), QStringLiteral("same"),
                                   QStringLiteral("main"), kAuthor, kEmail,
                                   false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(head(), mainTip);
  QCOMPARE(git_repository_state(_repo.get()),
           static_cast<int>(GIT_REPOSITORY_STATE_NONE));
  QCOMPARE(git({QStringLiteral("status"), QStringLiteral("--porcelain")}),
           QString());
}

void GitRepoTest::rebaseLinear() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("feature")});
  QVERIFY(writeFile(file("f1.txt"), "1\n"));
  gitCommitAllAsOther(QStringLiteral("F1"));
  QVERIFY(writeFile(file("f2.txt"), "2\n"));
  const QString oldTip = gitCommitAllAsOther(QStringLiteral("F2"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("main")});
  QVERIFY(writeFile(file("m.txt"), "m\n"));
  const QString mainTip = gitCommitAll(QStringLiteral("Main"));

  QVERIFY2(GitRepo::rebaseBranches(_repo.get(), QStringLiteral("feature"),
                                   QStringLiteral("main"),
                                   QStringLiteral("Rebaser"),
                                   QStringLiteral("rebaser@example.com"), false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(revParse(QStringLiteral("feature~2")), mainTip);
  QVERIFY(revParse(QStringLiteral("feature")) != oldTip);
  // Authors are kept, the committer is the one passed in.
  QCOMPARE(git({QStringLiteral("log"), QStringLiteral("-2"),
                QStringLiteral("--format=%s|%an|%cn"), QStringLiteral("feature")}),
           QStringLiteral("F2|Other Author|Rebaser\nF1|Other Author|Rebaser"));
  QCOMPARE(revParse(QStringLiteral("main")), mainTip);
  // The rebased branch is checked out.
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/feature"));
  QCOMPARE(readFile(file("m.txt")), QByteArray("m\n"));
  QCOMPARE(readFile(file("f2.txt")), QByteArray("2\n"));
  QCOMPARE(git({QStringLiteral("status"), QStringLiteral("--porcelain")}),
           QString());
  QCOMPARE(git_repository_state(_repo.get()),
           static_cast<int>(GIT_REPOSITORY_STATE_NONE));

  // Missing branches.
  QVERIFY(!GitRepo::rebaseBranches(_repo.get(), QStringLiteral("nope"),
                                   QStringLiteral("main"), kAuthor, kEmail,
                                   false));
  QCOMPARE(lastError(), GitError::RebaseBranchesFailedToLookupBranch);
  QVERIFY(!GitRepo::rebaseBranches(_repo.get(), QStringLiteral("feature"),
                                   QStringLiteral("nope"), kAuthor, kEmail,
                                   false));
  QCOMPARE(lastError(), GitError::RebaseBranchesFailedToLookupDestinationBranch);
  QVERIFY(!GitRepo::rebaseBranches(_repo.get(), QString(),
                                   QStringLiteral("main"), kAuthor, kEmail,
                                   false));
}

void GitRepoTest::rebaseAlreadyUpToDate() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  git({QStringLiteral("branch"), QStringLiteral("behind")});
  QVERIFY(writeFile(file("a.txt"), "a2\n"));
  const QString tip = gitCommitAll(QStringLiteral("Tip"));
  git({QStringLiteral("branch"), QStringLiteral("same")});

  // A branch at the destination stays where it is.
  QVERIFY2(GitRepo::rebaseBranches(_repo.get(), QStringLiteral("same"),
                                   QStringLiteral("main"), kAuthor, kEmail,
                                   false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(revParse(QStringLiteral("same")), tip);

  // A branch behind the destination is fast-forwarded and checked out.
  QVERIFY2(GitRepo::rebaseBranches(_repo.get(), QStringLiteral("behind"),
                                   QStringLiteral("main"), kAuthor, kEmail,
                                   false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(revParse(QStringLiteral("behind")), tip);
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/behind"));
  QCOMPARE(git_repository_state(_repo.get()),
           static_cast<int>(GIT_REPOSITORY_STATE_NONE));
}

void GitRepoTest::rebaseConflictAbortRestores() {
  makeConflictingBranches();
  const QString mainTip = head();
  const QString featureTip = revParse(QStringLiteral("feature"));

  QVERIFY(!GitRepo::rebaseBranches(_repo.get(), QStringLiteral("feature"),
                                   QStringLiteral("main"), kAuthor, kEmail,
                                   false));
  QCOMPARE(lastError(),
           GitError::RebaseBranchesFailedToCommitRebaseStepWithConflicts);
  QVERIFY(GitRepo::getIsRebaseInProgress(_repo.get()));
  QVERIFY(GitRepo::hasConflicts(_repo.get()));
  QVERIFY(readFile(file("a.txt")).contains("<<<<<<<"));

  QVERIFY2(GitRepo::abortRebase(_repo.get()), qPrintable(lastErrorMessage()));
  QVERIFY(!GitRepo::getIsRebaseInProgress(_repo.get()));
  QCOMPARE(revParse(QStringLiteral("feature")), featureTip);
  QCOMPARE(revParse(QStringLiteral("main")), mainTip);
  // The rebase started by checking out the source branch, so that is where
  // the abort returns to.
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/feature"));
  QCOMPARE(readFile(file("a.txt")), QByteArray("feature\n"));
  QCOMPARE(git({QStringLiteral("status"), QStringLiteral("--porcelain")}),
           QString());

  // Nothing left to abort.
  QVERIFY(!GitRepo::abortRebase(_repo.get()));
}

void GitRepoTest::rebaseConflictsAtSeveralSteps() {
  QVERIFY(writeFile(file("a.txt"), "base a\n"));
  QVERIFY(writeFile(file("c.txt"), "base c\n"));
  gitCommitAll(QStringLiteral("Base"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("feature")});
  QVERIFY(writeFile(file("a.txt"), "feature a\n"));
  gitCommitAllAsOther(QStringLiteral("Feature a"));
  QVERIFY(writeFile(file("b.txt"), "b\n"));
  gitCommitAllAsOther(QStringLiteral("Feature b"));
  QVERIFY(writeFile(file("c.txt"), "feature c\n"));
  gitCommitAllAsOther(QStringLiteral("Feature c"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("main")});
  QVERIFY(writeFile(file("a.txt"), "main a\n"));
  QVERIFY(writeFile(file("c.txt"), "main c\n"));
  const QString mainTip = gitCommitAll(QStringLiteral("Main"));

  QVERIFY(!GitRepo::rebaseBranches(_repo.get(), QStringLiteral("feature"),
                                   QStringLiteral("main"), kAuthor, kEmail,
                                   false));
  QVERIFY(GitRepo::getIsRebaseInProgress(_repo.get()));

  // Nothing is resolved yet.
  QVERIFY(!GitRepo::commitRebase(_repo.get(), QStringLiteral("Resolved a"),
                                 QString(), kAuthor, kEmail, true, false,
                                 false));
  QCOMPARE(lastError(), GitError::CommitStagedFailedToWriteIndexTree);
  QVERIFY(GitRepo::getIsRebaseInProgress(_repo.get()));

  // The first step is resolved, the next one applies, the third conflicts.
  QVERIFY(writeFile(file("a.txt"), "resolved a\n"));
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  QVERIFY(!GitRepo::commitRebase(_repo.get(), QStringLiteral("Resolved a"),
                                 QString(), kAuthor, kEmail, true, false,
                                 false));
  QCOMPARE(lastError(),
           GitError::RebaseBranchesFailedToCommitRebaseStepWithConflicts);
  QVERIFY(GitRepo::getIsRebaseInProgress(_repo.get()));
  QVERIFY(readFile(file("c.txt")).contains("<<<<<<<"));

  QVERIFY(writeFile(file("c.txt"), "resolved c\n"));
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("c.txt")}));
  QVERIFY2(GitRepo::commitRebase(_repo.get(), QStringLiteral("Resolved c"),
                                 QString(), kAuthor, kEmail, true, false,
                                 false),
           qPrintable(lastErrorMessage()));
  QVERIFY(!GitRepo::getIsRebaseInProgress(_repo.get()));
  QCOMPARE(git_repository_state(_repo.get()),
           static_cast<int>(GIT_REPOSITORY_STATE_NONE));
  QCOMPARE(git({QStringLiteral("log"), QStringLiteral("-4"),
                QStringLiteral("--format=%s"), QStringLiteral("feature")}),
           QStringLiteral("Resolved c\nFeature b\nResolved a\nMain"));
  QCOMPARE(revParse(QStringLiteral("feature~3")), mainTip);
  QCOMPARE(git({QStringLiteral("status"), QStringLiteral("--porcelain")}),
           QString());

  // Like `git rebase --continue`, every step keeps its author, the committer
  // is the one passed in.
  QCOMPARE(git({QStringLiteral("log"), QStringLiteral("-3"),
                QStringLiteral("--format=%cn"), QStringLiteral("feature")}),
           QStringLiteral("Tester\nTester\nTester"));
  QCOMPARE(git({QStringLiteral("log"), QStringLiteral("-1"),
                QStringLiteral("--format=%an"), QStringLiteral("feature~2")}),
           kOtherAuthor);
  QCOMPARE(git({QStringLiteral("log"), QStringLiteral("-1"),
                QStringLiteral("--format=%an"), QStringLiteral("feature~1")}),
           kOtherAuthor);
  QCOMPARE(git({QStringLiteral("log"), QStringLiteral("-1"),
                QStringLiteral("--format=%an"), QStringLiteral("feature")}),
           kOtherAuthor);
}

void GitRepoTest::rebaseResolvedToBaseDropsStep() {
  makeConflictingBranches();
  const QString mainTip = head();

  QVERIFY(!GitRepo::rebaseBranches(_repo.get(), QStringLiteral("feature"),
                                   QStringLiteral("main"), kAuthor, kEmail,
                                   false));
  // Resolved to the destination's version, the step changes nothing and is
  // dropped like `git rebase --continue` does.
  QVERIFY(writeFile(file("a.txt"), "main\n"));
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  QVERIFY2(GitRepo::commitRebase(_repo.get(), QStringLiteral("Resolved"),
                                 QString(), kAuthor, kEmail, true, false,
                                 false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(revParse(QStringLiteral("feature")), mainTip);
  QVERIFY(!GitRepo::getIsRebaseInProgress(_repo.get()));
  QCOMPARE(git({QStringLiteral("status"), QStringLiteral("--porcelain")}),
           QString());
}

void GitRepoTest::rebaseFlattensMergeCommits() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("feature")});
  QVERIFY(writeFile(file("f.txt"), "f\n"));
  gitCommitAll(QStringLiteral("F"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("side")});
  QVERIFY(writeFile(file("s.txt"), "s\n"));
  const QString sideTip = gitCommitAll(QStringLiteral("S"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("feature")});
  QVERIFY(writeFile(file("g.txt"), "g\n"));
  gitCommitAll(QStringLiteral("G"));
  git({QStringLiteral("merge"), QStringLiteral("-q"), QStringLiteral("--no-ff"),
       QStringLiteral("-m"), QStringLiteral("Merge side"), QStringLiteral("side")});
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("main")});
  QVERIFY(writeFile(file("m.txt"), "m\n"));
  gitCommitAll(QStringLiteral("Main"));

  QVERIFY2(GitRepo::rebaseBranches(_repo.get(), QStringLiteral("feature"),
                                   QStringLiteral("main"), kAuthor, kEmail,
                                   false),
           qPrintable(lastErrorMessage()));
  // The merge commit is dropped and the merged commits are replayed in line.
  QCOMPARE(git({QStringLiteral("rev-list"), QStringLiteral("--merges"),
                QStringLiteral("--count"), QStringLiteral("main..feature")}),
           QStringLiteral("0"));
  QCOMPARE(git({QStringLiteral("rev-list"), QStringLiteral("--count"),
                QStringLiteral("main..feature")}),
           QStringLiteral("3"));
  for (const char *name : {"a.txt", "f.txt", "s.txt", "g.txt", "m.txt"}) {
    QVERIFY2(QFileInfo::exists(file(name)), name);
  }
  QCOMPARE(revParse(QStringLiteral("side")), sideTip);
}

void GitRepoTest::rebaseWithDirtyWorkdirFails() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("feature")});
  QVERIFY(writeFile(file("f.txt"), "f\n"));
  gitCommitAll(QStringLiteral("Feature"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("main")});
  QVERIFY(writeFile(file("m.txt"), "m\n"));
  const QString mainTip = gitCommitAll(QStringLiteral("Main"));

  QVERIFY(writeFile(file("a.txt"), "dirty\n"));
  QVERIFY(!GitRepo::rebaseBranches(_repo.get(), QStringLiteral("feature"),
                                   QStringLiteral("main"), kAuthor, kEmail,
                                   false));
  QCOMPARE(lastError(), GitError::RebaseBranchesFailedToStartRebase);
  QCOMPARE(readFile(file("a.txt")), QByteArray("dirty\n"));
  QVERIFY(!GitRepo::getIsRebaseInProgress(_repo.get()));
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/main"));
  QCOMPARE(head(), mainTip);

  // Untracked files don't count.
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(writeFile(file("u.txt"), "u\n"));
  QVERIFY2(GitRepo::rebaseBranches(_repo.get(), QStringLiteral("feature"),
                                   QStringLiteral("main"), kAuthor, kEmail,
                                   false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(revParse(QStringLiteral("feature~1")), mainTip);
  QCOMPARE(readFile(file("u.txt")), QByteArray("u\n"));
}

void GitRepoTest::rebaseInProgressBlocksOtherOperations() {
  makeConflictingBranches();
  git({QStringLiteral("branch"), QStringLiteral("other"), QStringLiteral("main~1")});
  QVERIFY(!GitRepo::rebaseBranches(_repo.get(), QStringLiteral("feature"),
                                   QStringLiteral("main"), kAuthor, kEmail,
                                   false));
  QVERIFY(GitRepo::getIsRebaseInProgress(_repo.get()));
  const QString during = head();

  QVERIFY(!GitRepo::squashBranches(_repo.get(), QStringLiteral("other"),
                                   QStringLiteral("main"), kAuthor, kEmail,
                                   false));
  QCOMPARE(lastError(), GitError::SquashBranchesRepositoryBusy);
  GitRepo::BisectStatus bisectStatus;
  QVERIFY(!GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Bad,
                           QStringLiteral("HEAD"), bisectStatus));
  QCOMPARE(lastError(), GitError::BisectRepositoryBusy);
  QVERIFY(!GitRepo::checkoutBranch(_repo.get(), QStringLiteral("other"),
                                   GitRepo::BranchType::Local));
  QVERIFY(!GitRepo::merge(_repo.get(), QStringLiteral("other"),
                          QStringLiteral("main"), kAuthor, kEmail));
  QCOMPARE(head(), during);
  QVERIFY(GitRepo::getIsRebaseInProgress(_repo.get()));

  QVERIFY2(GitRepo::abortRebase(_repo.get()), qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("status"), QStringLiteral("--porcelain")}),
           QString());
}

void GitRepoTest::cherryPick() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("feature")});
  QVERIFY(writeFile(file("p.txt"), "p\n"));
  const QString picked = gitCommitAllAsOther(QStringLiteral("Pick me\n\nWith a body"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("main")});
  QVERIFY(writeFile(file("m.txt"), "m\n"));
  const QString mainTip = gitCommitAll(QStringLiteral("Main"));

  QVERIFY2(GitRepo::cherryPickCommit(_repo.get(), picked, kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("log"), QStringLiteral("-1"),
                QStringLiteral("--format=%P|%cn")}),
           mainTip + QStringLiteral("|Tester"));
  QCOMPARE(commitMessage(_repo.get(), QStringLiteral("HEAD")),
           commitMessage(_repo.get(), picked));
  QCOMPARE(readFile(file("p.txt")), QByteArray("p\n"));
  QCOMPARE(git({QStringLiteral("status"), QStringLiteral("--porcelain")}),
           QString());
  QCOMPARE(git_repository_state(_repo.get()),
           static_cast<int>(GIT_REPOSITORY_STATE_NONE));
  QCOMPARE(git({QStringLiteral("log"), QStringLiteral("-1"),
                QStringLiteral("--format=%an")}),
           kOtherAuthor);
}

void GitRepoTest::cherryPickConflict() {
  makeConflictingBranches();
  const QString mainTip = head();
  const QString picked = revParse(QStringLiteral("feature"));

  // Like `git cherry-pick`: the conflicts are written to the working copy, HEAD
  // stays and the cherry-pick is in progress.
  QVERIFY(!GitRepo::cherryPickCommit(_repo.get(), picked, kAuthor, kEmail));
  QCOMPARE(lastError(), GitError::CherryPickCommitFailedWithConflicts);
  QCOMPARE(head(), mainTip);
  QVERIFY(GitRepo::hasConflicts(_repo.get()));
  QCOMPARE(status().conflictedFiles, QStringList{QStringLiteral("a.txt")});
  QVERIFY(readFile(file("a.txt")).contains("<<<<<<<"));
  QCOMPARE(git_repository_state(_repo.get()),
           static_cast<int>(GIT_REPOSITORY_STATE_CHERRYPICK));
  QCOMPARE(revParse(QStringLiteral("CHERRY_PICK_HEAD")), picked);
  QVERIFY(GitRepo::getIsMergeInProgress(_repo.get()));

  // Aborted like a merge.
  QVERIFY2(GitRepo::abortMerge(_repo.get()), qPrintable(lastErrorMessage()));
  QCOMPARE(git_repository_state(_repo.get()),
           static_cast<int>(GIT_REPOSITORY_STATE_NONE));
  QCOMPARE(readFile(file("a.txt")), QByteArray("main\n"));
  QCOMPARE(git({QStringLiteral("status"), QStringLiteral("--porcelain")}),
           QString());

  // Or resolved and committed, which ends it.
  QVERIFY(!GitRepo::cherryPickCommit(_repo.get(), picked, kAuthor, kEmail));
  QVERIFY(writeFile(file("a.txt"), "resolved\n"));
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  QVERIFY2(GitRepo::commitStaged(_repo.get(), QStringLiteral("Picked"),
                                 QString(), kAuthor, kEmail, true, false,
                                 false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("log"), QStringLiteral("-1"),
                QStringLiteral("--format=%P")}),
           mainTip);
  QCOMPARE(git_repository_state(_repo.get()),
           static_cast<int>(GIT_REPOSITORY_STATE_NONE));

  // Local changes in the way stop it before anything is touched.
  QVERIFY(writeFile(file("a.txt"), "local\n"));
  const QString before = head();
  QVERIFY(!GitRepo::cherryPickCommit(_repo.get(), picked, kAuthor, kEmail));
  QCOMPARE(lastError(), GitError::CherryPickCommitFailedToPrepareCherryPickCommit);
  QCOMPARE(readFile(file("a.txt")), QByteArray("local\n"));
  QCOMPARE(head(), before);
  QCOMPARE(git_repository_state(_repo.get()),
           static_cast<int>(GIT_REPOSITORY_STATE_NONE));
}

void GitRepoTest::cherryPickMergeCommitFails() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString base = gitCommitAll(QStringLiteral("Base"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("feature")});
  QVERIFY(writeFile(file("f.txt"), "f\n"));
  gitCommitAll(QStringLiteral("Feature"));
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("main")});
  git({QStringLiteral("merge"), QStringLiteral("-q"), QStringLiteral("--no-ff"),
       QStringLiteral("-m"), QStringLiteral("Merge"), QStringLiteral("feature")});
  const QString mergeCommit = head();
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("other"), base});

  // Picking or reverting a merge needs a mainline, which isn't passed.
  QVERIFY(!GitRepo::cherryPickCommit(_repo.get(), mergeCommit, kAuthor, kEmail));
  QCOMPARE(lastError(), GitError::CherryPickCommitFailedToPrepareCherryPickCommit);
  QCOMPARE(head(), base);
  QVERIFY(!GitRepo::revertCommit(_repo.get(), mergeCommit, kAuthor, kEmail,
                                 QStringLiteral("Revert")));
  QCOMPARE(lastError(), GitError::RevertCommitFailedToPrepareRevertCommit);
  QCOMPARE(head(), base);
  QCOMPARE(git({QStringLiteral("status"), QStringLiteral("--porcelain")}),
           QString());
}

void GitRepoTest::cherryPickAlreadyApplied() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY(writeFile(file("a.txt"), "a2\n"));
  const QString tip = gitCommitAll(QStringLiteral("Tip"));

  // A commit HEAD already contains changes nothing. Like `git cherry-pick`,
  // no empty commit is made.
  QVERIFY(!GitRepo::cherryPickCommit(_repo.get(), tip, kAuthor, kEmail));
  QCOMPARE(lastError(), GitError::CherryPickCommitIsEmpty);
  QCOMPARE(head(), tip);
  QCOMPARE(git_repository_state(_repo.get()),
           static_cast<int>(GIT_REPOSITORY_STATE_NONE));
  QCOMPARE(git({QStringLiteral("status"), QStringLiteral("--porcelain")}),
           QString());

  // The same for a revert of a change that is reverted already.
  git({QStringLiteral("revert"), QStringLiteral("--no-edit"), tip});
  const QString reverted = head();
  QVERIFY(!GitRepo::revertCommit(_repo.get(), tip, kAuthor, kEmail,
                                 QStringLiteral("Again")));
  QCOMPARE(lastError(), GitError::RevertCommitIsEmpty);
  QCOMPARE(head(), reverted);
  QCOMPARE(git_repository_state(_repo.get()),
           static_cast<int>(GIT_REPOSITORY_STATE_NONE));
}

void GitRepoTest::revertCommit() {
  QVERIFY(writeFile(file("a.txt"), "1\n"));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY(writeFile(file("a.txt"), "2\n"));
  QVERIFY(writeFile(file("n.txt"), "n\n"));
  const QString target = gitCommitAll(QStringLiteral("Target"));
  QVERIFY(writeFile(file("o.txt"), "o\n"));
  const QString tip = gitCommitAll(QStringLiteral("Tip"));

  QVERIFY2(GitRepo::revertCommit(_repo.get(), target, kOtherAuthor, kOtherEmail,
                                 QStringLiteral("Revert it")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("log"), QStringLiteral("-1"),
                QStringLiteral("--format=%P|%B|%an")}),
           tip + QStringLiteral("|Revert it|Other Author"));
  QCOMPARE(readFile(file("a.txt")), QByteArray("1\n"));
  QVERIFY(!QFileInfo::exists(file("n.txt")));
  QCOMPARE(readFile(file("o.txt")), QByteArray("o\n"));
  QCOMPARE(git({QStringLiteral("status"), QStringLiteral("--porcelain")}),
           QString());
  QCOMPARE(git_repository_state(_repo.get()),
           static_cast<int>(GIT_REPOSITORY_STATE_NONE));
}

void GitRepoTest::revertConflict() {
  QVERIFY(writeFile(file("a.txt"), "1\n"));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY(writeFile(file("a.txt"), "2\n"));
  const QString target = gitCommitAll(QStringLiteral("Target"));
  QVERIFY(writeFile(file("a.txt"), "3\n"));
  const QString tip = gitCommitAll(QStringLiteral("Tip"));

  // Like `git revert`: the conflicts are left in the working copy, HEAD stays
  // and the revert is in progress.
  QVERIFY(!GitRepo::revertCommit(_repo.get(), target, kAuthor, kEmail,
                                 QStringLiteral("Revert")));
  QCOMPARE(lastError(), GitError::RevertCommitFailedWithConflicts);
  QCOMPARE(head(), tip);
  QVERIFY(GitRepo::hasConflicts(_repo.get()));
  QVERIFY(readFile(file("a.txt")).contains("<<<<<<<"));
  QCOMPARE(git_repository_state(_repo.get()),
           static_cast<int>(GIT_REPOSITORY_STATE_REVERT));
  QCOMPARE(revParse(QStringLiteral("REVERT_HEAD")), target);

  QVERIFY2(GitRepo::abortMerge(_repo.get()), qPrintable(lastErrorMessage()));
  QCOMPARE(git_repository_state(_repo.get()),
           static_cast<int>(GIT_REPOSITORY_STATE_NONE));
  QCOMPARE(readFile(file("a.txt")), QByteArray("3\n"));
  QCOMPARE(head(), tip);
}

void GitRepoTest::revertRootCommit() {
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("--orphan"),
       QStringLiteral("other")});
  QVERIFY(writeFile(file("r.txt"), "r\n"));
  const QString root = gitCommitAll(QStringLiteral("Root"));
  QVERIFY(writeFile(file("s.txt"), "s\n"));
  const QString tip = gitCommitAll(QStringLiteral("Tip"));
  QCOMPARE(git({QStringLiteral("rev-list"), QStringLiteral("--max-parents=0"),
                QStringLiteral("HEAD")}),
           root);

  QVERIFY2(GitRepo::revertCommit(_repo.get(), root, kAuthor, kEmail,
                                 QStringLiteral("Revert root")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(revParse(QStringLiteral("HEAD~1")), tip);
  QVERIFY(!QFileInfo::exists(file("r.txt")));
  QCOMPARE(readFile(file("s.txt")), QByteArray("s\n"));
  QCOMPARE(git({QStringLiteral("status"), QStringLiteral("--porcelain")}),
           QString());
}

void GitRepoTest::pickAndRevertUnknownCommits_data() {
  QTest::addColumn<QString>("commit");
  QTest::newRow("unknown") << QString(40, QLatin1Char('d'));
  QTest::newRow("garbage") << QStringLiteral("no-such-ref");
  QTest::newRow("empty") << QString();
}

void GitRepoTest::pickAndRevertUnknownCommits() {
  QFETCH(QString, commit);
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString before = gitCommitAll(QStringLiteral("Base"));

  QVERIFY(!GitRepo::cherryPickCommit(_repo.get(), commit, kAuthor, kEmail));
  QCOMPARE(lastError(), GitError::CherryPickCommitFailedToLookupTargetCommit);
  QVERIFY(!GitRepo::revertCommit(_repo.get(), commit, kAuthor, kEmail,
                                 QStringLiteral("Revert")));
  QCOMPARE(lastError(), GitError::RevertCommitFailedToLookupTargetCommit);
  QCOMPARE(head(), before);
}

// Stashes.

void GitRepoTest::stashAndList() {
  setIdentity();
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY(GitRepo::listStashes(_repo.get(), _quit).isEmpty());

  QVERIFY(writeFile(file("a.txt"), "first\n"));
  QVERIFY2(GitRepo::stash(_repo.get(), QStringLiteral("first")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("a.txt")), QByteArray("a\n"));
  QVERIFY(!status().hasChanges());
  QCOMPARE(GitRepo::listStashes(_repo.get(), _quit),
           QStringList{QStringLiteral("On main: first")});

  // Without a note git's default text is used. The newest stash comes first.
  QVERIFY(writeFile(file("a.txt"), "second\n"));
  QVERIFY2(GitRepo::stash(_repo.get(), QString()),
           qPrintable(lastErrorMessage()));
  QStringList stashes = GitRepo::listStashes(_repo.get(), _quit);
  QCOMPARE(stashes.size(), 2);
  QVERIFY2(stashes.first().startsWith(QStringLiteral("WIP on main: ")) &&
               stashes.first().endsWith(QStringLiteral(" Base")),
           qPrintable(stashes.first()));
  QCOMPARE(stashes.last(), QStringLiteral("On main: first"));

  const QString unicode = QString::fromUtf16(u"заметка");
  QVERIFY(writeFile(file("a.txt"), "third\n"));
  QVERIFY(GitRepo::stash(_repo.get(), unicode));
  stashes = GitRepo::listStashes(_repo.get(), _quit);
  QCOMPARE(stashes.first(), QStringLiteral("On main: ") + unicode);
  QCOMPARE(git({QStringLiteral("stash"), QStringLiteral("list")}).count('\n'), 2);

  _quit = true;
  // Listing stashes is not cancellable.
  QCOMPARE(GitRepo::listStashes(_repo.get(), _quit).size(), 3);
}

void GitRepoTest::stashWithoutIdentityFails() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY(writeFile(file("a.txt"), "changed\n"));

  // The stasher comes from user.name and user.email, like for `git stash`, not
  // from the author passed to openRepository.
  QVERIFY(!GitRepo::stash(_repo.get(), QStringLiteral("note")));
  QCOMPARE(lastError(), GitError::StashFailedToGetDefaultSignature);
  QCOMPARE(readFile(file("a.txt")), QByteArray("changed\n"));
  QVERIFY(GitRepo::listStashes(_repo.get(), _quit).isEmpty());
}

void GitRepoTest::stashIncludesUntrackedFiles() {
  setIdentity();
  QVERIFY(writeFile(file(".gitignore"), "*.log\n"));
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString base = gitCommitAll(QStringLiteral("Base"));
  QVERIFY(writeFile(file("a.txt"), "changed\n"));
  QVERIFY(writeFile(file("u.txt"), "untracked\n"));
  QVERIFY(writeFile(file("i.log"), "ignored\n"));

  QVERIFY2(GitRepo::stash(_repo.get(), QStringLiteral("with untracked")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("a.txt")), QByteArray("a\n"));
  QVERIFY(!QFileInfo::exists(file("u.txt")));
  // Ignored files are not stashed.
  QCOMPARE(readFile(file("i.log")), QByteArray("ignored\n"));

  const GitRepo::GitCommitStatus st = GitRepo::getStashStatus(_repo.get(), 0);
  QCOMPARE(st.commit, revParse(QStringLiteral("stash@{0}")));
  QCOMPARE(st.summary, QStringLiteral("On main: with untracked"));
  // Parents: HEAD, the index and the untracked files.
  QCOMPARE(st.parentShas.size(), 3);
  QCOMPARE(st.parentShas.first(), base);
  QCOMPARE(st.modifiedFiles, QStringList{QStringLiteral("a.txt")});
  QCOMPARE(st.untrackedFiles, QStringList{QStringLiteral("u.txt")});
  QVERIFY(st.newFiles.isEmpty());
  QCOMPARE(st.authorName, kAuthor);

  QVERIFY2(GitRepo::applyStash(_repo.get(), 0), qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("a.txt")), QByteArray("changed\n"));
  QCOMPARE(readFile(file("u.txt")), QByteArray("untracked\n"));
}

void GitRepoTest::stashCleanWorkdirFails() {
  setIdentity();
  QVERIFY(writeFile(file(".gitignore"), "*.log\n"));
  gitCommitAll(QStringLiteral("Base"));

  QVERIFY(!GitRepo::stash(_repo.get(), QStringLiteral("nothing")));
  QCOMPARE(lastError(), GitError::StashFailedToCreateStash);
  // Ignored files don't count.
  QVERIFY(writeFile(file("i.log"), "ignored\n"));
  QVERIFY(!GitRepo::stash(_repo.get(), QStringLiteral("nothing")));
  QCOMPARE(lastError(), GitError::StashFailedToCreateStash);
  QVERIFY(GitRepo::listStashes(_repo.get(), _quit).isEmpty());
  QVERIFY(QFileInfo::exists(file("i.log")));
}

void GitRepoTest::applyStashKeepsEntry() {
  setIdentity();
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY(writeFile(file("a.txt"), "changed\n"));
  QVERIFY(GitRepo::stash(_repo.get(), QStringLiteral("keep")));

  QVERIFY2(GitRepo::applyStash(_repo.get(), 0), qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("a.txt")), QByteArray("changed\n"));
  QCOMPARE(GitRepo::listStashes(_repo.get(), _quit).size(), 1);
  QCOMPARE(status().modifiedFiles, QStringList{QStringLiteral("a.txt")});
  QVERIFY(!GitRepo::hasConflicts(_repo.get()));
  // Without conflict markers there are no conflicts to list.
  QVERIFY(GitRepo::listConflictsForStashFile(_repo.get(), 0,
                                             QStringLiteral("a.txt"))
              .isEmpty());
  QVERIFY(GitRepo::listConflictsForStashFile(_repo.get(), 0,
                                             QStringLiteral("missing.txt"))
              .isEmpty());
}

void GitRepoTest::applyStashConflict() {
  setIdentity();
  QVERIFY(writeFile(file("a.txt"), "base\n"));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY(writeFile(file("a.txt"), "stashed\n"));
  QVERIFY(GitRepo::stash(_repo.get(), QStringLiteral("conflict")));
  QVERIFY(writeFile(file("a.txt"), "committed\n"));
  // Committed with git: applyStash must not use the index libgit2 read
  // before.
  gitCommitAll(QStringLiteral("Committed"));

  QVERIFY2(GitRepo::applyStash(_repo.get(), 0), qPrintable(lastErrorMessage()));
  const QByteArray content = readFile(file("a.txt"));
  QVERIFY2(content.contains("<<<<<<< Updated upstream") &&
               content.contains(">>>>>>> Stashed changes"),
           content.constData());
  QCOMPARE(GitRepo::listStashes(_repo.get(), _quit).size(), 1);
  QVERIFY(GitRepo::listConflictsForStashFile(_repo.get(), 0,
                                             QStringLiteral("a.txt"))
              .contains(QStringLiteral("<<<<<<< Updated upstream")));
  QVERIFY(GitRepo::hasConflicts(_repo.get()));
  QCOMPARE(status().conflictedFiles, QStringList{QStringLiteral("a.txt")});
}

void GitRepoTest::applyStashWithDirtyWorkdir() {
  setIdentity();
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(writeFile(file("b.txt"), "b\n"));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY(writeFile(file("a.txt"), "stashed\n"));
  QVERIFY(GitRepo::stash(_repo.get(), QStringLiteral("dirty")));

  // A local change to a file in the stash blocks the apply.
  QVERIFY(writeFile(file("a.txt"), "local\n"));
  QVERIFY(!GitRepo::applyStash(_repo.get(), 0));
  QCOMPARE(lastError(), GitError::ApplyStashFailedToApplyStash);
  QCOMPARE(readFile(file("a.txt")), QByteArray("local\n"));
  QCOMPARE(GitRepo::listStashes(_repo.get(), _quit).size(), 1);

  // Other local changes stay.
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(writeFile(file("b.txt"), "local b\n"));
  QVERIFY2(GitRepo::applyStash(_repo.get(), 0), qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("a.txt")), QByteArray("stashed\n"));
  QCOMPARE(readFile(file("b.txt")), QByteArray("local b\n"));
}

void GitRepoTest::stashInvalidIndexes_data() {
  QTest::addColumn<int>("index");
  QTest::newRow("negative") << -1;
  QTest::newRow("count") << 1;
  QTest::newRow("large") << 1000;
}

void GitRepoTest::stashInvalidIndexes() {
  QFETCH(int, index);
  setIdentity();
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY(writeFile(file("a.txt"), "changed\n"));
  QVERIFY(GitRepo::stash(_repo.get(), QStringLiteral("only")));

  QVERIFY(!GitRepo::applyStash(_repo.get(), index));
  QCOMPARE(lastError(), GitError::ApplyStashFailedToApplyStash);
  QVERIFY(!GitRepo::deleteStash(_repo.get(), index));
  QCOMPARE(lastError(), GitError::DeleteStashFailedToDropStash);
  const GitRepo::GitCommitStatus st = GitRepo::getStashStatus(_repo.get(), index);
  QVERIFY(st.commit.isEmpty());
  QCOMPARE(lastError(), GitError::GetStashStatusFailedToResolveRevision);
  QCOMPARE(GitRepo::listStashes(_repo.get(), _quit).size(), 1);
  QCOMPARE(readFile(file("a.txt")), QByteArray("a\n"));
}

void GitRepoTest::deleteStashReindexes() {
  setIdentity();
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  for (const QString &note : {QStringLiteral("one"), QStringLiteral("two"),
                              QStringLiteral("three")}) {
    QVERIFY(writeFile(file("a.txt"), note.toUtf8() + '\n'));
    QVERIFY(GitRepo::stash(_repo.get(), note));
  }
  QCOMPARE(GitRepo::listStashes(_repo.get(), _quit),
           (QStringList{QStringLiteral("On main: three"),
                        QStringLiteral("On main: two"),
                        QStringLiteral("On main: one")}));

  QVERIFY2(GitRepo::deleteStash(_repo.get(), 1), qPrintable(lastErrorMessage()));
  QCOMPARE(GitRepo::listStashes(_repo.get(), _quit),
           (QStringList{QStringLiteral("On main: three"),
                        QStringLiteral("On main: one")}));
  QCOMPARE(GitRepo::getStashStatus(_repo.get(), 1).summary,
           QStringLiteral("On main: one"));
  QVERIFY(GitRepo::deleteStash(_repo.get(), 0));
  QCOMPARE(GitRepo::listStashes(_repo.get(), _quit),
           QStringList{QStringLiteral("On main: one")});
  QVERIFY(GitRepo::applyStash(_repo.get(), 0));
  QCOMPARE(readFile(file("a.txt")), QByteArray("one\n"));
}

void GitRepoTest::deleteAllStashes() {
  setIdentity();
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY2(GitRepo::deleteAllStashes(_repo.get()), qPrintable(lastErrorMessage()));

  for (int i = 0; i < 3; ++i) {
    QVERIFY(writeFile(file("a.txt"), QByteArray::number(i) + '\n'));
    QVERIFY(GitRepo::stash(_repo.get(), QString::number(i)));
  }
  QVERIFY2(GitRepo::deleteAllStashes(_repo.get()), qPrintable(lastErrorMessage()));
  QVERIFY(GitRepo::listStashes(_repo.get(), _quit).isEmpty());
  QVERIFY(revParse(QStringLiteral("refs/stash")).isEmpty());
  QCOMPARE(readFile(file("a.txt")), QByteArray("a\n"));
}

void GitRepoTest::stashStagedAndUnstagedSameFile() {
  setIdentity();
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY(writeFile(file("a.txt"), "staged\n"));
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  QVERIFY(writeFile(file("a.txt"), "unstaged\n"));

  QVERIFY2(GitRepo::stash(_repo.get(), QStringLiteral("both")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("a.txt")), QByteArray("a\n"));
  QCOMPARE(git({QStringLiteral("show"), QStringLiteral(":a.txt")}),
           QStringLiteral("a"));

  // Both versions come back, the staged one to the index.
  QVERIFY2(GitRepo::applyStash(_repo.get(), 0), qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("a.txt")), QByteArray("unstaged\n"));
  QCOMPARE(git({QStringLiteral("show"), QStringLiteral(":a.txt")}),
           QStringLiteral("staged"));
  const GitRepo::GitStatus st = status();
  QCOMPARE(st.modifiedStagedFiles, QStringList{QStringLiteral("a.txt")});
  QCOMPARE(st.modifiedFiles, QStringList{QStringLiteral("a.txt")});
}

void GitRepoTest::stashBinaryFile() {
  setIdentity();
  const QByteArray original("\0\1\2\3original\0", 13);
  const QByteArray changed("\0\3\2\1changed\0\xff", 13);
  QVERIFY(writeFile(file("bin.dat"), original));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY(writeFile(file("bin.dat"), changed));

  QVERIFY2(GitRepo::stash(_repo.get(), QStringLiteral("binary")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("bin.dat")), original);
  QCOMPARE(GitRepo::getStashStatus(_repo.get(), 0).modifiedFiles,
           QStringList{QStringLiteral("bin.dat")});
  QVERIFY2(GitRepo::applyStash(_repo.get(), 0), qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("bin.dat")), changed);
}

// Worktrees.

void GitRepoTest::addWorktreeNewAndExistingBranch() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString base = gitCommitAll(QStringLiteral("Base"));
  const QString path1 = _base + QStringLiteral("/wt1");
  const QString path2 = _base + QStringLiteral("/wt2");

  // A new branch at HEAD.
  QVERIFY2(GitRepo::addWorktree(_repo.get(), QStringLiteral("wt1"), path1,
                                QStringLiteral("topic"), true),
           qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(path1 + QStringLiteral("/a.txt")), QByteArray("a\n"));
  QCOMPARE(gitIn(_config, path1,
                 {QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/topic"));
  QCOMPARE(revParse(QStringLiteral("topic")), base);

  // An existing branch.
  git({QStringLiteral("branch"), QStringLiteral("existing")});
  QVERIFY2(GitRepo::addWorktree(_repo.get(), QStringLiteral("wt2"), path2,
                                QStringLiteral("existing"), false,
                                QStringLiteral("ignored")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(gitIn(_config, path2,
                 {QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/existing"));
  RepoPtr worktree(GitRepo::openRepository(path2, kAuthor, kEmail));
  QVERIFY(worktree);
  QVERIFY(!GitRepo::getStatus(worktree.get(), _quit).hasChanges());
  QCOMPARE(GitRepo::getActiveBranch(worktree.get(), GitRepo::BranchType::Local,
                                    _quit),
           QStringLiteral("existing"));

  // The main working copy is untouched.
  QCOMPARE(GitRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local,
                                    _quit),
           QStringLiteral("main"));
  QCOMPARE(git({QStringLiteral("worktree"), QStringLiteral("list"),
                QStringLiteral("--porcelain")})
               .count(QStringLiteral("worktree ")),
           3);
}

void GitRepoTest::addWorktreeFailures_data() {
  QTest::addColumn<QString>("name");
  QTest::addColumn<QString>("path");
  QTest::addColumn<QString>("branch");
  QTest::addColumn<bool>("createBranch");
  QTest::addColumn<QString>("baseBranch");
  QTest::addColumn<GitRepo::GitError>("error");
  const QString wt = QStringLiteral("wt");
  QTest::newRow("no branch") << wt << wt << QString() << true << QString()
                             << GitError::AddWorktreeBranchRequired;
  QTest::newRow("checked out branch")
      << wt << wt << QStringLiteral("main") << false << QString()
      << GitError::AddWorktreeFailedToAddWorktree;
  QTest::newRow("branch exists")
      << wt << wt << QStringLiteral("main") << true << QString()
      << GitError::AddWorktreeFailedToCreateBranch;
  QTest::newRow("missing branch")
      << wt << wt << QStringLiteral("nope") << false << QString()
      << GitError::AddWorktreeFailedToLookupBranch;
  QTest::newRow("missing base branch")
      << wt << wt << QStringLiteral("new") << true << QStringLiteral("nope")
      << GitError::AddWorktreeFailedToLookupBranch;
  QTest::newRow("invalid branch name")
      << wt << wt << QStringLiteral("a..b") << true << QString()
      << GitError::AddWorktreeFailedToCreateBranch;
  QTest::newRow("duplicate name")
      << QStringLiteral("dup") << wt << QStringLiteral("new") << true << QString()
      << GitError::AddWorktreeFailedToAddWorktree;
  QTest::newRow("duplicate path")
      << wt << QStringLiteral("dup") << QStringLiteral("new") << true << QString()
      << GitError::AddWorktreeFailedToAddWorktree;
  QTest::newRow("directory not empty")
      << wt << QStringLiteral("full") << QStringLiteral("new") << true
      << QString() << GitError::AddWorktreeFailedToAddWorktree;
}

void GitRepoTest::addWorktreeFailures() {
  QFETCH(QString, name);
  QFETCH(QString, path);
  QFETCH(QString, branch);
  QFETCH(bool, createBranch);
  QFETCH(QString, baseBranch);
  QFETCH(GitRepo::GitError, error);

  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY(GitRepo::addWorktree(_repo.get(), QStringLiteral("dup"),
                               _base + QStringLiteral("/dup"),
                               QStringLiteral("dupbranch"), true));
  QVERIFY(writeFile(_base + QStringLiteral("/full/keep.txt"), "keep\n"));

  QVERIFY(!GitRepo::addWorktree(_repo.get(), name, _base + '/' + path, branch,
                                createBranch, baseBranch));
  QCOMPARE(lastError(), error);
  // Nothing is left behind: no branch, no worktree, no directory.
  QCOMPARE(localBranches(),
           (QStringList{QStringLiteral("dupbranch"), QStringLiteral("main")}));
  QCOMPARE(GitRepo::listWorktrees(_repo.get()).size(), 2);
  QCOMPARE(readFile(_base + QStringLiteral("/full/keep.txt")),
           QByteArray("keep\n"));
  QCOMPARE(readFile(_base + QStringLiteral("/dup/a.txt")), QByteArray("a\n"));
  QVERIFY(!QFileInfo::exists(_base + QStringLiteral("/wt")));
}

void GitRepoTest::addWorktreeFromBaseBranch() {
  QVERIFY(writeFile(file("a.txt"), "root\n"));
  const QString root = gitCommitAll(QStringLiteral("Root"));
  git({QStringLiteral("branch"), QStringLiteral("base")});
  QVERIFY(writeFile(file("a.txt"), "tip\n"));
  const QString tip = gitCommitAll(QStringLiteral("Tip"));

  const QString fromBase = _base + QStringLiteral("/frombase");
  QVERIFY2(GitRepo::addWorktree(_repo.get(), QStringLiteral("frombase"), fromBase,
                                QStringLiteral("fromBase"), true,
                                QStringLiteral("base")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(revParse(QStringLiteral("fromBase")), root);
  QCOMPARE(readFile(fromBase + QStringLiteral("/a.txt")), QByteArray("root\n"));

  // An empty directory that already exists is used.
  const QString fromHead = _base + QStringLiteral("/fromhead");
  QVERIFY(QDir().mkpath(fromHead));
  QVERIFY2(GitRepo::addWorktree(_repo.get(), QStringLiteral("fromhead"), fromHead,
                                QStringLiteral("fromHead"), true),
           qPrintable(lastErrorMessage()));
  QCOMPARE(revParse(QStringLiteral("fromHead")), tip);
  QCOMPARE(readFile(fromHead + QStringLiteral("/a.txt")), QByteArray("tip\n"));
}

void GitRepoTest::addWorktreeUnusualPaths_data() {
  QTest::addColumn<QString>("path");
  QTest::addColumn<bool>("native");
  QTest::newRow("spaces") << QStringLiteral("my worktree") << false;
  QTest::newRow("unicode") << QString::fromUtf16(u"wörk ünï") << false;
  QTest::newRow("nested") << QStringLiteral("deep/er/wt") << false;
  QTest::newRow("native separators") << QStringLiteral("native/wt") << true;
}

void GitRepoTest::addWorktreeUnusualPaths() {
  QFETCH(QString, path);
  QFETCH(bool, native);
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));

  const QString expected = _base + '/' + path;
  const QString argument = native ? QDir::toNativeSeparators(expected) : expected;
  QVERIFY2(GitRepo::addWorktree(_repo.get(), QStringLiteral("wt"), argument,
                                QStringLiteral("topic"), true),
           qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(expected + QStringLiteral("/a.txt")), QByteArray("a\n"));

  const QVector<GitRepo::WorktreeInfo> worktrees =
      GitRepo::listWorktrees(_repo.get());
  QCOMPARE(worktrees.size(), 2);
  QCOMPARE(QFileInfo(worktrees.last().path).canonicalFilePath(),
           QFileInfo(expected).canonicalFilePath());
  QVERIFY(worktrees.last().valid);
  QCOMPARE(worktrees.last().branch, QStringLiteral("topic"));

  RepoPtr worktree(GitRepo::openRepository(argument, kAuthor, kEmail));
  QVERIFY2(worktree, qPrintable(lastErrorMessage()));
  QVERIFY(!GitRepo::getStatus(worktree.get(), _quit).hasChanges());
}

void GitRepoTest::listWorktreesFields() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString base = gitCommitAll(QStringLiteral("Base"));
  QVERIFY(GitRepo::addWorktree(_repo.get(), QStringLiteral("zeta"),
                               _base + QStringLiteral("/zeta"),
                               QStringLiteral("z"), true));
  QVERIFY(GitRepo::addWorktree(_repo.get(), QStringLiteral("Alpha"),
                               _base + QStringLiteral("/alpha"),
                               QStringLiteral("a"), true));
  git({QStringLiteral("worktree"), QStringLiteral("add"), QStringLiteral("-q"),
       QStringLiteral("--detach"), _base + QStringLiteral("/det")});

  const QVector<GitRepo::WorktreeInfo> list = GitRepo::listWorktrees(_repo.get());
  QCOMPARE(list.size(), 4);
  // The main worktree first, then by name, ignoring case.
  QCOMPARE(list[0].name, QStringLiteral("repo"));
  QVERIFY(list[0].isMain);
  QVERIFY(list[0].isCurrent);
  QCOMPARE(QFileInfo(list[0].path).canonicalFilePath(),
           QFileInfo(_root).canonicalFilePath());
  QCOMPARE(list[0].branch, QStringLiteral("main"));
  QCOMPARE(list[0].head, base.left(7));
  QVERIFY(list[0].valid);
  QVERIFY(!list[0].locked);
  QCOMPARE(list[1].name, QStringLiteral("Alpha"));
  QCOMPARE(list[1].branch, QStringLiteral("a"));
  QCOMPARE(list[2].name, QStringLiteral("det"));
  QCOMPARE(list[2].branch, QString());
  QCOMPARE(list[3].name, QStringLiteral("zeta"));
  QCOMPARE(list[3].branch, QStringLiteral("z"));
  for (int i = 1; i < list.size(); ++i) {
    QVERIFY(!list[i].isMain);
    QVERIFY(!list[i].isCurrent);
    QVERIFY(list[i].valid);
    QVERIFY(!list[i].locked);
    QCOMPARE(list[i].head, base.left(7));
    QVERIFY(!list[i].path.endsWith('/'));
    QVERIFY(!list[i].path.contains('\\'));
  }

  // From a linked worktree the list is the same, only the current one moves.
  RepoPtr zeta(GitRepo::openRepository(_base + QStringLiteral("/zeta"), kAuthor,
                                       kEmail));
  QVERIFY(zeta);
  const QVector<GitRepo::WorktreeInfo> fromZeta = GitRepo::listWorktrees(zeta.get());
  QCOMPARE(fromZeta.size(), list.size());
  for (int i = 0; i < list.size(); ++i) {
    QCOMPARE(fromZeta[i].name, list[i].name);
    QCOMPARE(QFileInfo(fromZeta[i].path).canonicalFilePath(),
             QFileInfo(list[i].path).canonicalFilePath());
    QCOMPARE(fromZeta[i].branch, list[i].branch);
    QCOMPARE(fromZeta[i].head, list[i].head);
    QCOMPARE(fromZeta[i].isMain, list[i].isMain);
    QCOMPARE(fromZeta[i].isCurrent, list[i].name == QStringLiteral("zeta"));
  }
}

void GitRepoTest::listWorktreesMissingDirectory() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  const QString path = _base + QStringLiteral("/gone");
  QVERIFY(GitRepo::addWorktree(_repo.get(), QStringLiteral("gone"), path,
                               QStringLiteral("gonebranch"), true));
  makeWritable(path);
  QVERIFY(QDir(path).removeRecursively());

  QVector<GitRepo::WorktreeInfo> list = GitRepo::listWorktrees(_repo.get());
  QCOMPARE(list.size(), 2);
  QCOMPARE(list[1].name, QStringLiteral("gone"));
  QVERIFY(!list[1].valid);

  // Removing it cleans up the administrative data and frees the branch.
  QVERIFY2(GitRepo::removeWorktree(_repo.get(), QStringLiteral("gone"), false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(GitRepo::listWorktrees(_repo.get()).size(), 1);
  QVERIFY(!QFileInfo::exists(file(".git/worktrees/gone")));
  QVERIFY2(GitRepo::deleteBranch(_repo.get(), QStringLiteral("gonebranch")),
           qPrintable(lastErrorMessage()));
}

void GitRepoTest::lockAndUnlockWorktree() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  const QString path = _base + QStringLiteral("/wt");
  QVERIFY(GitRepo::addWorktree(_repo.get(), QStringLiteral("wt"), path,
                               QStringLiteral("topic"), true));
  const auto isLocked = [this] {
    return GitRepo::listWorktrees(_repo.get()).last().locked;
  };

  QVERIFY2(GitRepo::lockWorktree(_repo.get(), QStringLiteral("wt"), QString()),
           qPrintable(lastErrorMessage()));
  QVERIFY(isLocked());
  // Locking twice and unlocking twice are fine.
  QVERIFY(GitRepo::lockWorktree(_repo.get(), QStringLiteral("wt"),
                                QStringLiteral("again")));
  QVERIFY(isLocked());
  QVERIFY2(GitRepo::unlockWorktree(_repo.get(), QStringLiteral("wt")),
           qPrintable(lastErrorMessage()));
  QVERIFY(!isLocked());
  QVERIFY(GitRepo::unlockWorktree(_repo.get(), QStringLiteral("wt")));
  QVERIFY(!isLocked());

  // The reason is written for git.
  QVERIFY(GitRepo::lockWorktree(_repo.get(), QStringLiteral("wt"),
                                QStringLiteral("on a usb stick")));
  QVERIFY(git({QStringLiteral("worktree"), QStringLiteral("list"),
               QStringLiteral("--porcelain")})
              .contains(QStringLiteral("locked on a usb stick")));

  // A locked worktree is only removed by force.
  QVERIFY(!GitRepo::removeWorktree(_repo.get(), QStringLiteral("wt"), false));
  QCOMPARE(lastError(), GitError::RemoveWorktreeFailedToPrune);
  QCOMPARE(readFile(path + QStringLiteral("/a.txt")), QByteArray("a\n"));
  QVERIFY2(GitRepo::removeWorktree(_repo.get(), QStringLiteral("wt"), true),
           qPrintable(lastErrorMessage()));
  QVERIFY(!QFileInfo::exists(path));

  // The main worktree can't be locked, a missing one isn't found.
  QVERIFY(!GitRepo::lockWorktree(_repo.get(), QStringLiteral("repo"), QString()));
  QCOMPARE(lastError(), GitError::LockWorktreeMainNotAllowed);
  QVERIFY(!GitRepo::unlockWorktree(_repo.get(), QStringLiteral("repo")));
  QCOMPARE(lastError(), GitError::UnlockWorktreeMainNotAllowed);
  QVERIFY(!GitRepo::lockWorktree(_repo.get(), QStringLiteral("wt"), QString()));
  QCOMPARE(lastError(), GitError::LockWorktreeFailedToLookupWorktree);
  QVERIFY(!GitRepo::unlockWorktree(_repo.get(), QStringLiteral("wt")));
  QCOMPARE(lastError(), GitError::UnlockWorktreeFailedToLookupWorktree);
}

void GitRepoTest::removeWorktree() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  const QString path = _base + QStringLiteral("/wt");
  QVERIFY(GitRepo::addWorktree(_repo.get(), QStringLiteral("wt"), path,
                               QStringLiteral("topic"), true));

  QVERIFY2(GitRepo::removeWorktree(_repo.get(), QStringLiteral("wt"), false),
           qPrintable(lastErrorMessage()));
  QVERIFY(!QFileInfo::exists(path));
  QVERIFY(!QFileInfo::exists(file(".git/worktrees/wt")));
  QCOMPARE(GitRepo::listWorktrees(_repo.get()).size(), 1);
  // The branch is kept.
  QCOMPARE(localBranches(),
           (QStringList{QStringLiteral("main"), QStringLiteral("topic")}));

  QVERIFY(!GitRepo::removeWorktree(_repo.get(), QStringLiteral("repo"), true));
  QCOMPARE(lastError(), GitError::RemoveWorktreeMainNotAllowed);
  QCOMPARE(readFile(file("a.txt")), QByteArray("a\n"));
  // A missing worktree isn't found.
  QVERIFY(!GitRepo::removeWorktree(_repo.get(), QStringLiteral("wt"), false));
  QCOMPARE(lastError(), GitError::RemoveWorktreeFailedToLookupWorktree);
}

void GitRepoTest::removeDirtyWorktreeNeedsForce() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  const QString path = _base + QStringLiteral("/wt");
  QVERIFY(GitRepo::addWorktree(_repo.get(), QStringLiteral("wt"), path,
                               QStringLiteral("topic"), true));
  QVERIFY(writeFile(path + QStringLiteral("/a.txt"), "dirty\n"));
  QVERIFY(writeFile(path + QStringLiteral("/u.txt"), "untracked\n"));

  // `git worktree remove` refuses this without --force.
  const bool removed =
      GitRepo::removeWorktree(_repo.get(), QStringLiteral("wt"), false);
  QVERIFY(!removed);
  QCOMPARE(lastError(), GitError::RemoveWorktreeHasChanges);
  QCOMPARE(readFile(path + QStringLiteral("/a.txt")), QByteArray("dirty\n"));

  // Untracked files alone count as well.
  QVERIFY(writeFile(path + QStringLiteral("/a.txt"), "a\n"));
  QVERIFY(!GitRepo::removeWorktree(_repo.get(), QStringLiteral("wt"), false));
  QCOMPARE(lastError(), GitError::RemoveWorktreeHasChanges);
  QCOMPARE(readFile(path + QStringLiteral("/u.txt")), QByteArray("untracked\n"));

  QVERIFY2(GitRepo::removeWorktree(_repo.get(), QStringLiteral("wt"), true),
           qPrintable(lastErrorMessage()));
  QVERIFY(!QFileInfo::exists(path));
}

void GitRepoTest::commitInWorktreeVisibleInMain() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString base = gitCommitAll(QStringLiteral("Base"));
  const QString path = _base + QStringLiteral("/wt");
  QVERIFY(GitRepo::addWorktree(_repo.get(), QStringLiteral("wt"), path,
                               QStringLiteral("topic"), true));
  RepoPtr worktree(GitRepo::openRepository(path, kAuthor, kEmail));
  QVERIFY(worktree);

  QVERIFY(writeFile(path + QStringLiteral("/w.txt"), "w\n"));
  QVERIFY(GitRepo::stageFiles(worktree.get(), {QStringLiteral("w.txt")}));
  QVERIFY2(GitRepo::commitStaged(worktree.get(), QStringLiteral("In worktree"),
                                 QString(), kAuthor, kEmail, true, false,
                                 false),
           qPrintable(lastErrorMessage()));
  const QString commit =
      gitIn(_config, path, {QStringLiteral("rev-parse"), QStringLiteral("HEAD")});

  QCOMPARE(revParse(QStringLiteral("topic")), commit);
  const GitRepo::GitHistory history = GitRepo::getBranchHistory(
      _repo.get(), QStringLiteral("topic"), GitRepo::BranchType::Local, 0, _quit);
  QVERIFY(history.valid);
  QCOMPARE(history.commits.first(), commit);
  QCOMPARE(GitRepo::getCommitBranches(_repo.get(), commit, _quit),
           QStringList{QStringLiteral("topic")});

  // The main working copy is untouched.
  QCOMPARE(head(), base);
  QVERIFY(!QFileInfo::exists(file("w.txt")));
  QCOMPARE(GitRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local,
                                    _quit),
           QStringLiteral("main"));
  QVERIFY(!status().hasChanges());
}

// Bisect.

void GitRepoTest::bisectFindsFirstBadCommit() {
  const QStringList commits = makeLinearHistory(10);
  const int firstBad = 6;
  GitRepo::BisectStatus st;

  QVERIFY2(GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Bad,
                           commits[9], st),
           qPrintable(lastErrorMessage()));
  QVERIFY(st.active);
  QVERIFY(st.waitingForGood);
  QVERIFY(!st.waitingForBad);
  QVERIFY(!st.finished);
  QVERIFY(st.currentCommit.isEmpty());
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/main"));

  QVERIFY2(GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Good,
                           commits[0], st),
           qPrintable(lastErrorMessage()));
  QVERIFY(!st.waitingForGood);
  QVERIFY(!st.waitingForBad);
  QCOMPARE(st.remaining, 9);
  QCOMPARE(st.steps, 3);
  QVERIFY(!st.currentCommit.isEmpty());
  QCOMPARE(head(), st.currentCommit);
  bool attached = true;
  git({QStringLiteral("symbolic-ref"), QStringLiteral("-q"), QStringLiteral("HEAD")},
      &attached);
  QVERIFY(!attached);

  int rounds = 0;
  int remaining = st.remaining;
  while (!st.finished) {
    QVERIFY(++rounds <= 5);
    const int index = commits.indexOf(st.currentCommit);
    QVERIFY2(index > 0 && index < 9, qPrintable(st.currentCommit));
    QCOMPARE(st.currentSummary, QStringLiteral("C%1").arg(index));
    QCOMPARE(readFile(file("v.txt")), QByteArray::number(index) + '\n');
    // bisect() resets the status first, so the commit is passed as a copy.
    const QString current = st.currentCommit;
    QVERIFY2(GitRepo::bisect(_repo.get(),
                             index < firstBad ? GitRepo::BisectAction::Good
                                              : GitRepo::BisectAction::Bad,
                             current, st),
             qPrintable(lastErrorMessage()));
    QVERIFY(st.remaining < remaining);
    remaining = st.remaining;
  }
  QVERIFY(!st.ambiguous);
  QCOMPARE(st.firstBadCommit, commits[firstBad]);
  QCOMPARE(st.firstBadSummary, QStringLiteral("C6"));
  QVERIFY(readFile(file(".git/BISECT_LOG"))
              .contains("# first bad commit: [" + commits[firstBad].toLatin1() +
                        "] C6"));

  QVERIFY2(GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Reset, QString(),
                           st),
           qPrintable(lastErrorMessage()));
  QVERIFY(!st.active);
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/main"));
  QCOMPARE(head(), commits[9]);
  QVERIFY(!QFileInfo::exists(file(".git/BISECT_START")));
  QVERIFY(!QFileInfo::exists(file(".git/BISECT_LOG")));
  QCOMPARE(git({QStringLiteral("for-each-ref"), QStringLiteral("refs/bisect")}),
           QString());
}

void GitRepoTest::bisectWaitingStates_data() {
  QTest::addColumn<GitRepo::BisectAction>("action");
  QTest::addColumn<bool>("waitingForGood");
  QTest::addColumn<bool>("waitingForBad");
  QTest::newRow("good only") << GitRepo::BisectAction::Good << false << true;
  QTest::newRow("bad only") << GitRepo::BisectAction::Bad << true << false;
  QTest::newRow("skip only") << GitRepo::BisectAction::Skip << true << true;
}

void GitRepoTest::bisectWaitingStates() {
  QFETCH(GitRepo::BisectAction, action);
  QFETCH(bool, waitingForGood);
  QFETCH(bool, waitingForBad);
  const QStringList commits = makeLinearHistory(3);
  GitRepo::BisectStatus st;

  // Revisions are resolved, by name or abbreviated.
  const QString commit = action == GitRepo::BisectAction::Good
                             ? commits[0].left(10)
                             : QStringLiteral("main");
  QVERIFY2(GitRepo::bisect(_repo.get(), action, commit, st),
           qPrintable(lastErrorMessage()));
  QVERIFY(st.active);
  QVERIFY(!st.finished);
  QCOMPARE(st.waitingForGood, waitingForGood);
  QCOMPARE(st.waitingForBad, waitingForBad);
  QVERIFY(st.currentCommit.isEmpty());
  QCOMPARE(head(), commits[2]);
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/main"));
  QCOMPARE(readFile(file(".git/BISECT_START")), QByteArray("main\n"));
  QCOMPARE(GitRepo::getBisectMarks(_repo.get()).size(), 1);

  // With both ends known the middle commit is checked out.
  QVERIFY(GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Good, commits[0],
                          st));
  QVERIFY(GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Bad, commits[2],
                          st));
  QVERIFY(!st.waitingForGood);
  QVERIFY(!st.waitingForBad);
  QCOMPARE(st.currentCommit, commits[1]);
  QCOMPARE(st.remaining, 2);
  QCOMPARE(st.steps, 1);
  QCOMPARE(head(), commits[1]);
}

void GitRepoTest::bisectSkipAmbiguous() {
  const QStringList commits = makeLinearHistory(5);
  GitRepo::BisectStatus st;
  QVERIFY(GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Bad, commits[4],
                          st));
  QVERIFY(GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Good, commits[0],
                          st));

  // Every candidate but the bad commit is skipped.
  int skips = 0;
  while (!st.finished) {
    QVERIFY(++skips <= 3);
    const QString current = st.currentCommit;
    QVERIFY2(GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Skip, current,
                             st),
             qPrintable(lastErrorMessage()));
  }
  QCOMPARE(skips, 3);
  QVERIFY(st.ambiguous);
  QCOMPARE(st.firstBadCommit, commits[4]);
  const QHash<QString, int> marks = GitRepo::getBisectMarks(_repo.get());
  for (int i = 1; i < 4; ++i) {
    QCOMPARE(marks.value(commits[i]), static_cast<int>(GitRepo::BisectMark::Skip));
  }
}

void GitRepoTest::bisectInvalidRange() {
  const QStringList commits = makeLinearHistory(4);
  GitRepo::BisectStatus st;

  // The good commit is a descendant of the bad one.
  QVERIFY(GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Bad, commits[1],
                          st));
  QVERIFY(!GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Good, commits[3],
                           st));
  QCOMPARE(lastError(), GitError::BisectInvalidRange);
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/main"));
  QVERIFY(GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Reset, QString(),
                          st));

  // The same commit is good and bad.
  QVERIFY(GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Bad, commits[2],
                          st));
  QVERIFY(!GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Good, commits[2],
                           st));
  QCOMPARE(lastError(), GitError::BisectInvalidRange);
  QVERIFY(GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Reset, QString(),
                          st));
  QCOMPARE(head(), commits[3]);
}

void GitRepoTest::bisectResetRestoresHead() {
  const QStringList commits = makeLinearHistory(5);
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("topic")});
  GitRepo::BisectStatus st;
  QVERIFY(GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Bad, commits[4],
                          st));
  QVERIFY(GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Good, commits[0],
                          st));
  QVERIFY(head() != commits[4]);

  // Back to the branch the session started on.
  QVERIFY2(GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Reset, QString(),
                           st),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/topic"));
  QCOMPARE(head(), commits[4]);
  QCOMPARE(readFile(file("v.txt")), QByteArray("4\n"));
  QVERIFY(!GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Reset, QString(),
                           st));
  QCOMPARE(lastError(), GitError::BisectNotInProgress);

  // Started on a detached HEAD.
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("--detach"),
       commits[2]});
  QVERIFY(GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Bad, commits[4],
                          st));
  QVERIFY(GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Good, commits[0],
                          st));
  QVERIFY(GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Reset, QString(),
                          st));
  QCOMPARE(head(), commits[2]);
  bool attached = true;
  git({QStringLiteral("symbolic-ref"), QStringLiteral("-q"), QStringLiteral("HEAD")},
      &attached);
  QVERIFY(!attached);
}

void GitRepoTest::bisectBusyRepository() {
  makeMergeConflict();
  GitRepo::BisectStatus st;
  QVERIFY(!GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Bad,
                           QStringLiteral("HEAD"), st));
  QCOMPARE(lastError(), GitError::BisectRepositoryBusy);
  QVERIFY(!st.active);
  QVERIFY(!QFileInfo::exists(file(".git/BISECT_START")));
  QVERIFY(GitRepo::getIsMergeInProgress(_repo.get()));
}

void GitRepoTest::bisectInvalidArguments_data() {
  QTest::addColumn<QString>("commit");
  QTest::addColumn<GitRepo::GitError>("error");
  QTest::newRow("empty") << QString() << GitError::BisectInvalidArguments;
  QTest::newRow("unknown commit") << QString(40, QLatin1Char('d'))
                                  << GitError::BisectFailedToResolveCommit;
  QTest::newRow("unknown ref") << QStringLiteral("no-such-ref")
                               << GitError::BisectFailedToResolveCommit;
  QTest::newRow("tree") << QStringLiteral("HEAD^{tree}")
                        << GitError::BisectFailedToResolveCommit;
}

void GitRepoTest::bisectInvalidArguments() {
  QFETCH(QString, commit);
  QFETCH(GitRepo::GitError, error);
  makeLinearHistory(2);
  GitRepo::BisectStatus st;

  QVERIFY(!GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Bad, commit, st));
  QCOMPARE(lastError(), error);
  QVERIFY(!st.active);
  QVERIFY(!QFileInfo::exists(file(".git/BISECT_START")));
  QVERIFY(GitRepo::getBisectMarks(_repo.get()).isEmpty());
}

void GitRepoTest::bisectCliCompatibility() {
  const QStringList commits = makeLinearHistory(8);
  const int firstBad = 5;
  const auto verdict = [&commits](const QString &commit) {
    return commits.indexOf(commit) < firstBad ? GitRepo::BisectAction::Good
                                              : GitRepo::BisectAction::Bad;
  };
  GitRepo::BisectStatus st;
  bool ok = false;

  // Started with git, finished here.
  git({QStringLiteral("bisect"), QStringLiteral("start"), commits[7], commits[0]},
      &ok);
  QVERIFY(ok);
  const QHash<QString, int> marks = GitRepo::getBisectMarks(_repo.get());
  QCOMPARE(marks.value(commits[7]), static_cast<int>(GitRepo::BisectMark::Bad));
  QCOMPARE(marks.value(commits[0]), static_cast<int>(GitRepo::BisectMark::Good));
  QCOMPARE(marks.value(head()), static_cast<int>(GitRepo::BisectMark::Current));
  QString current = head();
  for (int round = 0; round < 6 && !st.finished; ++round) {
    QVERIFY(commits.indexOf(current) > 0);
    QVERIFY2(GitRepo::bisect(_repo.get(), verdict(current), current, st),
             qPrintable(lastErrorMessage()));
    current = st.currentCommit;
  }
  QVERIFY(st.finished);
  QCOMPARE(st.firstBadCommit, commits[firstBad]);
  QVERIFY2(GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Reset, QString(),
                           st),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/main"));
  git({QStringLiteral("bisect"), QStringLiteral("log")}, &ok);
  QVERIFY(!ok);
  for (const char *name :
       {".git/BISECT_TERMS", ".git/BISECT_NAMES", ".git/BISECT_EXPECTED_REV",
        ".git/BISECT_ANCESTORS_OK"}) {
    QVERIFY2(!QFileInfo::exists(file(name)), name);
  }

  // Started here, continued with git.
  QVERIFY(GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Bad, commits[7],
                          st));
  QVERIFY(GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Good, commits[0],
                          st));
  const QString log = git({QStringLiteral("bisect"), QStringLiteral("log")}, &ok);
  QVERIFY(ok);
  QVERIFY2(log.contains(QStringLiteral("git bisect good ") + commits[0]) &&
               log.contains(QStringLiteral("git bisect bad ") + commits[7]),
           qPrintable(log));
  const QString output =
      git({QStringLiteral("bisect"),
           verdict(head()) == GitRepo::BisectAction::Good ? QStringLiteral("good")
                                                          : QStringLiteral("bad")},
          &ok);
  QVERIFY2(ok, qPrintable(output));
  // And the marks git wrote are read here.
  QCOMPARE(GitRepo::getBisectMarks(_repo.get()).size(), 4);
  QVERIFY2(GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Reset, QString(),
                           st),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/main"));
  QCOMPARE(head(), commits[7]);
}

void GitRepoTest::bisectMarksInHistory() {
  const QStringList commits = makeLinearHistory(6);
  GitRepo::BisectStatus st;
  QVERIFY(GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Bad, commits[5],
                          st));
  QVERIFY(GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Good, commits[0],
                          st));
  const QString skipped = st.currentCommit;
  QVERIFY(GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Skip, skipped, st));
  QVERIFY(!st.currentCommit.isEmpty());
  QVERIFY(st.currentCommit != skipped);

  const QHash<QString, int> marks = GitRepo::getBisectMarks(_repo.get());
  QCOMPARE(marks.size(), 4);
  QCOMPARE(marks.value(commits[0]), static_cast<int>(GitRepo::BisectMark::Good));
  QCOMPARE(marks.value(commits[5]), static_cast<int>(GitRepo::BisectMark::Bad));
  QCOMPARE(marks.value(skipped), static_cast<int>(GitRepo::BisectMark::Skip));
  QCOMPARE(marks.value(st.currentCommit),
           static_cast<int>(GitRepo::BisectMark::Current));

  // Page 0 of the history carries the marks.
  GitRepo::GitHistory history = GitRepo::getBranchHistory(
      _repo.get(), QStringLiteral("main"), GitRepo::BranchType::Local, 0, _quit);
  QVERIFY(history.valid);
  QVERIFY(history.bisectMarks == marks);

  QVERIFY(GitRepo::bisect(_repo.get(), GitRepo::BisectAction::Reset, QString(),
                          st));
  QVERIFY(GitRepo::getBisectMarks(_repo.get()).isEmpty());
  history = GitRepo::getBranchHistory(_repo.get(), QStringLiteral("main"),
                                      GitRepo::BranchType::Local, 0, _quit);
  QVERIFY(history.bisectMarks.isEmpty());
}

// Remotes, targets and upstreams.

void GitRepoTest::addRemoteWithPushUrl() {
  const QString fetchUrl = QStringLiteral("https://example.invalid/fetch.git");
  const QString pushUrl = QStringLiteral("https://example.invalid/push.git");
  QVERIFY2(GitRepo::addRemote(_repo.get(), QStringLiteral("origin"), fetchUrl,
                              pushUrl),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("remote"), QStringLiteral("get-url"),
                QStringLiteral("origin")}),
           fetchUrl);
  QCOMPARE(git({QStringLiteral("remote"), QStringLiteral("get-url"),
                QStringLiteral("--push"), QStringLiteral("origin")}),
           pushUrl);
  QCOMPARE(git({QStringLiteral("config"), QStringLiteral("remote.origin.fetch")}),
           QStringLiteral("+refs/heads/*:refs/remotes/origin/*"));
  const GitRepo::RemoteInfo origin = remote(QStringLiteral("origin"));
  QCOMPARE(origin.fetchUrl, fetchUrl);
  QCOMPARE(origin.pushUrl, pushUrl);
  QVERIFY(origin.isDefault);
  QCOMPARE(targets(origin), QStringLiteral("Default|") + fetchUrl + '|' + pushUrl);

  // No push URL, or the fetch URL again, stores none.
  const QString second = QStringLiteral("https://example.invalid/second.git");
  QVERIFY(GitRepo::addRemote(_repo.get(), QStringLiteral("second"), second,
                             QString()));
  QVERIFY(GitRepo::addRemote(_repo.get(), QStringLiteral("third"), second,
                             second));
  for (const QString &name : {QStringLiteral("second"), QStringLiteral("third")}) {
    bool ok = true;
    git({QStringLiteral("config"), QStringLiteral("remote.%1.pushurl").arg(name)},
        &ok);
    QVERIFY(!ok);
    QCOMPARE(remote(name).pushUrl, second);
    QVERIFY(!remote(name).isDefault);
  }

  // A local path.
  const QString path = _base + QStringLiteral("/remote.git");
  QVERIFY(GitRepo::addRemote(_repo.get(), QStringLiteral("local"), path,
                             QString()));
  QCOMPARE(remote(QStringLiteral("local")).fetchUrl, path);
}

void GitRepoTest::addRemoteNames_data() {
  QTest::addColumn<QString>("name");
  QTest::addColumn<QString>("url");
  QTest::addColumn<bool>("accepted");
  const QString url = QStringLiteral("https://example.invalid/new.git");
  QTest::newRow("slash") << QStringLiteral("a/b") << url << true;
  QTest::newRow("unicode") << QString::fromUtf16(u"ünï") << url << true;
  QTest::newRow("dot") << QStringLiteral("my.remote") << url << true;
  QTest::newRow("case variant") << QStringLiteral("ORIGIN") << url << true;
  QTest::newRow("empty") << QString() << url << false;
  QTest::newRow("space") << QStringLiteral("a b") << url << false;
  QTest::newRow("dot dot") << QStringLiteral("a..b") << url << false;
  QTest::newRow("trailing slash") << QStringLiteral("a/") << url << false;
  QTest::newRow("lock suffix") << QStringLiteral("x.lock") << url << false;
  QTest::newRow("duplicate") << QStringLiteral("origin") << url << false;
  QTest::newRow("empty url") << QStringLiteral("new") << QString() << false;
}

void GitRepoTest::addRemoteNames() {
  QFETCH(QString, name);
  QFETCH(QString, url);
  QFETCH(bool, accepted);
  const QString originUrl = QStringLiteral("https://example.invalid/origin.git");
  QVERIFY(GitRepo::addRemote(_repo.get(), QStringLiteral("origin"), originUrl,
                             QString()));

  QCOMPARE(GitRepo::addRemote(_repo.get(), name, url, QString()), accepted);
  if (accepted) {
    QVERIFY(remoteNames().contains(name));
    QCOMPARE(git({QStringLiteral("remote"), QStringLiteral("get-url"), name}), url);
  } else {
    if (!name.isEmpty() && !url.isEmpty()) {
      QCOMPARE(lastError(), GitError::AddRemoteFailedToCreateRemote);
    }
    QCOMPARE(remoteNames(), QStringList{QStringLiteral("origin")});
  }
  QCOMPARE(remote(QStringLiteral("origin")).fetchUrl, originUrl);
}

void GitRepoTest::listRemotesSorted() {
  QVERIFY(GitRepo::listRemotes(_repo.get()).isEmpty());
  for (const QString &name : {QStringLiteral("zeta"), QStringLiteral("alpha"),
                              QStringLiteral("Mid"), QStringLiteral("origin")}) {
    QVERIFY(GitRepo::addRemote(_repo.get(), name,
                               QStringLiteral("https://example.invalid/") + name,
                               QString()));
  }
  const QStringList expected{QStringLiteral("Mid"), QStringLiteral("alpha"),
                             QStringLiteral("origin"), QStringLiteral("zeta")};
  QCOMPARE(remoteNames(), expected);

  // Targets are not listed as remotes.
  QVERIFY(GitRepo::addTarget(_repo.get(), QStringLiteral("origin"),
                             QStringLiteral("backup"),
                             QStringLiteral("https://example.invalid/b"),
                             QStringLiteral("https://example.invalid/bp")));
  QCOMPARE(remoteNames(), expected);
  QStringList fromGit =
      git({QStringLiteral("remote")}).split('\n', Qt::SkipEmptyParts);
  fromGit.sort();
  QCOMPARE(fromGit, expected);
}

void GitRepoTest::defaultRemoteFallbacks() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));

  // Without remotes there is no default.
  QCOMPARE(GitRepo::getDefaultRemote(_repo.get()), QString());

  QVERIFY(GitRepo::addRemote(_repo.get(), QStringLiteral("upstream"),
                             QStringLiteral("https://example.invalid/up.git"),
                             QString()));
  QCOMPARE(GitRepo::getDefaultRemote(_repo.get()), QStringLiteral("upstream"));

  // The remote of the current branch's upstream wins over origin.
  git({QStringLiteral("update-ref"), QStringLiteral("refs/remotes/upstream/main"),
       QStringLiteral("HEAD")});
  git({QStringLiteral("branch"), QStringLiteral("-q"), QStringLiteral("-u"),
       QStringLiteral("upstream/main")});
  QVERIFY(GitRepo::addRemote(_repo.get(), QStringLiteral("origin"),
                             QStringLiteral("https://example.invalid/origin.git"),
                             QString()));
  QCOMPARE(GitRepo::getDefaultRemote(_repo.get()), QStringLiteral("upstream"));

  // The chosen default wins over both, until it is removed.
  QVERIFY(GitRepo::setDefaultRemote(_repo.get(), QStringLiteral("origin")));
  QCOMPARE(GitRepo::getDefaultRemote(_repo.get()), QStringLiteral("origin"));
  QVERIFY(GitRepo::removeRemote(_repo.get(), QStringLiteral("origin")));
  QCOMPARE(GitRepo::getDefaultRemote(_repo.get()), QStringLiteral("upstream"));

  // A detached HEAD has no upstream, the first remote by name is used.
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("--detach")});
  QVERIFY(GitRepo::addRemote(_repo.get(), QStringLiteral("zeta"),
                             QStringLiteral("https://example.invalid/z.git"),
                             QString()));
  QCOMPARE(GitRepo::getDefaultRemote(_repo.get()), QStringLiteral("upstream"));
  QVERIFY(GitRepo::addRemote(_repo.get(), QStringLiteral("origin"),
                             QStringLiteral("https://example.invalid/origin.git"),
                             QString()));
  QCOMPARE(GitRepo::getDefaultRemote(_repo.get()), QStringLiteral("origin"));
}

void GitRepoTest::setDefaultRemoteMissingFails() {
  bool ok = true;
  QVERIFY(!GitRepo::setDefaultRemote(_repo.get(), QStringLiteral("nope")));
  QCOMPARE(lastError(), GitError::SetDefaultRemoteFailedToLookupRemote);
  QVERIFY(!GitRepo::setDefaultRemote(_repo.get(), QString()));
  QCOMPARE(lastError(), GitError::SetDefaultRemoteFailedToLookupRemote);
  git({QStringLiteral("config"), QStringLiteral("sourcerer.defaultremote")}, &ok);
  QVERIFY(!ok);

  QVERIFY(GitRepo::addRemote(_repo.get(), QStringLiteral("origin"),
                             QStringLiteral("https://example.invalid/o.git"),
                             QString()));
  QVERIFY2(GitRepo::setDefaultRemote(_repo.get(), QStringLiteral("origin")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("config"), QStringLiteral("sourcerer.defaultremote")}),
           QStringLiteral("origin"));
}

void GitRepoTest::renameRemoteMovesRefsAndUpstream() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  const QString url = QStringLiteral("https://example.invalid/origin.git");
  const QString pushUrl = QStringLiteral("https://example.invalid/push.git");
  QVERIFY(GitRepo::addRemote(_repo.get(), QStringLiteral("origin"), url, pushUrl));
  QVERIFY(GitRepo::addRemote(_repo.get(), QStringLiteral("other"),
                             QStringLiteral("https://example.invalid/other.git"),
                             QString()));
  for (const QString &ref : {QStringLiteral("refs/remotes/origin/main"),
                             QStringLiteral("refs/remotes/origin/feature"),
                             QStringLiteral("refs/remotes/other/main")}) {
    git({QStringLiteral("update-ref"), ref, QStringLiteral("HEAD")});
  }
  git({QStringLiteral("branch"), QStringLiteral("-q"), QStringLiteral("-u"),
       QStringLiteral("origin/main")});
  git({QStringLiteral("branch"), QStringLiteral("-q"), QStringLiteral("feature"),
       QStringLiteral("origin/feature")});
  QVERIFY(GitRepo::setDefaultRemote(_repo.get(), QStringLiteral("origin")));
  QVERIFY(GitRepo::addTarget(_repo.get(), QStringLiteral("origin"),
                             QStringLiteral("backup"),
                             QStringLiteral("https://example.invalid/b.git"),
                             QStringLiteral("https://example.invalid/bp.git")));

  QVERIFY2(GitRepo::updateRemote(_repo.get(), QStringLiteral("origin"),
                                 QStringLiteral("renamed"), url, pushUrl),
           qPrintable(lastErrorMessage()));
  QCOMPARE(remoteNames(),
           (QStringList{QStringLiteral("other"), QStringLiteral("renamed")}));
  QCOMPARE(git({QStringLiteral("for-each-ref"), QStringLiteral("--format=%(refname)"),
                QStringLiteral("refs/remotes")}),
           QStringLiteral("refs/remotes/other/main\n"
                          "refs/remotes/renamed/feature\n"
                          "refs/remotes/renamed/main"));
  QCOMPARE(git({QStringLiteral("config"), QStringLiteral("remote.renamed.fetch")}),
           QStringLiteral("+refs/heads/*:refs/remotes/renamed/*"));
  QCOMPARE(git({QStringLiteral("config"), QStringLiteral("branch.main.remote")}),
           QStringLiteral("renamed"));
  QCOMPARE(git({QStringLiteral("config"), QStringLiteral("branch.feature.remote")}),
           QStringLiteral("renamed"));
  const auto [found, remoteName, remoteBranch] =
      GitRepo::getUpstream(_repo.get(), QStringLiteral("main"));
  QVERIFY(found);
  QCOMPARE(remoteName, QStringLiteral("renamed"));
  QCOMPARE(remoteBranch, QStringLiteral("main"));
  const GitRepo::RemoteInfo renamed = remote(QStringLiteral("renamed"));
  QCOMPARE(renamed.pushUrl, pushUrl);
  QCOMPARE(targets(renamed),
           QStringLiteral("Default|%1|%2;backup|https://example.invalid/b.git|"
                          "https://example.invalid/bp.git")
               .arg(url, pushUrl));
  QCOMPARE(git({QStringLiteral("config"), QStringLiteral("sourcerer.defaultremote")}),
           QStringLiteral("renamed"));

  // Onto an existing remote, a missing remote and an invalid name.
  QVERIFY(!GitRepo::updateRemote(_repo.get(), QStringLiteral("renamed"),
                                 QStringLiteral("other"), url, QString()));
  QCOMPARE(lastError(), GitError::UpdateRemoteFailedToRenameRemote);
  QVERIFY(!GitRepo::updateRemote(_repo.get(), QStringLiteral("nope"),
                                 QStringLiteral("x"), url, QString()));
  QCOMPARE(lastError(), GitError::UpdateRemoteNotFound);
  QVERIFY(!GitRepo::updateRemote(_repo.get(), QStringLiteral("renamed"),
                                 QStringLiteral("a b"), url, QString()));
  QCOMPARE(lastError(), GitError::UpdateRemoteFailedToRenameRemote);
  QCOMPARE(remoteNames(),
           (QStringList{QStringLiteral("other"), QStringLiteral("renamed")}));
}

void GitRepoTest::updateRemoteUrlsOnly() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY(GitRepo::addRemote(_repo.get(), QStringLiteral("origin"),
                             QStringLiteral("https://example.invalid/old.git"),
                             QString()));
  git({QStringLiteral("update-ref"), QStringLiteral("refs/remotes/origin/main"),
       QStringLiteral("HEAD")});
  const QString fetchUrl = QStringLiteral("https://example.invalid/new.git");
  const QString pushUrl = QStringLiteral("https://example.invalid/newpush.git");

  QVERIFY2(GitRepo::updateRemote(_repo.get(), QStringLiteral("origin"),
                                 QStringLiteral("origin"), fetchUrl, pushUrl),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("remote"), QStringLiteral("get-url"),
                QStringLiteral("origin")}),
           fetchUrl);
  QCOMPARE(git({QStringLiteral("remote"), QStringLiteral("get-url"),
                QStringLiteral("--push"), QStringLiteral("origin")}),
           pushUrl);
  QVERIFY(!revParse(QStringLiteral("refs/remotes/origin/main")).isEmpty());

  // An empty fetch URL is refused.
  QVERIFY(!GitRepo::updateRemote(_repo.get(), QStringLiteral("origin"),
                                 QStringLiteral("origin"), QString(), pushUrl));
  QCOMPARE(remote(QStringLiteral("origin")).fetchUrl, fetchUrl);

  // Without a push URL, pushes should go to the fetch URL.
  const bool cleared = GitRepo::updateRemote(
      _repo.get(), QStringLiteral("origin"), QStringLiteral("origin"), fetchUrl,
      QString());
  QVERIFY2(cleared, qPrintable(lastErrorMessage()));
  QCOMPARE(remote(QStringLiteral("origin")).pushUrl, fetchUrl);
  bool ok = true;
  git({QStringLiteral("config"), QStringLiteral("remote.origin.pushurl")}, &ok);
  QVERIFY(!ok);
  QCOMPARE(git({QStringLiteral("remote"), QStringLiteral("get-url"),
                QStringLiteral("--push"), QStringLiteral("origin")}),
           fetchUrl);

  // Renamed at the same time.
  const bool renamed = GitRepo::updateRemote(
      _repo.get(), QStringLiteral("origin"), QStringLiteral("renamed"),
      fetchUrl, QString());
  QVERIFY(renamed);
  QCOMPARE(remoteNames(), QStringList{QStringLiteral("renamed")});
  QVERIFY(!revParse(QStringLiteral("refs/remotes/renamed/main")).isEmpty());
}

void GitRepoTest::removeRemote() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY(GitRepo::addRemote(_repo.get(), QStringLiteral("origin"),
                             QStringLiteral("https://example.invalid/o.git"),
                             QString()));
  QVERIFY(GitRepo::addRemote(_repo.get(), QStringLiteral("other"),
                             QStringLiteral("https://example.invalid/x.git"),
                             QString()));
  git({QStringLiteral("update-ref"), QStringLiteral("refs/remotes/origin/main"),
       QStringLiteral("HEAD")});
  git({QStringLiteral("branch"), QStringLiteral("-q"), QStringLiteral("-u"),
       QStringLiteral("origin/main")});
  QVERIFY(GitRepo::addTarget(_repo.get(), QStringLiteral("origin"),
                             QStringLiteral("backup"),
                             QStringLiteral("https://example.invalid/b.git"),
                             QStringLiteral("https://example.invalid/bp.git")));

  QVERIFY2(GitRepo::removeRemote(_repo.get(), QStringLiteral("origin")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(remoteNames(), QStringList{QStringLiteral("other")});
  QCOMPARE(git({QStringLiteral("for-each-ref"), QStringLiteral("refs/remotes")}),
           QString());
  bool ok = true;
  git({QStringLiteral("config"), QStringLiteral("branch.main.remote")}, &ok);
  QVERIFY(!ok);
  // The targets go with it.
  git({QStringLiteral("config"), QStringLiteral("--get-regexp"),
       QStringLiteral("^remote\\.origin")},
      &ok);
  QVERIFY(!ok);
  git({QStringLiteral("config"), QStringLiteral("--get-regexp"),
       QStringLiteral("^sourcerer-target\\.")},
      &ok);
  QVERIFY(!ok);
  QVERIFY(!std::get<0>(GitRepo::getUpstream(_repo.get(), QStringLiteral("main"))));

  QVERIFY(!GitRepo::removeRemote(_repo.get(), QStringLiteral("origin")));
  QCOMPARE(lastError(), GitError::RemoveRemoteFailedToDeleteRemote);
  QVERIFY(!GitRepo::removeRemote(_repo.get(), QString()));
  QCOMPARE(lastError(), GitError::RemoveRemoteFailedToDeleteRemote);
  QCOMPARE(remoteNames(), QStringList{QStringLiteral("other")});
}

void GitRepoTest::targetsAddEditDeleteDefault() {
  const QString a = QStringLiteral("https://example.invalid/a.git");
  const QString b = QStringLiteral("https://example.invalid/b.git");
  const QString bp = QStringLiteral("https://example.invalid/bp.git");
  const QString c = QStringLiteral("https://example.invalid/c.git");
  const QString cp = QStringLiteral("https://example.invalid/cp.git");
  QVERIFY(GitRepo::addRemote(_repo.get(), QStringLiteral("origin"), a, QString()));
  const auto originTargets = [this] { return targets(remote(QStringLiteral("origin"))); };

  QVERIFY2(GitRepo::addTarget(_repo.get(), QStringLiteral("origin"),
                              QStringLiteral("backup"), b, bp),
           qPrintable(lastErrorMessage()));
  QCOMPARE(originTargets(),
           QStringLiteral("Default|%1|%1;backup|%2|%3").arg(a, b, bp));
  QCOMPARE(remote(QStringLiteral("origin")).fetchUrl, a);

  // Adding the same target again is refused, editing changes it.
  QVERIFY(!GitRepo::addTarget(_repo.get(), QStringLiteral("origin"),
                              QStringLiteral("backup"), c, cp));
  QCOMPARE(lastError(), GitError::AddTargetAlreadyExists);
  QCOMPARE(originTargets(),
           QStringLiteral("Default|%1|%1;backup|%2|%3").arg(a, b, bp));
  QVERIFY(GitRepo::editTarget(_repo.get(), QStringLiteral("origin"),
                              QStringLiteral("backup"), c, cp));
  QCOMPARE(originTargets(),
           QStringLiteral("Default|%1|%1;backup|%2|%3").arg(a, c, cp));
  QVERIFY(!GitRepo::editTarget(_repo.get(), QStringLiteral("origin"),
                               QStringLiteral("missing"), c, cp));
  QCOMPARE(lastError(), GitError::EditTargetNotFound);
  // The default target, empty names and URLs are refused.
  QVERIFY(!GitRepo::addTarget(_repo.get(), QStringLiteral("origin"),
                              QStringLiteral("Default"), b, bp));
  QVERIFY(!GitRepo::addTarget(_repo.get(), QStringLiteral("origin"), QString(),
                              b, bp));
  QVERIFY(!GitRepo::addTarget(_repo.get(), QStringLiteral("origin"),
                              QStringLiteral("x"), QString(), bp));
  QVERIFY(!GitRepo::addTarget(_repo.get(), QString(), QStringLiteral("x"), b, bp));
  const bool orphan = GitRepo::addTarget(_repo.get(), QStringLiteral("nope"),
                                         QStringLiteral("x"), b, bp);
  QVERIFY(!orphan);
  QCOMPARE(lastError(), GitError::AddTargetRemoteNotFound);
  QVERIFY(!GitRepo::editTarget(_repo.get(), QStringLiteral("nope"),
                               QStringLiteral("x"), b, bp));
  QCOMPARE(lastError(), GitError::AddTargetRemoteNotFound);
  bool ok = true;
  git({QStringLiteral("config"), QStringLiteral("--get-regexp"),
       QStringLiteral("nope")},
      &ok);
  QVERIFY(!ok);

  QVERIFY2(GitRepo::editTarget(_repo.get(), QStringLiteral("origin"),
                               QStringLiteral("backup"), b, bp),
           qPrintable(lastErrorMessage()));
  QCOMPARE(originTargets(),
           QStringLiteral("Default|%1|%1;backup|%2|%3").arg(a, b, bp));
  QVERIFY(!GitRepo::editTarget(_repo.get(), QStringLiteral("origin"),
                               QStringLiteral("Default"), b, bp));
  QVERIFY(!GitRepo::editTarget(_repo.get(), QStringLiteral("origin"),
                               QStringLiteral("backup"), QString(), bp));

  // Switching the default target changes the remote's URLs.
  QVERIFY2(GitRepo::setDefaultTarget(_repo.get(), QStringLiteral("origin"),
                                     QStringLiteral("backup")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("remote"), QStringLiteral("get-url"),
                QStringLiteral("origin")}),
           b);
  QCOMPARE(git({QStringLiteral("remote"), QStringLiteral("get-url"),
                QStringLiteral("--push"), QStringLiteral("origin")}),
           bp);
  QCOMPARE(remote(QStringLiteral("origin")).fetchUrl, b);
  // The target now holds the previous default.
  QCOMPARE(originTargets(),
           QStringLiteral("Default|%1|%2;backup|%3|%3").arg(b, bp, a));
  QVERIFY(GitRepo::setDefaultTarget(_repo.get(), QStringLiteral("origin"),
                                    QStringLiteral("Default")));
  QVERIFY(!GitRepo::setDefaultTarget(_repo.get(), QStringLiteral("origin"),
                                     QStringLiteral("missing")));
  QCOMPARE(lastError(), GitError::EditTargetNotFound);
  QVERIFY(!GitRepo::setDefaultTarget(_repo.get(), QStringLiteral("nope"),
                                     QStringLiteral("backup")));
  QCOMPARE(remote(QStringLiteral("origin")).fetchUrl, b);

  // Deleting, a target that is gone already is not found.
  QVERIFY2(GitRepo::deleteTarget(_repo.get(), QStringLiteral("origin"),
                                 QStringLiteral("backup")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(remote(QStringLiteral("origin")).targets.size(), 1);
  QVERIFY(!GitRepo::deleteTarget(_repo.get(), QStringLiteral("origin"),
                                 QStringLiteral("backup")));
  QCOMPARE(lastError(), GitError::DeleteTargetNotFound);
  QVERIFY(!GitRepo::deleteTarget(_repo.get(), QStringLiteral("origin"),
                                 QStringLiteral("Default")));
  QCOMPARE(remoteNames(), QStringList{QStringLiteral("origin")});
}

void GitRepoTest::legacyTargetsAreMigrated() {
  const QString url = QStringLiteral("https://example.invalid/o.git");
  const QString backup = QStringLiteral("https://example.invalid/b.git");
  const QString backupPush = QStringLiteral("https://example.invalid/bp.git");
  git({QStringLiteral("remote"), QStringLiteral("add"), QStringLiteral("origin"),
       url});
  // Written like older versions did.
  git({QStringLiteral("config"), QStringLiteral("remote.origin.target.backup.fetchUrl"),
       backup});
  git({QStringLiteral("config"), QStringLiteral("remote.origin.target.backup.pushUrl"),
       backupPush});
  git({QStringLiteral("config"), QStringLiteral("remote.origin.target.Default.fetchUrl"),
       url});
  QVERIFY(git({QStringLiteral("remote")}).contains(QStringLiteral("origin.target")));

  QCOMPARE(remoteNames(), QStringList{QStringLiteral("origin")});
  QCOMPARE(targets(remote(QStringLiteral("origin"))),
           QStringLiteral("Default|%1|%1;backup|%2|%3").arg(url, backup, backupPush));
  // git no longer sees the targets as remotes.
  QCOMPARE(git({QStringLiteral("remote")}), QStringLiteral("origin"));
  QCOMPARE(git({QStringLiteral("config"),
                QStringLiteral("sourcerer-target.origin:backup.pushUrl")}),
           backupPush);
  QVERIFY(GitRepo::setDefaultTarget(_repo.get(), QStringLiteral("origin"),
                                    QStringLiteral("backup")));
  QCOMPARE(git({QStringLiteral("remote"), QStringLiteral("get-url"),
                QStringLiteral("origin")}),
           backup);
}

void GitRepoTest::upstreamSetAndGet() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY(GitRepo::addRemote(_repo.get(), QStringLiteral("origin"),
                             QStringLiteral("https://example.invalid/o.git"),
                             QString()));
  git({QStringLiteral("update-ref"), QStringLiteral("refs/remotes/origin/main"),
       QStringLiteral("HEAD")});
  const auto upstream = [this](const QString &branch) {
    const auto [found, remoteName, remoteBranch] =
        GitRepo::getUpstream(_repo.get(), branch);
    return QStringLiteral("%1|%2|%3")
        .arg(found ? QStringLiteral("yes") : QStringLiteral("no"), remoteName,
             remoteBranch);
  };

  // An existing remote branch.
  QVERIFY2(GitRepo::setUpstream(_repo.get(), QStringLiteral("origin"),
                                QStringLiteral("main"), QStringLiteral("main")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("rev-parse"), QStringLiteral("--abbrev-ref"),
                QStringLiteral("main@{upstream}")}),
           QStringLiteral("origin/main"));
  QCOMPARE(upstream(QStringLiteral("main")), QStringLiteral("yes|origin|main"));

  // A remote branch that doesn't exist yet goes into the configuration.
  git({QStringLiteral("branch"), QStringLiteral("feature/x")});
  QVERIFY2(GitRepo::setUpstream(_repo.get(), QStringLiteral("origin"),
                                QStringLiteral("feature/x"),
                                QStringLiteral("feature/x")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("config"), QStringLiteral("branch.feature/x.remote")}),
           QStringLiteral("origin"));
  QCOMPARE(git({QStringLiteral("config"), QStringLiteral("branch.feature/x.merge")}),
           QStringLiteral("refs/heads/feature/x"));
  QCOMPARE(upstream(QStringLiteral("feature/x")),
           QStringLiteral("yes|origin|feature/x"));

  // Set again, the entries are replaced.
  QVERIFY(GitRepo::setUpstream(_repo.get(), QStringLiteral("origin"),
                               QStringLiteral("renamed/x"),
                               QStringLiteral("feature/x")));
  QCOMPARE(git({QStringLiteral("config"), QStringLiteral("--get-all"),
                QStringLiteral("branch.feature/x.merge")}),
           QStringLiteral("refs/heads/renamed/x"));
  QCOMPARE(git({QStringLiteral("config"), QStringLiteral("--get-all"),
                QStringLiteral("branch.feature/x.remote")}),
           QStringLiteral("origin"));
  // Once the remote branch exists it is resolved by libgit2.
  git({QStringLiteral("update-ref"), QStringLiteral("refs/remotes/origin/renamed/x"),
       QStringLiteral("HEAD")});
  QCOMPARE(upstream(QStringLiteral("feature/x")),
           QStringLiteral("yes|origin|renamed/x"));

  // No upstream, and no branch.
  git({QStringLiteral("branch"), QStringLiteral("plain")});
  QCOMPARE(upstream(QStringLiteral("plain")), QStringLiteral("no||"));
  QVERIFY(!GitRepo::setUpstream(_repo.get(), QStringLiteral("origin"),
                                QStringLiteral("main"), QStringLiteral("nope")));
  QCOMPARE(lastError(), GitError::SetUpstreamFailedToLookupBranch);
  QCOMPARE(upstream(QStringLiteral("nope")), QStringLiteral("no||"));
  QCOMPARE(lastError(), GitError::GetUpstreamFailedToLookupBranch);

  // Like git, the remote has to exist.
  QVERIFY(!GitRepo::setUpstream(_repo.get(), QStringLiteral("nope"),
                                QStringLiteral("main"), QStringLiteral("plain")));
  QCOMPARE(lastError(), GitError::SetUpstreamRemoteNotFound);
  QCOMPARE(upstream(QStringLiteral("plain")), QStringLiteral("no||"));
}

void GitRepoTest::upstreamAdversarialConfig() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  git({QStringLiteral("remote"), QStringLiteral("add"), QStringLiteral("other"),
       QStringLiteral("https://example.invalid/other.git")});
  for (const QString &branch : {QStringLiteral("weird"), QStringLiteral("Quoted"),
                                QStringLiteral("a/b"), QStringLiteral("inc")}) {
    git({QStringLiteral("branch"), branch});
  }
  QByteArray config = readFile(file(".git/config"));
  config += "# A comment\n"
            "[branch \"weird\"]\n"
            "\tremote=origin\n"
            "\tmerge=refs/heads/weird\n"
            "[branch \"Quoted\"]\r\n"
            "    remote = \"origin\" ; a comment\r\n"
            "    merge = refs/heads/Quoted # another one\r\n"
            "[branch \"a/b\"]\n"
            "\tremote = origin\n"
            "\tmerge = refs/heads/a/b\n"
            "[include]\n"
            "\tpath = extra.config\n";
  QVERIFY(writeFile(file(".git/config"), config));
  QVERIFY(writeFile(file(".git/extra.config"),
                    "[branch \"inc\"]\n\tremote = origin\n\tmerge = refs/heads/inc\n"));
  reopen();

  for (const QString &branch : {QStringLiteral("weird"), QStringLiteral("Quoted"),
                                QStringLiteral("a/b")}) {
    const auto [found, remoteName, remoteBranch] =
        GitRepo::getUpstream(_repo.get(), branch);
    QVERIFY2(found, qPrintable(branch));
    QCOMPARE(remoteName, QStringLiteral("origin"));
    QCOMPARE(remoteBranch, branch);
  }
  QCOMPARE(git({QStringLiteral("config"), QStringLiteral("branch.Quoted.remote")}),
           QStringLiteral("origin"));
  // git finds the included file next to .git/config.
  QCOMPARE(git({QStringLiteral("config"), QStringLiteral("branch.inc.remote")}),
           QStringLiteral("origin"));
  const bool included =
      std::get<0>(GitRepo::getUpstream(_repo.get(), QStringLiteral("inc")));
  QVERIFY(included);

  // setUpstream edits the file by hand when the remote branch is missing.
  QVERIFY(GitRepo::setUpstream(_repo.get(), QStringLiteral("other"),
                               QStringLiteral("weird"), QStringLiteral("weird")));
  bool ok = false;
  git({QStringLiteral("config"), QStringLiteral("--list")}, &ok);
  QVERIFY(ok);
  QCOMPARE(git({QStringLiteral("config"), QStringLiteral("--get-all"),
                QStringLiteral("branch.weird.merge")}),
           QStringLiteral("refs/heads/weird"));
  QCOMPARE(git({QStringLiteral("config"), QStringLiteral("--get-all"),
                QStringLiteral("branch.weird.remote")}),
           QStringLiteral("other"));
  QVERIFY(GitRepo::setUpstream(_repo.get(), QStringLiteral("other"),
                               QStringLiteral("Quoted"), QStringLiteral("Quoted")));
  QCOMPARE(git({QStringLiteral("config"), QStringLiteral("--get-all"),
                QStringLiteral("branch.Quoted.remote")}),
           QStringLiteral("other"));
  QVERIFY(readFile(file(".git/config")).contains("# A comment"));
}

void GitRepoTest::upstreamFromLinkedWorktree() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  QVERIFY(GitRepo::addRemote(_repo.get(), QStringLiteral("origin"),
                             QStringLiteral("https://example.invalid/o.git"),
                             QString()));
  const QString path = _base + QStringLiteral("/wt");
  git({QStringLiteral("worktree"), QStringLiteral("add"), QStringLiteral("-q"),
       QStringLiteral("-b"), QStringLiteral("wtbranch"), path});
  RepoPtr worktree(GitRepo::openRepository(path, kAuthor, kEmail));
  QVERIFY(worktree);

  // The remote branch doesn't exist, so the configuration is edited by hand.
  QVERIFY(GitRepo::setUpstream(worktree.get(), QStringLiteral("origin"),
                               QStringLiteral("wtbranch"),
                               QStringLiteral("wtbranch")));
  bool ok = false;
  const QString remoteName =
      git({QStringLiteral("config"), QStringLiteral("branch.wtbranch.remote")}, &ok);
  QCOMPARE(remoteName, QStringLiteral("origin"));

  git({QStringLiteral("config"), QStringLiteral("branch.wtbranch.remote"),
       QStringLiteral("origin")});
  git({QStringLiteral("config"), QStringLiteral("branch.wtbranch.merge"),
       QStringLiteral("refs/heads/wtbranch")});
  QVERIFY(std::get<0>(GitRepo::getUpstream(_repo.get(), QStringLiteral("wtbranch"))));
  const bool found =
      std::get<0>(GitRepo::getUpstream(worktree.get(), QStringLiteral("wtbranch")));
  QVERIFY(found);
}

void GitRepoTest::configEditsPreserveComments() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Base"));
  QByteArray config = readFile(file(".git/config"));
  config += "# keep this comment\n"
            "[custom \"section\"]\n"
            "\tkey = value ; inline comment\n";
  QVERIFY(writeFile(file(".git/config"), config));
  reopen();

  const QString url = QStringLiteral("https://example.invalid/o.git");
  QVERIFY(GitRepo::addRemote(_repo.get(), QStringLiteral("origin"), url, QString()));
  QVERIFY(GitRepo::addTarget(_repo.get(), QStringLiteral("origin"),
                             QStringLiteral("backup"),
                             QStringLiteral("https://example.invalid/b.git"),
                             QString()));
  QVERIFY(GitRepo::editTarget(_repo.get(), QStringLiteral("origin"),
                              QStringLiteral("backup"),
                              QStringLiteral("https://example.invalid/c.git"),
                              QStringLiteral("https://example.invalid/cp.git")));
  QVERIFY(GitRepo::setDefaultRemote(_repo.get(), QStringLiteral("origin")));
  git({QStringLiteral("update-ref"), QStringLiteral("refs/remotes/origin/main"),
       QStringLiteral("HEAD")});
  QVERIFY(GitRepo::setUpstream(_repo.get(), QStringLiteral("origin"),
                               QStringLiteral("main"), QStringLiteral("main")));
  git({QStringLiteral("branch"), QStringLiteral("feature")});
  QVERIFY(GitRepo::setUpstream(_repo.get(), QStringLiteral("origin"),
                               QStringLiteral("feature"),
                               QStringLiteral("feature")));
  QVERIFY(GitRepo::updateRemote(_repo.get(), QStringLiteral("origin"),
                                QStringLiteral("renamed"), url, QString()));
  QVERIFY(GitRepo::setDefaultTarget(_repo.get(), QStringLiteral("renamed"),
                                    QStringLiteral("backup")));
  QVERIFY(GitRepo::deleteTarget(_repo.get(), QStringLiteral("renamed"),
                                QStringLiteral("backup")));
  QVERIFY(GitRepo::removeRemote(_repo.get(), QStringLiteral("renamed")));

  QVERIFY(readFile(file(".git/config")).contains("# keep this comment"));
  QCOMPARE(git({QStringLiteral("config"), QStringLiteral("custom.section.key")}),
           QStringLiteral("value"));
  bool ok = false;
  git({QStringLiteral("config"), QStringLiteral("--list")}, &ok);
  QVERIFY(ok);
}

// Test hooks.

void GitRepoTest::commercialPlanSwitch() {
  const auto restore = qScopeGuard(
      [] { GitRepo::Testing::setCommercialPlanOverride(-1); });
  const QString url = QStringLiteral("git@example.invalid:team/repo.git");
  const QString destination = _base + QStringLiteral("/clone");

  // Without the plan, git@ URLs are refused before connecting.
  GitRepo::Testing::setCommercialPlanOverride(0);
  QVERIFY(!GitRepo::cloneRepository(url, destination, QStringLiteral("uuid")));
  QCOMPARE(lastError(), GitError::CloneRepositoryGitSshUrlRequiresCommercialPlan);
  QVERIFY(!QFileInfo::exists(destination));

  // With it the clone is attempted (and fails on the invalid host).
  GitRepo::Testing::setCommercialPlanOverride(1);
  QVERIFY(!GitRepo::cloneRepository(url, destination, QStringLiteral("uuid")));
  QCOMPARE(lastError(), GitError::CloneRepositoryFailedToCloneRepository);
  QVERIFY(!QFileInfo::exists(destination));
}

// Existing scenarios.

void GitRepoTest::defaultRemote() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  gitCommitAll(QStringLiteral("Root"));
  git({QStringLiteral("remote"), QStringLiteral("add"),
       QStringLiteral("origin"),
       QStringLiteral("https://example.invalid/origin.git")});
  git({QStringLiteral("remote"), QStringLiteral("add"),
       QStringLiteral("upstream"),
       QStringLiteral("https://example.invalid/upstream.git")});

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
  gitCommitAll(QStringLiteral("Base"));

  // feature: a conflicting step, then one that applies cleanly.
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("feature")});
  QVERIFY(writeFile(file("a.txt"), "feature\n"));
  gitCommitAll(QStringLiteral("Feature change"));
  QVERIFY(writeFile(file("b.txt"), "b\n"));
  gitCommitAll(QStringLiteral("Feature file"));

  git({QStringLiteral("checkout"), QStringLiteral("-q"),
       QStringLiteral("main")});
  QVERIFY(writeFile(file("a.txt"), "main\n"));
  const QString mainTip = gitCommitAll(QStringLiteral("Main change"));

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
           qPrintable(lastErrorMessage()));

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
