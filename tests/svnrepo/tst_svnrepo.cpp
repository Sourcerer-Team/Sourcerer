// MIT License
// Copyright (c) 2026 Sourcerer

// End to end tests of SvnRepo against local file:// repositories. Every test
// gets its own repository and working copy (init/cleanup). See
// docs/SVN_TEST_PLAN.md; QEXPECT_FAIL markers name the suspected defect (§7)
// that makes the check fail today.

#include "SvnTestHelpers.h"

#include <SvnRepo.h>

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

using namespace SvnTest;

class SvnRepoTest : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void init();
  void cleanup();
  void cleanupTestCase();

  // The fixtures themselves (SVN_TEST_PLAN 2.3, 2.4).
  void fixtureRepositories();

  void initCreatesStandardLayout();
  void stageUnstageAndCommit();
  void unversionedDirectories();
  void unstageModificationKeepsIt();
  void diffAndDiscard();
  void historyAndCommitQueries();
  void blameAttributesLines();
  void branchesSwitchAndMerge();
  void tags();
  void cherryPickAndRevert();
  void patches();
  void conflictedUpdateIsNotAbortable();
  void stashes();
  void externals();
  void remotes();
  void gitOnlyOperationsAreUnsupported();

  // 3.1 Repository lifecycle.
  void initIgnoresDefaultBranch_data();
  void initIgnoresDefaultBranch();
  void initAuthorIsRecorded();
  void initRefusesExistingWorkingCopy();
  void initIntoGitRepository();
  void initUnusualRepoNames_data();
  void initUnusualRepoNames();
  void initIntoUnusualPaths_data();
  void initIntoUnusualPaths();
  void openRejectsNonWorkingCopy_data();
  void openRejectsNonWorkingCopy();
  void openFromSubdirectory();
  void openPathSpellings_data();
  void openPathSpellings();
  void openWorkingCopiesOfBothVersions();
  void openLockedWorkingCopy();
  void closeRepositoryOfOtherPath();
  void cloneUrlSpellings_data();
  void cloneUrlSpellings();
  void cloneIntoExistingDestinations_data();
  void cloneIntoExistingDestinations();
  void cloneFailureCleansUp();
  void cloneRepositoryRoot();
  void lastErrorIsPerThread();
  void concurrentCallsOnTwoThreads();
  void successfulCallDoesNotResetTheError();

  // 3.2 Status.
  void statusCategories();
  void statusIgnoredFilesAreReported();
  void statusDirectoryPropertyChanges();
  void statusUnversionedDirectories();
  void statusExternals();
  void statusUserConfigGlobalIgnores();
  void statusUnusualFileNames_data();
  void statusUnusualFileNames();
  void statusMixedLineEndings_data();
  void statusMixedLineEndings();
  void statusKeywords();
  void statusNeedsLock();
  void statusOtherChangelist();
  void statusSorted();
  void statusCancelled();
  void statusManyFiles();
  void statusFailureIsNotClean();

  // 3.3 Staging.
  void unstagedChangesAreNeverCommitted_data();
  void unstagedChangesAreNeverCommitted();
  void stageUnstageRoundTrip_data();
  void stageUnstageRoundTrip();
  void stageModifiedDeletedMissing();
  void stageDirectory();
  void stageEmptyAddedDirectory();
  void stageCopiedDirectory();
  void stageInsideUnversionedParents();
  void stageIgnoredFile();
  void stageConflictedFile_data();
  void stageConflictedFile();
  void stageMovesFileFromUserChangelist();
  void stageNothing();
  void stageNonexistentAndOutsidePaths_data();
  void stageNonexistentAndOutsidePaths();
  void stageFileInsideIgnoredDirectory();
  void unstageKeepsAddAndDelete();
  void unstageUnversionedOrUnstagedFile();

  // 3.4 Commit and sign-off.
  void commitStagedOnly();
  void commitStagedDeletedDirectory();
  void commitStagedReplacedDirectory();
  void commitStagedAddedTree();
  void commitStagedMoveBothSides();
  void commitStagedMoveOneSide();
  void commitWithoutStagedFiles();
  void commitStagedFileRevertedExternally();
  void commitMessageComposition_data();
  void commitMessageComposition();
  void commitSignoff();
  void commitAuthor();
  void commitOutOfDate();
  void commitNeedsLockAndForeignLock();
  void commitDoesNotIncludeUnstagedPropertyChanges();
  void commitSignCommitIsReported();

  // 3.5 Discard, ignore, checkout, patch.
  void discardCategories_data();
  void discardCategories();
  void discardStagedChanges();
  void discardDeletedChildOfDeletedDirectory();
  void discardOneSideOfMove();
  void discardTreeConflict();
  void discardRootAndOutsidePaths_data();
  void discardRootAndOutsidePaths();
  void discardPartialFailureLeavesConsistentState();
  void ignoreFileNonRecursive();
  void ignoreRecursive_data();
  void ignoreRecursive();
  void ignoreGlobCharacters_data();
  void ignoreGlobCharacters();
  void ignoreExistingPropertyValues();
  void ignoreInsideUnversionedTree();
  void ignoreOutsideWorkingCopyTerminates();
  void ignoreVersionedFile();
  void checkoutCommit_data();
  void checkoutCommit();
  void checkoutCommitWithConflictingLocalEdit();
  void patchRoundTrips_data();
  void patchRoundTrips();
  void patchSkippedTargetFails();
  void patchRejectedHunks();
  void patchAlreadyAppliedEmptyGarbage_data();
  void patchAlreadyAppliedEmptyGarbage();
  void patchCrlfEmptyContextLine();
  void patchNonUtf8Content();

  // 3.6 Conflicts, merge state, tree conflicts.
  void conflictKinds_data();
  void conflictKinds();
  void listConflictsForNonConflictedFile();
  void abortMergeAfterConflictedOperation_data();
  void abortMergeAfterConflictedOperation();
  void abortMergeWithoutOperationFails_data();
  void abortMergeWithoutOperationFails();
  void abortMergeTwiceFails();
  void mergeInProgressState();
  void mergeInProgressFromCli();
  void resolveTreeConflict_data();
  void resolveTreeConflict();

  // 3.7 Diff, blame, file size.
  void diffCategories_data();
  void diffCategories();
  void diffUnusualFileNames_data();
  void diffUnusualFileNames();
  void diffOutsideWorkingCopy();
  void diffTextIsAValidPatch();
  void diffFromCommit_data();
  void diffFromCommit();
  void diffFromCommitAbsolutePaths();
  void createPatchFromCommitLayouts_data();
  void createPatchFromCommitLayouts();
  void blameCases_data();
  void blameCases();
  void blameLineCountMatchesFile_data();
  void blameLineCountMatchesFile();
  void fileSizeFromCommit_data();
  void fileSizeFromCommit();

  // 3.8 History.
  void historyLinearParents();
  void historyFollowsCopies();
  void historyPaging_data();
  void historyPaging();
  void historyRevpropEdgeCases();
  void historyTimestamps();
  void historyAfterBranchDeletedOrRenamed_data();
  void historyAfterBranchDeletedOrRenamed();
  void historyOtherBranchesOnly();
  void historyCancelled();
  void historyProjectRootLayout();
  void historyRepoWithSpaceInPath();
  void commitStatusFileLists_data();
  void commitStatusFileLists();
  void commitStatusOfInvalidRevision_data();
  void commitStatusOfInvalidRevision();
  void commitBranchesTagsHeads();
  void commitIsHeadAfterOwnCommit();

  // 3.9 Branches.
  void listBranchesLayouts_data();
  void listBranchesLayouts();
  void branchNames_data();
  void branchNames();
  void createBranchFromCommitRevisions_data();
  void createBranchFromCommitRevisions();
  void branchesFromSubdirectoryWc();
  void checkoutBranchCases();
  void deleteBranchGuards_data();
  void deleteBranchGuards();
  void renameActiveBranch();
  void renameOntoExistingBranch();
  void renameTrunk();
  void remoteBranchesWithStatus();

  // 3.10 Tags.
  void tagLifecycle_data();
  void tagLifecycle();
  void deleteTagGuards();
  void tagFromTagWc();
  void tagMessage();
  void pushTagAndPushAllTags();

  // 3.11 Merge, cherry-pick, revert.
  void mergeCreatesMergeRevision();
  void mergeWithLocalChangeInTheWayFails();
  void mergeAlreadyUpToDate();
  void mergeIntoItself();
  void mergeIntoOtherBranchChecksItOut();
  void mergeFromSubdirectoryWc();
  void cherryPickCases_data();
  void cherryPickCases();
  void cherryPickWithLocalChangeInTheWayFails();
  void twoBranchRevision();
  void revertCommitCases_data();
  void revertCommitCases();
  void revertCommitOfAddedDirectory();
  void operationStateSurvivesReopen();
  void authorOfPickedRevision();

  // 3.12 Stash (shelves).
  void stashCategories_data();
  void stashCategories();
  void stashKeepsStagingOrDocumentsLoss();
  void stashTwiceInARow();
  void stashOrdering();
  void applyStashCases_data();
  void applyStashCases();
  void stashIndexes_data();
  void stashIndexes();
  void stashStatus();
  void stashDiffEolNative();
  void stashCliInterop();
  void stashNote_data();
  void stashNote();

  // 3.13 Remotes and synchronisation.
  void updateRemoteRelocates_data();
  void updateRemoteRelocates();
  void remoteUnsupportedOperations_data();
  void remoteUnsupportedOperations();
  void getUpstreamUnknownBranch_data();
  void getUpstreamUnknownBranch();
  void pullUpdatesAndReportsConflicts();
  void pullMixedRevisionWc();
  void pullRebaseFlag();
  void pushWithStagedWork();
  void fetchAllAndFetchRemote();

  // 3.14 Externals (submodules).
  void addSubmoduleCases_data();
  void addSubmoduleCases();
  void addSubmoduleDoesNotUpdateWholeWc();
  void addSubmoduleCommitted();
  void deleteSubmoduleKeepsOthers();
  void deleteSubmodulePathContainment_data();
  void deleteSubmodulePathContainment();
  void submoduleStatus();
  void fileExternals();

  // 3.15 Git-only operations.
  void gitOnlyOperationsTable_data();
  void gitOnlyOperationsTable();
  void workflowTypeIsConsistent();
  void readGitFlowNextConfigSetsError();

private:
  QString file(const QString &name) const { return _wc + "/" + name; }
  SvnRepo::SvnStatus status() const { return SvnRepo::getStatus(_wc, _quit); }
  QString head() const { return SvnTest::head(_repoDir); }
  // Commits a.txt with `content` through the API; returns the new revision.
  QString commitA(const QByteArray &content, const QString &summary);

  std::unique_ptr<QTemporaryDir> _tmp;
  QString _base;
  QString _wc;
  QString _repoDir;
  QString _rootUrl;
  bool _quit = false;
};

QString SvnRepoTest::commitA(const QByteArray &content,
                             const QString &summary) {
  if (!writeFile(file("a.txt"), content) || !commitAll(_wc, summary)) {
    return QString();
  }
  return head();
}

void SvnRepoTest::initTestCase() {
  if (!toolsAvailable()) {
    QSKIP("svn, svnadmin or svnlook was not found");
  }
  QStandardPaths::setTestModeEnabled(true);
  QVERIFY(SvnRepo::init());
  SvnRepo::initForCurrentThread();
  SvnRepo::Testing::installMalfunctionHandler();
  useTestCredentialsKey();
}

void SvnRepoTest::init() {
  _tmp = std::make_unique<QTemporaryDir>();
  QVERIFY(_tmp->isValid());
  _base = QDir::cleanPath(QDir::fromNativeSeparators(_tmp->path()));
  setConfigDir(_base + QStringLiteral("/svnconfig"));

  _wc = _base + QStringLiteral("/wc");
  _repoDir = _base + QStringLiteral("/repo-svnrepo");
  QVERIFY2(SvnRepo::initRepository(QStringLiteral("repo"), _wc,
                                   QStringLiteral("main"), kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QVERIFY2(!SvnRepo::openRepository(_wc).isEmpty(),
           qPrintable(lastErrorMessage()));

  const QVector<SvnRepo::RemoteInfo> remotes = SvnRepo::listRemotes(_wc);
  QCOMPARE(remotes.size(), 1);
  _rootUrl = remotes.first().fetchUrl;
}

void SvnRepoTest::cleanup() {
  SvnRepo::closeRepository(_wc);
  SvnRepo::clearCachedCredentials(_wc);
  if (_tmp) {
    makeWritable(_tmp->path());
  }
  _tmp.reset();
  _quit = false;

  const QStringList malfunctions = SvnRepo::Testing::takeMalfunctions();
  QVERIFY2(malfunctions.isEmpty(), qPrintable(malfunctions.join('\n')));
}

void SvnRepoTest::cleanupTestCase() {
  SvnRepo::Testing::setConfigDirectory(QString());
}

void SvnRepoTest::fixtureRepositories() {
  // Revision properties the library can't write.
  const QString edges = _base + QStringLiteral("/edges");
  QVERIFY(!loadDump(edges, Dumps::revpropEdgeCases()).isEmpty());
  QCOMPARE(SvnTest::head(edges), QStringLiteral("6"));
  bool found = true;
  svnlookRevprop(edges, QStringLiteral("2"), QStringLiteral("svn:author"),
                 &found);
  QVERIFY(!found);
  QCOMPARE(svnlookRevprop(edges, QStringLiteral("2"), QStringLiteral("svn:log")),
           QByteArray("No author"));
  svnlookRevprop(edges, QStringLiteral("3"), QStringLiteral("svn:date"),
                 &found);
  QVERIFY(!found);
  QCOMPARE(svnlookRevprop(edges, QStringLiteral("4"), QStringLiteral("svn:log"),
                          &found),
           QByteArray());
  QVERIFY(found);
  QCOMPARE(svnlookRevprop(edges, QStringLiteral("5"), QStringLiteral("svn:log")),
           QByteArray("Summary\r\n\r\nBody\r\n"));
  QCOMPARE(svnlookRevprop(edges, QStringLiteral("6"), QStringLiteral("svn:log")),
           QByteArray("Caf\xe9 \xff"));
  QCOMPARE(svnlookAuthor(edges, QStringLiteral("1")), kAuthor);

  // Other layouts.
  const QStringList standard{QStringLiteral("branches/"),
                             QStringLiteral("tags/"), QStringLiteral("trunk/")};
  const QString project = loadDump(_base + QStringLiteral("/project"),
                                   Dumps::projectRootLayout());
  QVERIFY(!project.isEmpty());
  QCOMPARE(svn({QStringLiteral("ls"), project + QStringLiteral("/proj")})
               .split('\n'),
           standard);
  const QString flat =
      loadDump(_base + QStringLiteral("/flat"), Dumps::noTrunkLayout());
  QVERIFY(!flat.isEmpty());
  QCOMPARE(svn({QStringLiteral("ls"), flat}), QStringLiteral("src/"));

  // Revisions that touch two branches, only a tag, a replaced node, a move.
  const QString branches = _base + QStringLiteral("/branches");
  QVERIFY(!loadDump(branches, Dumps::branchesAndTags()).isEmpty());
  QCOMPARE(changedPaths(branches, QStringLiteral("4")),
           (QStringList{QStringLiteral("U branches/b1/a.txt"),
                        QStringLiteral("U branches/b2/a.txt")}));
  QCOMPARE(changedPaths(branches, QStringLiteral("5")),
           QStringList{QStringLiteral("A tags/v1")});
  const QString moved = _base + QStringLiteral("/moved");
  QVERIFY(!loadDump(moved, Dumps::replacedAndMoved()).isEmpty());
  QCOMPARE(changedPaths(moved, QStringLiteral("2")),
           QStringList{QStringLiteral("R trunk/a.txt")});
  QCOMPARE(changedPaths(moved, QStringLiteral("3")),
           (QStringList{QStringLiteral("A trunk/moved"),
                        QStringLiteral("D trunk/dir")}));

  // makeRepo layouts.
  const QString made =
      makeRepo(_base + QStringLiteral("/made"), Layout::Standard);
  QVERIFY(!made.isEmpty());
  QCOMPARE(svn({QStringLiteral("ls"), made}).split('\n'), standard);
  const QString madeProject =
      makeRepo(_base + QStringLiteral("/madeProject"), Layout::ProjectRoot);
  QCOMPARE(svn({QStringLiteral("ls"), madeProject + QStringLiteral("/proj")})
               .split('\n'),
           standard);
  const QString madeFlat =
      makeRepo(_base + QStringLiteral("/madeFlat"), Layout::Flat);
  QVERIFY(!madeFlat.isEmpty());
  QCOMPARE(SvnTest::head(_base + QStringLiteral("/madeFlat")),
           QStringLiteral("0"));

  // Revision property changes need a pre-revprop-change hook.
  const QString madeDir = _base + QStringLiteral("/made");
  const QStringList setLog{QStringLiteral("propset"),
                           QStringLiteral("--revprop"),
                           QStringLiteral("-r"),
                           QStringLiteral("1"),
                           QStringLiteral("svn:log"),
                           QStringLiteral("Changed"),
                           made};
  bool ok = true;
  svn(setLog, &ok);
  QVERIFY(!ok);
  QVERIFY(addPreRevpropChangeHook(madeDir, false));
  svn(setLog, &ok);
  QVERIFY(!ok);
  QCOMPARE(svnlookLog(madeDir, QStringLiteral("1")), QByteArray("Layout"));
  QVERIFY(addPreRevpropChangeHook(madeDir, true));
  svn(setLog, &ok);
  QVERIFY(ok);
  QCOMPARE(svnlookLog(madeDir, QStringLiteral("1")), QByteArray("Changed"));

  // A second working copy of the fixture's repository.
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk"),
                   _base + QStringLiteral("/second")));
  QCOMPARE(svnInfo(_base + QStringLiteral("/second"), QStringLiteral("url")),
           _rootUrl + QStringLiteral("/trunk"));
}

void SvnRepoTest::initCreatesStandardLayout() {
  // The repository is created next to the working copy:
  // <parent>/<repoName>-svnrepo.
  QVERIFY(QFileInfo(_repoDir + QStringLiteral("/format")).exists());
  QCOMPARE(svnInfo(_wc, QStringLiteral("repos-root-url")), _rootUrl);
  QCOMPARE(svnInfo(_wc, QStringLiteral("url")),
           _rootUrl + QStringLiteral("/trunk"));
  QCOMPARE(svn({QStringLiteral("ls"), _rootUrl}).split('\n'),
           (QStringList{QStringLiteral("branches/"), QStringLiteral("tags/"),
                        QStringLiteral("trunk/")}));
  QCOMPARE(head(), QStringLiteral("1"));

  QCOMPARE(SvnRepo::getActiveBranch(_wc, SvnRepo::BranchType::Local, _quit),
           QStringLiteral("trunk"));
  QCOMPARE(SvnRepo::listLocalBranches(_wc, _quit),
           QStringList{QStringLiteral("trunk")});
  QVERIFY(SvnRepo::listRemoteBranches(_wc, _quit).isEmpty());
  QVERIFY(SvnRepo::listTags(_wc).isEmpty());
  QVERIFY(!status().hasChanges());
  // The 1.14 command line client reads the working copy the library made.
  QVERIFY(svnStatusXml(_wc).isEmpty());
}

void SvnRepoTest::stageUnstageAndCommit() {
  QVERIFY(writeFile(file("a.txt"), "one\ntwo\nthree\n"));

  SvnRepo::SvnStatus st = status();
  QCOMPARE(st.newFiles, QStringList{QStringLiteral("a.txt")});
  QVERIFY(st.newStagedFiles.isEmpty());

  QVERIFY2(SvnRepo::stageFiles(_wc, {QStringLiteral("a.txt")}),
           qPrintable(lastErrorMessage()));
  st = status();
  QCOMPARE(st.newStagedFiles, QStringList{QStringLiteral("a.txt")});
  QVERIFY(st.newFiles.isEmpty());
  StatusMap oracle = svnStatusXml(_wc);
  QCOMPARE(oracle.value("a.txt").item, QStringLiteral("added"));
  QCOMPARE(changelistMembers(oracle), QStringList{QStringLiteral("a.txt")});

  // Unstaging only removes the selection: the file stays added.
  QVERIFY2(SvnRepo::unstageFiles(_wc, {QStringLiteral("a.txt")}),
           qPrintable(lastErrorMessage()));
  st = status();
  QCOMPARE(st.newFiles, QStringList{QStringLiteral("a.txt")});
  QVERIFY(st.newStagedFiles.isEmpty());
  oracle = svnStatusXml(_wc);
  QCOMPARE(oracle.value("a.txt").item, QStringLiteral("added"));
  QVERIFY(changelistMembers(oracle).isEmpty());
  QCOMPARE(readFile(file("a.txt")), QByteArray("one\ntwo\nthree\n"));

  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("a.txt")}));
  QVERIFY2(SvnRepo::commitStaged(_wc, QStringLiteral("Add a"),
                                 QStringLiteral("Body"), kAuthor, kEmail, true,
                                 true, false),
           qPrintable(lastErrorMessage()));
  QVERIFY(!status().hasChanges());
  QVERIFY(svnStatusXml(_wc).isEmpty());

  const QString revision = head();
  QCOMPARE(revision, QStringLiteral("2"));
  QCOMPARE(changedPaths(_repoDir, revision),
           QStringList{QStringLiteral("A trunk/a.txt")});
  const QString log = QString::fromUtf8(svnlookLog(_repoDir, revision));
  QVERIFY2(log.startsWith(QStringLiteral("Add a\n\nBody")), qPrintable(log));
  QCOMPARE(log.count(QStringLiteral("Signed-off-by: Tester "
                                    "<tester@example.com>")),
           1);

  const SvnRepo::CommitStatus commit =
      SvnRepo::getCommitStatus(_wc, revision);
  QCOMPARE(commit.message.trimmed(), log.trimmed());
  QCOMPARE(commit.newFiles, QStringList{QStringLiteral("a.txt")});
}

void SvnRepoTest::unversionedDirectories() {
  QVERIFY(writeFile(file("dir/c.txt"), "c\n"));
  QVERIFY(writeFile(file("dir/sub/b.txt"), "b\n"));

  // The files are reported instead of the directory, so they can be shown in
  // a tree.
  SvnRepo::SvnStatus st = status();
  QCOMPARE(st.newFiles, (QStringList{QStringLiteral("dir/c.txt"),
                                     QStringLiteral("dir/sub/b.txt")}));

  QVERIFY2(SvnRepo::stageFiles(_wc, {QStringLiteral("dir/sub/b.txt")}),
           qPrintable(lastErrorMessage()));
  st = status();
  QCOMPARE(st.newStagedFiles, QStringList{QStringLiteral("dir/sub/b.txt")});
  QCOMPARE(st.newFiles, QStringList{QStringLiteral("dir/c.txt")});
  QCOMPARE(changelistMembers(svnStatusXml(_wc)),
           QStringList{QStringLiteral("dir/sub/b.txt")});

  // The added parent directories are committed with the staged file; the
  // unstaged sibling is not.
  QVERIFY2(SvnRepo::commitStaged(_wc, QStringLiteral("Add b"), QString(),
                                 kAuthor, kEmail, true, false, false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(changedPaths(_repoDir, head()),
           (QStringList{QStringLiteral("A trunk/dir"),
                        QStringLiteral("A trunk/dir/sub"),
                        QStringLiteral("A trunk/dir/sub/b.txt")}));
  st = status();
  QVERIFY(st.newStagedFiles.isEmpty());
  QCOMPARE(st.newFiles, QStringList{QStringLiteral("dir/c.txt")});
  QCOMPARE(svnStatusXml(_wc).value("dir/c.txt").item,
           QStringLiteral("unversioned"));

  QVERIFY2(commitAll(_wc, QStringLiteral("Add c")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(changedPaths(_repoDir, head()),
           QStringList{QStringLiteral("A trunk/dir/c.txt")});
  QVERIFY(!status().hasChanges());
}

void SvnRepoTest::unstageModificationKeepsIt() {
  QVERIFY(!commitA("one\ntwo\nthree\n", QStringLiteral("Add a")).isEmpty());
  const QString base = head();

  // A plain edit is not staged: staging is the user's selection.
  QVERIFY(writeFile(file("a.txt"), "one\nTWO\nthree\n"));
  SvnRepo::SvnStatus st = status();
  QCOMPARE(st.modifiedFiles, QStringList{QStringLiteral("a.txt")});
  QVERIFY(st.modifiedStagedFiles.isEmpty());

  resetLastError();
  QVERIFY(!SvnRepo::commitStaged(_wc, QStringLiteral("Nothing"), QString(),
                                 kAuthor, kEmail, true, false, false));
  EXPECT_SVN_ERROR(SvnRepo::SvnError::CommitFailed);
  QCOMPARE(head(), base);

  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("a.txt")}));
  st = status();
  QCOMPARE(st.modifiedStagedFiles, QStringList{QStringLiteral("a.txt")});
  QVERIFY(st.modifiedFiles.isEmpty());
  QCOMPARE(changelistMembers(svnStatusXml(_wc)),
           QStringList{QStringLiteral("a.txt")});

  QVERIFY2(SvnRepo::unstageFiles(_wc, {QStringLiteral("a.txt")}),
           qPrintable(lastErrorMessage()));
  st = status();
  QCOMPARE(st.modifiedFiles, QStringList{QStringLiteral("a.txt")});
  QVERIFY(st.modifiedStagedFiles.isEmpty());
  const StatusMap oracle = svnStatusXml(_wc);
  QCOMPARE(oracle.value("a.txt").item, QStringLiteral("modified"));
  QVERIFY(changelistMembers(oracle).isEmpty());
  QCOMPARE(readFile(file("a.txt")), QByteArray("one\nTWO\nthree\n"));
  QCOMPARE(head(), base);

  QVERIFY(SvnRepo::discardFiles(_wc, {QStringLiteral("a.txt")}));
  QCOMPARE(readFile(file("a.txt")), QByteArray("one\ntwo\nthree\n"));
}

void SvnRepoTest::diffAndDiscard() {
  QVERIFY(!commitA("one\ntwo\nthree\n", QStringLiteral("Add a")).isEmpty());
  QVERIFY(writeFile(file("a.txt"), "one\nTWO\nthree\n"));
  QVERIFY(writeFile(file("new.txt"), "fresh\n"));
  QVERIFY(writeFile(file("staged.txt"), "staged\n"));
  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("staged.txt")}));

  const SvnRepo::FilesDiff diff = SvnRepo::diffFiles(
      _wc, {QStringLiteral("a.txt"), QStringLiteral("new.txt")});
  QVERIFY2(diff.valid, qPrintable(lastErrorMessage()));
  QCOMPARE(diff.deltas.size(), 2);

  const SvnRepo::FilesDiffDelta &modified = diff.deltas[0];
  QCOMPARE(modified.newFilePath, QStringLiteral("a.txt"));
  QCOMPARE(modified.hunks.size(), 1);
  QCOMPARE(changedLines(modified),
           (QStringList{QStringLiteral("-two"), QStringLiteral("+TWO")}));
  QCOMPARE(diff.repoContent.value(QStringLiteral("a.txt")),
           QStringLiteral("one\ntwo\nthree\n"));
  QCOMPARE(diff.filesContent.value(QStringLiteral("a.txt")),
           QStringLiteral("one\nTWO\nthree\n"));

  const SvnRepo::FilesDiffDelta &added = diff.deltas[1];
  QCOMPARE(changedLines(added), QStringList{QStringLiteral("+fresh")});
  QVERIFY(diff.diff.contains(QStringLiteral("diff --git a/a.txt b/a.txt")));

  // Discard is the Git backend's (§7.4): tracked files go back to BASE, added
  // and unversioned files stay on disk as unversioned.
  QVERIFY2(SvnRepo::discardFiles(_wc, {QStringLiteral("a.txt"),
                                       QStringLiteral("new.txt"),
                                       QStringLiteral("staged.txt")}),
           qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("a.txt")), QByteArray("one\ntwo\nthree\n"));
  QCOMPARE(readFile(file("new.txt")), QByteArray("fresh\n"));
  QCOMPARE(readFile(file("staged.txt")), QByteArray("staged\n"));

  const SvnRepo::SvnStatus st = status();
  QVERIFY(st.modifiedFiles.isEmpty());
  QVERIFY(st.newStagedFiles.isEmpty());
  QCOMPARE(st.newFiles, (QStringList{QStringLiteral("new.txt"),
                                     QStringLiteral("staged.txt")}));
  QVERIFY(changelistMembers(svnStatusXml(_wc)).isEmpty());
}

void SvnRepoTest::historyAndCommitQueries() {
  const QString first =
      commitA("one\ntwo\nthree\n", QStringLiteral("Add a"));
  const QString revision =
      commitA("one\ntwo\nthree\nfour\n", QStringLiteral("Add four"));
  QVERIFY2(!revision.isEmpty(), qPrintable(lastErrorMessage()));
  QCOMPARE(revision.toInt(), first.toInt() + 1);

  // No update after the commits: the working copy is mixed-revision, as it
  // is for the user.
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
  QVERIFY(!SvnRepo::getCommitIsHead(_wc, first, _quit));
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

  const SvnRepo::CommitStatus commit =
      SvnRepo::getCommitStatus(_wc, revision);
  QCOMPARE(commit.commit, revision);
  QCOMPARE(commit.modifiedFiles, QStringList{QStringLiteral("a.txt")});
  QCOMPARE(commit.author, svnlookAuthor(_repoDir, revision));

  resetLastError();
  QVERIFY(SvnRepo::getCommitStatus(_wc, QStringLiteral("not-a-revision"))
              .commit.isEmpty());
  EXPECT_SVN_ERROR(SvnRepo::SvnError::InvalidRevision);
}

void SvnRepoTest::blameAttributesLines() {
  const QString first =
      commitA("one\ntwo\nthree\n", QStringLiteral("Add a"));
  const QString second =
      commitA("one\ntwo\nthree\nfour\n", QStringLiteral("Add four"));
  QVERIFY(!second.isEmpty());

  const SvnRepo::BlameReport report =
      SvnRepo::blame(_wc, {QStringLiteral("a.txt")}, QString());
  QVERIFY2(report.valid, qPrintable(lastErrorMessage()));
  QCOMPARE(report.fileInfos.size(), 1);
  const QVector<SvnRepo::BlameLineInfo> &lines =
      report.fileInfos.first().lineInfos;
  QCOMPARE(lines.size(), 4);
  QCOMPARE(lines[0].lineNumber, qint64(1));
  QCOMPARE(lines[0].commit, first);
  QCOMPARE(lines[3].content, QStringLiteral("four"));
  QCOMPARE(lines[3].commit, second);
  QCOMPARE(lines[3].summary, QStringLiteral("Add four"));
}

void SvnRepoTest::branchesSwitchAndMerge() {
  QVERIFY(!commitA("one\ntwo\nthree\n", QStringLiteral("Add a")).isEmpty());

  QVERIFY2(
      SvnRepo::createBranchFromCommit(_wc, head(), QStringLiteral("feature")),
      qPrintable(lastErrorMessage()));
  QCOMPARE(svn({QStringLiteral("ls"), _rootUrl + QStringLiteral("/branches")}),
           QStringLiteral("feature/"));
  QCOMPARE(SvnRepo::listLocalBranches(_wc, _quit),
           (QStringList{QStringLiteral("trunk"), QStringLiteral("feature")}));
  QCOMPARE(SvnRepo::listRemoteBranches(_wc, _quit),
           QStringList{QStringLiteral("feature")});

  QVERIFY2(SvnRepo::checkoutBranch(_wc, QStringLiteral("feature"),
                                   SvnRepo::BranchType::Local),
           qPrintable(lastErrorMessage()));
  QCOMPARE(SvnRepo::getActiveBranch(_wc, SvnRepo::BranchType::Local, _quit),
           QStringLiteral("feature"));
  QCOMPARE(svnInfo(_wc, QStringLiteral("url")),
           _rootUrl + QStringLiteral("/branches/feature"));

  QVERIFY(writeFile(file("feature.txt"), "feature\n"));
  QVERIFY2(commitAll(_wc, QStringLiteral("Feature work")),
           qPrintable(lastErrorMessage()));

  // The checked out branch can't be deleted.
  resetLastError();
  QVERIFY(!SvnRepo::deleteBranch(_wc, QStringLiteral("feature")));
  QVERIFY(!lastErrorMessage().isEmpty());

  // An unstaged edit the merge doesn't touch must stay out of the merge
  // revision (§7.4).
  QVERIFY(writeFile(file("a.txt"), "one\ntwo\nthree\nlocal\n"));
  QVERIFY2(SvnRepo::merge(_wc, QStringLiteral("feature"),
                          QStringLiteral("trunk"), kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QCOMPARE(SvnRepo::getActiveBranch(_wc, SvnRepo::BranchType::Local, _quit),
           QStringLiteral("trunk"));
  QVERIFY(QFileInfo::exists(file("feature.txt")));
  QVERIFY(QString::fromUtf8(svnlookLog(_repoDir, head()))
              .startsWith(QStringLiteral("Merge branch 'feature'")));
  QVERIFY(!svnPropget(QStringLiteral("svn:mergeinfo"), _rootUrl + "/trunk")
               .isEmpty());
  const QStringList changed = changedPaths(_repoDir, head());
  QVERIFY(changed.contains(QStringLiteral("A trunk/feature.txt")));
  QVERIFY2(!changed.contains(QStringLiteral("U trunk/a.txt")),
           qPrintable(changed.join(", ")));
  QCOMPARE(status().modifiedFiles, QStringList{QStringLiteral("a.txt")});
  QCOMPARE(readFile(file("a.txt")), QByteArray("one\ntwo\nthree\nlocal\n"));

  QVERIFY2(SvnRepo::renameBranch(_wc, QStringLiteral("feature"),
                                 QStringLiteral("feature2")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(SvnRepo::listRemoteBranches(_wc, _quit),
           QStringList{QStringLiteral("feature2")});
  QVERIFY(SvnRepo::deleteBranch(_wc, QStringLiteral("feature2")));
  QVERIFY(SvnRepo::listRemoteBranches(_wc, _quit).isEmpty());
  QVERIFY(svn({QStringLiteral("ls"), _rootUrl + QStringLiteral("/branches")})
              .isEmpty());
}

void SvnRepoTest::tags() {
  const QString revision =
      commitA("one\ntwo\nthree\n", QStringLiteral("Add a"));
  QVERIFY(!revision.isEmpty());

  QVERIFY2(SvnRepo::createTagFromCommit(_wc, revision, QStringLiteral("v1"),
                                        kAuthor, kEmail, QString()),
           qPrintable(lastErrorMessage()));
  QCOMPARE(svn({QStringLiteral("ls"), _rootUrl + QStringLiteral("/tags")}),
           QStringLiteral("v1/"));
  QCOMPARE(SvnRepo::listTags(_wc), QStringList{QStringLiteral("v1")});
  QCOMPARE(SvnRepo::getCommitTags(_wc, revision, _quit),
           QStringList{QStringLiteral("v1")});

  QVERIFY(SvnRepo::renameTag(_wc, QStringLiteral("v1"), QStringLiteral("v2")));
  QCOMPARE(SvnRepo::listTags(_wc), QStringList{QStringLiteral("v2")});

  QVERIFY(SvnRepo::checkoutBranch(_wc, QStringLiteral("v2"),
                                  SvnRepo::BranchType::Tag));
  QCOMPARE(SvnRepo::getActiveBranch(_wc, SvnRepo::BranchType::Tag, _quit),
           QStringLiteral("v2"));
  QCOMPARE(svnInfo(_wc, QStringLiteral("url")),
           _rootUrl + QStringLiteral("/tags/v2"));
  QVERIFY(SvnRepo::checkoutBranch(_wc, QStringLiteral("trunk"),
                                  SvnRepo::BranchType::Local));

  QVERIFY(SvnRepo::deleteTag(_wc, QStringLiteral("v2")));
  QVERIFY(SvnRepo::listTags(_wc).isEmpty());
  QVERIFY(svn({QStringLiteral("ls"), _rootUrl + QStringLiteral("/tags")})
              .isEmpty());
}

void SvnRepoTest::cherryPickAndRevert() {
  QVERIFY(writeFile(file("b.txt"), "b\n"));
  QVERIFY(!commitA("one\ntwo\nthree\nfour\n", QStringLiteral("Add four"))
               .isEmpty());
  const QString revision =
      commitA("one\ntwo\nthree\nfour\nfive\n", QStringLiteral("Add five"));
  QVERIFY(!revision.isEmpty());

  // An unstaged edit the operations don't touch must stay out of their
  // revisions (§7.4).
  QVERIFY(writeFile(file("b.txt"), "local\n"));

  QVERIFY2(SvnRepo::revertCommit(_wc, revision, kAuthor, kEmail, QString()),
           qPrintable(lastErrorMessage()));
  QCOMPARE(head().toInt(), revision.toInt() + 1);
  QCOMPARE(readFile(file("a.txt")), QByteArray("one\ntwo\nthree\nfour\n"));
  QVERIFY(QString::fromUtf8(svnlookLog(_repoDir, head()))
              .startsWith(QStringLiteral("Revert \"Add five\"")));
  QStringList changed = changedPaths(_repoDir, head());
  QVERIFY(changed.contains(QStringLiteral("U trunk/a.txt")));
  QVERIFY2(!changed.contains(QStringLiteral("U trunk/b.txt")),
           qPrintable(changed.join(", ")));

  // Cherry-picking the reverted revision brings the change back. A new
  // unstaged edit, in case the revert already committed the first one.
  QVERIFY(writeFile(file("b.txt"), "local again\n"));
  QVERIFY2(SvnRepo::cherryPickCommit(_wc, revision, kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QCOMPARE(head().toInt(), revision.toInt() + 2);
  QCOMPARE(readFile(file("a.txt")),
           QByteArray("one\ntwo\nthree\nfour\nfive\n"));
  changed = changedPaths(_repoDir, head());
  QVERIFY(changed.contains(QStringLiteral("U trunk/a.txt")));
  QVERIFY2(!changed.contains(QStringLiteral("U trunk/b.txt")),
           qPrintable(changed.join(", ")));

  QCOMPARE(readFile(file("b.txt")), QByteArray("local again\n"));
  QCOMPARE(status().modifiedFiles, QStringList{QStringLiteral("b.txt")});
}

void SvnRepoTest::patches() {
  QVERIFY(!commitA("one\ntwo\nthree\nfour\n", QStringLiteral("Add four"))
               .isEmpty());
  const QString revision =
      commitA("one\ntwo\nthree\nfour\nfive\n", QStringLiteral("Add five"));
  QVERIFY(!revision.isEmpty());

  const QString patch = SvnRepo::createPatchFromCommit(_wc, revision);
  QVERIFY2(patch.contains(QStringLiteral("diff --git a/a.txt b/a.txt")),
           qPrintable(patch));
  QVERIFY(patch.contains(QStringLiteral("+five")));

  QVERIFY(!commitA("one\ntwo\nthree\nfour\n", QStringLiteral("Drop five"))
               .isEmpty());
  const QString base = head();

  QVERIFY2(SvnRepo::applyPatch(_wc, patch), qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("a.txt")),
           QByteArray("one\ntwo\nthree\nfour\nfive\n"));
  // The patched file is an unstaged change; nothing was committed.
  QCOMPARE(status().modifiedFiles, QStringList{QStringLiteral("a.txt")});
  QCOMPARE(head(), base);

  QVERIFY(SvnRepo::discardFiles(_wc, {QStringLiteral("a.txt")}));
  QCOMPARE(readFile(file("a.txt")), QByteArray("one\ntwo\nthree\nfour\n"));
}

void SvnRepoTest::conflictedUpdateIsNotAbortable() {
  QVERIFY(!commitA("one\ntwo\nthree\n", QStringLiteral("Add a")).isEmpty());

  // Another working copy commits a conflicting change.
  const QString other = _base + QStringLiteral("/other");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk"), other));
  QVERIFY(writeFile(other + "/a.txt", "ONE\ntwo\nthree\n"));
  bool ok = false;
  svnIn(other,
        {QStringLiteral("commit"), QStringLiteral("-m"),
         QStringLiteral("Other change")},
        &ok);
  QVERIFY(ok);

  QVERIFY(writeFile(file("a.txt"), "uno\ntwo\nthree\n"));
  resetLastError();
  const bool pulled = SvnRepo::pull(_wc, QString(), false);
  QVERIFY(!pulled);
  QVERIFY(SvnRepo::hasConflicts(_wc));
  QCOMPARE(status().conflictedFiles, QStringList{QStringLiteral("a.txt")});
  QCOMPARE(svnStatusXml(_wc).value("a.txt").item,
           QStringLiteral("conflicted"));
  QVERIFY(SvnRepo::listConflictsForAFile(_wc, QStringLiteral("a.txt"))
              .contains(QStringLiteral("<<<<<<<")));

  // An update conflict is not a merge, cherry-pick or revert: there is
  // nothing to abort, and the user's edit must survive (§7.4).
  QVERIFY(!SvnRepo::getIsMergeInProgress(_wc));
  resetLastError();
  const bool aborted = SvnRepo::abortMerge(_wc);
  QVERIFY(!aborted);
  QCOMPARE(lastError(), SvnRepo::SvnError::AbortMergeNothingToAbort);
  QVERIFY(!lastErrorMessage().isEmpty());
  QVERIFY(readFile(file("a.txt")).contains("uno"));
  QVERIFY(SvnRepo::hasConflicts(_wc));
}

void SvnRepoTest::stashes() {
  QVERIFY(!commitA("one\ntwo\nthree\n", QStringLiteral("Add a")).isEmpty());
  QVERIFY(writeFile(file("a.txt"), "one\ntwo\nthree\nstashed\n"));

  QVERIFY2(SvnRepo::stash(_wc, QStringLiteral("my note")),
           qPrintable(lastErrorMessage()));
  QVERIFY(!status().hasChanges());
  QVERIFY(svnStatusXml(_wc).isEmpty());
  QCOMPARE(readFile(file("a.txt")), QByteArray("one\ntwo\nthree\n"));

  const QStringList stashes = SvnRepo::listStashes(_wc, _quit);
  QCOMPARE(stashes, QStringList{QStringLiteral("On trunk: my note")});
  QCOMPARE(SvnRepo::getStashStatus(_wc, 0).message,
           QStringLiteral("On trunk: my note"));

  QVERIFY2(SvnRepo::applyStash(_wc, 0), qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("a.txt")), QByteArray("one\ntwo\nthree\nstashed\n"));
  QCOMPARE(status().modifiedFiles, QStringList{QStringLiteral("a.txt")});

  QVERIFY(SvnRepo::deleteStash(_wc, 0));
  QVERIFY(SvnRepo::listStashes(_wc, _quit).isEmpty());
  resetLastError();
  QVERIFY(!SvnRepo::deleteStash(_wc, 0));
  QVERIFY(!lastErrorMessage().isEmpty());
  QCOMPARE(lastError(), SvnRepo::SvnError::InvalidArgument);

  // Nothing to stash in a clean working copy.
  QVERIFY(SvnRepo::discardFiles(_wc, {QStringLiteral("a.txt")}));
  resetLastError();
  QVERIFY(!SvnRepo::stash(_wc, QStringLiteral("empty")));
  QVERIFY(!lastErrorMessage().isEmpty());
  QVERIFY(SvnRepo::listStashes(_wc, _quit).isEmpty());
}

void SvnRepoTest::externals() {
  QVERIFY(!commitA("one\ntwo\nthree\n", QStringLiteral("Add a")).isEmpty());

  QVERIFY2(SvnRepo::addSubmodule(_wc, _rootUrl + QStringLiteral("/trunk"),
                                 QStringLiteral("ext"), QString(), QString()),
           qPrintable(lastErrorMessage()));
  QCOMPARE(SvnRepo::listSubmodules(_wc), QStringList{QStringLiteral("ext")});
  QVERIFY(QFileInfo::exists(file("ext/a.txt")));
  QVERIFY(svnPropget(QStringLiteral("svn:externals"), _wc)
              .contains(QStringLiteral("ext")));
  QCOMPARE(svnStatusXml(_wc).value(".").props, QStringLiteral("modified"));
  // The svn:externals change is a change of the working copy root that has
  // to be committed.
  QVERIFY(status().hasChanges());

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
  QVERIFY(svnPropget(QStringLiteral("svn:externals"), _wc).isEmpty());
  resetLastError();
  QVERIFY(!SvnRepo::updateSubmodule(_wc, QStringLiteral("ext")));
  QVERIFY(!lastErrorMessage().isEmpty());
}

void SvnRepoTest::remotes() {
  const QVector<SvnRepo::RemoteInfo> remotes = SvnRepo::listRemotes(_wc);
  QCOMPARE(remotes.size(), 1);
  QCOMPARE(remotes.first().name, QStringLiteral("origin"));
  QVERIFY(remotes.first().isDefault);
  QCOMPARE(remotes.first().fetchUrl,
           svnInfo(_wc, QStringLiteral("repos-root-url")));

  QCOMPARE(SvnRepo::getDefaultRemote(_wc), QStringLiteral("origin"));
  const auto [found, remote, branch] =
      SvnRepo::getUpstream(_wc, QStringLiteral("trunk"));
  QVERIFY(found);
  QCOMPARE(remote, QStringLiteral("origin"));
  QCOMPARE(branch, QStringLiteral("trunk"));

  QVERIFY(SvnRepo::fetchAll(_wc));
  QVERIFY(SvnRepo::push(_wc, QStringLiteral("trunk"), false));
  QVERIFY2(SvnRepo::updateRemote(_wc, QStringLiteral("origin"), QString(),
                                 _rootUrl, _rootUrl),
           qPrintable(lastErrorMessage()));
  QCOMPARE(svnInfo(_wc, QStringLiteral("repos-root-url")), _rootUrl);

  resetLastError();
  QVERIFY(
      !SvnRepo::addRemote(_wc, QStringLiteral("upstream"), _rootUrl, _rootUrl));
  EXPECT_SVN_ERROR(SvnRepo::SvnError::UnsupportedOperation);
  resetLastError();
  QVERIFY(!SvnRepo::removeRemote(_wc, QStringLiteral("origin")));
  EXPECT_SVN_ERROR(SvnRepo::SvnError::UnsupportedOperation);
  QCOMPARE(SvnRepo::listRemotes(_wc).size(), 1);
}

void SvnRepoTest::gitOnlyOperationsAreUnsupported() {
  QVERIFY(writeFile(file("b.txt"), "b\n"));
  QVERIFY(!commitA("one\ntwo\nthree\n", QStringLiteral("Add a")).isEmpty());
  const QString base = head();
  const QByteArray baseLog = svnlookLog(_repoDir, base);
  const QString wcRevision = svnInfo(_wc, QStringLiteral("revision"));

  // A staged and an unstaged edit that none of the calls may touch.
  QVERIFY(writeFile(file("a.txt"), "staged\n"));
  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("a.txt")}));
  QVERIFY(writeFile(file("b.txt"), "unstaged\n"));
  const auto expectUntouched = [&]() {
    QCOMPARE(head(), base);
    QCOMPARE(svnlookLog(_repoDir, base), baseLog);
    QCOMPARE(svnInfo(_wc, QStringLiteral("revision")), wcRevision);
    QCOMPARE(readFile(file("a.txt")), QByteArray("staged\n"));
    QCOMPARE(readFile(file("b.txt")), QByteArray("unstaged\n"));
    QCOMPARE(changelistMembers(svnStatusXml(_wc)),
             QStringList{QStringLiteral("a.txt")});
  };

  // Amend and reset don't exist for Subversion (§7.4).
  resetLastError();
  QVERIFY(!SvnRepo::amend(_wc, QStringLiteral("New message"), QString(),
                          kAuthor, kEmail, false, false));
  EXPECT_SVN_ERROR(SvnRepo::SvnError::UnsupportedOperation);
  expectUntouched();

  for (const SvnRepo::ResetType type :
       {SvnRepo::ResetType::Soft, SvnRepo::ResetType::Mixed,
        SvnRepo::ResetType::Hard, SvnRepo::ResetType::Unknown}) {
    resetLastError();
    QVERIFY(!SvnRepo::resetToCommit(_wc, QStringLiteral("1"), type));
    EXPECT_SVN_ERROR(SvnRepo::SvnError::UnsupportedOperation);
    expectUntouched();
  }

  resetLastError();
  QVERIFY(!SvnRepo::rebaseBranches(
      _wc, QStringLiteral("a"), QStringLiteral("b"), kAuthor, kEmail, false));
  EXPECT_SVN_ERROR(SvnRepo::SvnError::UnsupportedOperation);

  GitRepo::BisectStatus bisectStatus;
  resetLastError();
  QVERIFY(!SvnRepo::bisect(_wc, GitRepo::BisectAction::Good, QString(),
                           bisectStatus));
  EXPECT_SVN_ERROR(SvnRepo::SvnError::UnsupportedOperation);

  resetLastError();
  QVERIFY(!SvnRepo::addWorktree(_wc, QStringLiteral("w"), _base + "/w",
                                QStringLiteral("trunk"), false));
  EXPECT_SVN_ERROR(SvnRepo::SvnError::UnsupportedOperation);
  QVERIFY(!QFileInfo::exists(_base + "/w"));

  resetLastError();
  QVERIFY(!SvnRepo::startGitHubFlowBranch(_wc, QStringLiteral("x"),
                                          QStringLiteral("trunk")));
  EXPECT_SVN_ERROR(SvnRepo::SvnError::UnsupportedOperation);

  QVERIFY(SvnRepo::listWorktrees(_wc).isEmpty());
  QVERIFY(!SvnRepo::getIsRebaseInProgress(_wc));
  QVERIFY(SvnRepo::getBisectMarks(_wc).isEmpty());
  expectUntouched();
}

// --- 3.1 Repository lifecycle ----------------------------------------------

namespace {

// The "*-svnrepo" directories below dir, as paths relative to it.
QStringList repositoryDirectories(const QString &dir) {
  QStringList found;
  QDirIterator it(dir, {QStringLiteral("*-svnrepo")},
                  QDir::Dirs | QDir::NoDotAndDotDot,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    found += QDir(dir).relativeFilePath(it.next());
  }
  found.sort();
  return found;
}

#ifdef Q_OS_WIN
// A directory junction at link pointing to target.
bool makeJunction(const QString &link, const QString &target) {
  QProcess process;
  process.start(QStringLiteral("cmd.exe"),
                {QStringLiteral("/c"), QStringLiteral("mklink"),
                 QStringLiteral("/J"), QDir::toNativeSeparators(link),
                 QDir::toNativeSeparators(target)});
  return process.waitForFinished(30 * 1000) && process.exitCode() == 0;
}
#endif

} // namespace

void SvnRepoTest::initIgnoresDefaultBranch_data() {
  QTest::addColumn<QString>("branch");
  QTest::newRow("main") << QStringLiteral("main");
  QTest::newRow("trunk") << QStringLiteral("trunk");
  QTest::newRow("slash") << QStringLiteral("feature/x");
  QTest::newRow("unicode") << QString::fromUtf16(u"ветка");
}

void SvnRepoTest::initIgnoresDefaultBranch() {
  QFETCH(QString, branch);

  // Subversion's layout always has trunk, and the working copy is trunk
  // whatever default branch is asked for (B23 documents that it is ignored).
  const QString wc = _base + QStringLiteral("/other");
  QVERIFY2(SvnRepo::initRepository(QStringLiteral("other"), wc, branch,
                                   kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  const QString url = fileUrl(_base + QStringLiteral("/other-svnrepo"));
  QCOMPARE(svnInfo(wc, QStringLiteral("url")), url + QStringLiteral("/trunk"));
  QVERIFY(svn({QStringLiteral("ls"), url + QStringLiteral("/branches")})
              .isEmpty());
  QCOMPARE(SvnRepo::getActiveBranch(wc, SvnRepo::BranchType::Local, _quit),
           QStringLiteral("trunk"));
}

void SvnRepoTest::initAuthorIsRecorded() {
  // The layout revision is the first one made in the user's name.
  QCOMPARE(svnlookAuthor(_repoDir, QStringLiteral("1")), kAuthor);
  QCOMPARE(QString::fromUtf8(svnlookLog(_repoDir, QStringLiteral("1"))),
           QStringLiteral("Initial layout"));
}

void SvnRepoTest::initRefusesExistingWorkingCopy() {
  // The same repository name again: refused before anything is made.
  resetLastError();
  QVERIFY(!SvnRepo::initRepository(QStringLiteral("repo"), _wc,
                                   QStringLiteral("main"), kAuthor, kEmail));
  EXPECT_SVN_ERROR(SvnRepo::SvnError::InvalidArgument);
  QCOMPARE(head(), QStringLiteral("1"));

  // Another name into the existing working copy: the checkout fails, and the
  // new repository must not stay behind.
  resetLastError();
  QVERIFY(!SvnRepo::initRepository(QStringLiteral("again"), _wc,
                                   QStringLiteral("main"), kAuthor, kEmail));
  QVERIFY(!lastErrorMessage().isEmpty());
  QCOMPARE(svnInfo(_wc, QStringLiteral("url")),
           _rootUrl + QStringLiteral("/trunk"));
  QVERIFY(!QFileInfo::exists(_base + QStringLiteral("/again-svnrepo")));

  // A retry with the problem fixed works.
  const bool retried = SvnRepo::initRepository(
      QStringLiteral("again"), _base + QStringLiteral("/again"),
      QStringLiteral("main"), kAuthor, kEmail);
  QVERIFY2(retried, qPrintable(lastErrorMessage()));
}

void SvnRepoTest::initIntoGitRepository() {
  const QString dir = _base + QStringLiteral("/gitrepo");
  QVERIFY(QDir().mkpath(dir + QStringLiteral("/.git")));
  QVERIFY(writeFile(dir + QStringLiteral("/.git/HEAD"),
                    "ref: refs/heads/main\n"));
  QVERIFY(writeFile(dir + QStringLiteral("/tracked.txt"), "git\n"));

  // A Subversion working copy inside a Git repository mixes the two; it is
  // refused and nothing is created.
  resetLastError();
  const bool created = SvnRepo::initRepository(
      QStringLiteral("inside"), dir, QStringLiteral("main"), kAuthor, kEmail);
  QVERIFY(!created);
  QVERIFY(!QFileInfo::exists(_base + QStringLiteral("/inside-svnrepo")));
  QCOMPARE(readFile(dir + QStringLiteral("/tracked.txt")), QByteArray("git\n"));
}

void SvnRepoTest::initUnusualRepoNames_data() {
  QTest::addColumn<QString>("name");
  QTest::addColumn<bool>("valid");
  QTest::newRow("spaces") << QStringLiteral("my repo") << true;
  QTest::newRow("unicode") << QString::fromUtf16(u"répo 日本")
                           << true;
  QTest::newRow("escape") << QStringLiteral("../escape") << false;
  QTest::newRow("slash") << QStringLiteral("a/b") << false;
  QTest::newRow("backslash") << QStringLiteral("a\\b") << false;
  QTest::newRow("empty") << QString() << false;
  QTest::newRow("blank") << QStringLiteral("   ") << false;
  // A drive root as the working directory is not tested: it would create a
  // repository in the root of the drive.
}

void SvnRepoTest::initUnusualRepoNames() {
  QFETCH(QString, name);
  QFETCH(bool, valid);

  const QString parent = _base + QStringLiteral("/parent");
  QVERIFY(QDir().mkpath(parent));
  const QString wc = parent + QStringLiteral("/wc");
  const QStringList before = repositoryDirectories(_base);

  resetLastError();
  const bool created = SvnRepo::initRepository(
      name, wc, QStringLiteral("main"), kAuthor, kEmail);
  if (valid) {
    QVERIFY2(created, qPrintable(lastErrorMessage()));
    QVERIFY(QFileInfo(parent + QLatin1Char('/') + name +
                      QStringLiteral("-svnrepo/format"))
                .exists());
    QVERIFY(svnStatusXml(wc).isEmpty());
    return;
  }

  // Unsafe names are refused and nothing is created, inside or outside the
  // parent directory.
  QVERIFY(!created);
  QVERIFY(!lastErrorMessage().isEmpty());
  QCOMPARE(repositoryDirectories(_base), before);
}

void SvnRepoTest::initIntoUnusualPaths_data() { unusualNames(); }

void SvnRepoTest::initIntoUnusualPaths() {
  QFETCH(QString, name);

  // The name as the working copy's directory.
  const QString wc = _base + QStringLiteral("/p/") + name;
  QVERIFY(QDir().mkpath(QFileInfo(wc).absolutePath()));
  QVERIFY2(SvnRepo::initRepository(QStringLiteral("r"), wc,
                                   QStringLiteral("main"), kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QVERIFY2(!SvnRepo::openRepository(wc).isEmpty(),
           qPrintable(lastErrorMessage()));
  QVERIFY(writeFile(wc + QStringLiteral("/x.txt"), "x\n"));
  QVERIFY2(commitAll(wc, QStringLiteral("Add x")),
           qPrintable(lastErrorMessage()));
  const QString repoDir =
      QFileInfo(wc).absolutePath() + QStringLiteral("/r-svnrepo");
  QCOMPARE(changedPaths(repoDir, QStringLiteral("2")),
           QStringList{QStringLiteral("A trunk/x.txt")});
  QVERIFY(!SvnRepo::getStatus(wc, _quit).hasChanges());
  SvnRepo::closeRepository(wc);
}

void SvnRepoTest::openRejectsNonWorkingCopy_data() {
  QTest::addColumn<QString>("kind");
  QTest::newRow("empty dir") << QStringLiteral("empty dir");
  QTest::newRow("missing") << QStringLiteral("missing");
  QTest::newRow("file") << QStringLiteral("file");
  QTest::newRow("empty path") << QStringLiteral("empty path");
  QTest::newRow("unversioned subdir") << QStringLiteral("unversioned subdir");
}

void SvnRepoTest::openRejectsNonWorkingCopy() {
  QFETCH(QString, kind);

  QString path;
  if (kind == QLatin1String("empty dir")) {
    path = _base + QStringLiteral("/empty");
    QVERIFY(QDir().mkpath(path));
  } else if (kind == QLatin1String("missing")) {
    path = _base + QStringLiteral("/missing");
  } else if (kind == QLatin1String("file")) {
    path = _base + QStringLiteral("/file.txt");
    QVERIFY(writeFile(path, "x"));
  } else if (kind == QLatin1String("unversioned subdir")) {
    path = file("loose");
    QVERIFY(QDir().mkpath(path));
  }

  resetLastError();
  QVERIFY(SvnRepo::openRepository(path).isEmpty());
  EXPECT_SVN_ERROR(SvnRepo::SvnError::ErrorWhileOpening);
}

void SvnRepoTest::openFromSubdirectory() {
  QVERIFY(writeFile(file("dir/a.txt"), "a\n"));
  QVERIFY(commitAll(_wc, QStringLiteral("Add dir")));
  QVERIFY(writeFile(file("dir/a.txt"), "changed\n"));
  QVERIFY(writeFile(file("top.txt"), "top\n"));

  // Opening a folder inside the working copy opens the working copy, so paths
  // stay relative to its root.
  const QString opened = SvnRepo::openRepository(file("dir"));
  QVERIFY2(!opened.isEmpty(), qPrintable(lastErrorMessage()));
  QCOMPARE(opened, _wc);
  const SvnRepo::SvnStatus st = SvnRepo::getStatus(opened, _quit);
  QCOMPARE(st.modifiedFiles, QStringList{QStringLiteral("dir/a.txt")});
  QCOMPARE(st.newFiles, QStringList{QStringLiteral("top.txt")});
}

void SvnRepoTest::openPathSpellings_data() {
  QTest::addColumn<QString>("spelling");
  QTest::newRow("dot segments") << QStringLiteral("dots");
  QTest::newRow("backslashes") << QStringLiteral("native");
  QTest::newRow("trailing slash") << QStringLiteral("trailing");
#ifdef Q_OS_WIN
  QTest::newRow("lowercase drive") << QStringLiteral("lower");
  QTest::newRow("junction") << QStringLiteral("junction");
#endif
}

void SvnRepoTest::openPathSpellings() {
  QFETCH(QString, spelling);

  QString path;
  if (spelling == QLatin1String("dots")) {
    path = _base + QStringLiteral("/wc/./x/../");
  } else if (spelling == QLatin1String("native")) {
    path = QDir::toNativeSeparators(_wc);
  } else if (spelling == QLatin1String("trailing")) {
    path = _wc + QLatin1Char('/');
  } else if (spelling == QLatin1String("lower")) {
    path = _wc.left(1).toLower() + _wc.mid(1);
#ifdef Q_OS_WIN
  } else if (spelling == QLatin1String("junction")) {
    path = _base + QStringLiteral("/link");
    QVERIFY(makeJunction(path, _wc));
#endif
  }

  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString opened = SvnRepo::openRepository(path);
  QVERIFY2(!opened.isEmpty(), qPrintable(lastErrorMessage()));
  // Every spelling reaches the same working copy, and the other API calls
  // accept it too.
  QCOMPARE(SvnRepo::getStatus(path, _quit).newFiles,
           QStringList{QStringLiteral("a.txt")});
  QVERIFY2(SvnRepo::stageFiles(path, {QStringLiteral("a.txt")}),
           qPrintable(lastErrorMessage()));
  QCOMPARE(changelistMembers(svnStatusXml(_wc)),
           QStringList{QStringLiteral("a.txt")});
  const bool committed =
      SvnRepo::commitStaged(path, QStringLiteral("Add a"), QString(), kAuthor,
                            kEmail, true, false, false);
  // B31 depends on the machine: it fails on a Windows 11 desktop, but not in
  // the Windows Server Core build container.
  if (!committed) {
    QEXPECT_FAIL("junction", "B31: commits through a junction fail",
                 Continue);
  }
  QVERIFY2(committed, qPrintable(lastErrorMessage()));
  if (!committed) {
    QEXPECT_FAIL("junction", "B31: commits through a junction fail",
                 Continue);
  }
  QCOMPARE(changedPaths(_repoDir, head()),
           QStringList{QStringLiteral("A trunk/a.txt")});
  SvnRepo::closeRepository(path);
}

void SvnRepoTest::openWorkingCopiesOfBothVersions() {
  // A working copy made by the 1.14 command line client opens.
  const QString cliWc = _base + QStringLiteral("/cli");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk"), cliWc));
  QVERIFY2(!SvnRepo::openRepository(cliWc).isEmpty(),
           qPrintable(lastErrorMessage()));
  QVERIFY(writeFile(cliWc + QStringLiteral("/b.txt"), "b\n"));
  QVERIFY2(commitAll(cliWc, QStringLiteral("Add b")),
           qPrintable(lastErrorMessage()));
  SvnRepo::closeRepository(cliWc);

  // A working copy the library made is read by the 1.14 client.
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  QVERIFY(ok);
  QVERIFY(QFileInfo::exists(file("b.txt")));
  QCOMPARE(svnInfo(_wc, QStringLiteral("revision")), head());

  // Not covered yet: a working copy in the 1.6 format, which needs an upgrade
  // first. There is no fixture for it (SVN_TEST_PLAN 3.1).
}

void SvnRepoTest::openLockedWorkingCopy() {
  QVERIFY(!commitA("a\n", QStringLiteral("Add a")).isEmpty());
  const QString base = head();
  QVERIFY(writeFile(file("a.txt"), "changed\n"));
  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("a.txt")}));
  QVERIFY2(SvnRepo::Testing::leaveStaleWorkingCopyLock(_wc),
           qPrintable(lastErrorMessage()));
  QVERIFY(svnStatusXml(_wc, true).contains(QStringLiteral(".")));

  // Reading works on a locked working copy, as with the CLI.
  QVERIFY(!SvnRepo::openRepository(_wc).isEmpty());
  QCOMPARE(status().modifiedStagedFiles, QStringList{QStringLiteral("a.txt")});

  // Writing fails with Subversion's own error (not "Nothing is staged", B14),
  // and the lock is not cleaned up behind the user's back.
  resetLastError();
  QVERIFY(!SvnRepo::commitStaged(_wc, QStringLiteral("Locked"), QString(),
                                 kAuthor, kEmail, true, false, false));
  EXPECT_SVN_ERROR(SvnRepo::SvnError::CommitFailed);
  QVERIFY2(lastErrorMessage().contains(QStringLiteral("lock"),
                                       Qt::CaseInsensitive),
           qPrintable(lastErrorMessage()));
  QCOMPARE(head(), base);
  bool ok = true;
  svnIn(_wc, {QStringLiteral("commit"), QStringLiteral("-m"),
              QStringLiteral("CLI")},
        &ok);
  QVERIFY(!ok);
}

void SvnRepoTest::closeRepositoryOfOtherPath() {
  const QString other = _base + QStringLiteral("/other");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk"), other));
  QVERIFY(!SvnRepo::openRepository(_wc).isEmpty());
  QVERIFY(!SvnRepo::openRepository(other).isEmpty());

  // Closing A leaves B usable.
  SvnRepo::closeRepository(_wc);
  QVERIFY(writeFile(other + QStringLiteral("/b.txt"), "b\n"));
  QCOMPARE(SvnRepo::getStatus(other, _quit).newFiles,
           QStringList{QStringLiteral("b.txt")});
  QVERIFY2(commitAll(other, QStringLiteral("Add b")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(changedPaths(_repoDir, head()),
           QStringList{QStringLiteral("A trunk/b.txt")});
  SvnRepo::closeRepository(other);
}

void SvnRepoTest::cloneUrlSpellings_data() {
  QTest::addColumn<QString>("spelling");
  QTest::newRow("canonical") << QStringLiteral("canonical");
  QTest::newRow("trailing slash") << QStringLiteral("trailing");
  QTest::newRow("upper scheme") << QStringLiteral("upper");
  QTest::newRow("localhost") << QStringLiteral("localhost");
  QTest::newRow("unescaped space") << QStringLiteral("space");
  QTest::newRow("escaped space") << QStringLiteral("escaped");
  QTest::newRow("local path") << QStringLiteral("path");
  QTest::newRow("backslash path") << QStringLiteral("backslash");
  QTest::newRow("at sign") << QStringLiteral("at");
}

void SvnRepoTest::cloneUrlSpellings() {
  QFETCH(QString, spelling);

  // Repositories whose directory needs escaping in a URL.
  QString repoDir = _repoDir;
  if (spelling == QLatin1String("space") ||
      spelling == QLatin1String("escaped")) {
    repoDir = _base + QStringLiteral("/my repo");
    QVERIFY(!makeRepo(repoDir, Layout::Standard).isEmpty());
  } else if (spelling == QLatin1String("at")) {
    repoDir = _base + QStringLiteral("/at@repo");
    QVERIFY(!makeRepo(repoDir, Layout::Standard).isEmpty());
  }
  const QString canonical = fileUrl(repoDir) + QStringLiteral("/trunk");

  QString url = canonical;
  if (spelling == QLatin1String("trailing")) {
    url += QLatin1Char('/');
  } else if (spelling == QLatin1String("upper")) {
    url.replace(0, 4, QStringLiteral("FILE"));
  } else if (spelling == QLatin1String("localhost")) {
    url.replace(QStringLiteral("file:///"), QStringLiteral("file://localhost/"));
  } else if (spelling == QLatin1String("space")) {
    url = QStringLiteral("file:///") + repoDir + QStringLiteral("/trunk");
  } else if (spelling == QLatin1String("path")) {
    url = repoDir + QStringLiteral("/trunk");
  } else if (spelling == QLatin1String("backslash")) {
    url = QDir::toNativeSeparators(repoDir + QStringLiteral("/trunk"));
  }

  // Canonicalised, or refused with CheckoutFailed; never a malfunction.
  const QString dest = _base + QStringLiteral("/clone");
  resetLastError();
  const bool cloned = SvnRepo::cloneRepository(url, dest);
  const QStringList malfunctions = SvnRepo::Testing::takeMalfunctions();
  QVERIFY2(malfunctions.isEmpty(), qPrintable(malfunctions.join('\n')));
  if (cloned) {
    QCOMPARE(svnInfo(dest, QStringLiteral("url")), canonical);
    QVERIFY(svnStatusXml(dest).isEmpty());
  } else {
    EXPECT_SVN_ERROR(SvnRepo::SvnError::CheckoutFailed);
    QVERIFY(!QFileInfo::exists(dest + QStringLiteral("/.svn")));
  }
  SvnRepo::closeRepository(dest);
}

void SvnRepoTest::cloneIntoExistingDestinations_data() {
  QTest::addColumn<QString>("kind");
  QTest::newRow("non-empty dir") << QStringLiteral("non-empty");
  QTest::newRow("other working copy") << QStringLiteral("other wc");
  QTest::newRow("missing parents") << QStringLiteral("missing parents");
}

void SvnRepoTest::cloneIntoExistingDestinations() {
  QFETCH(QString, kind);

  QVERIFY(!commitA("a\n", QStringLiteral("Add a")).isEmpty());
  QString dest = _base + QStringLiteral("/dest");
  if (kind == QLatin1String("non-empty")) {
    QVERIFY(writeFile(dest + QStringLiteral("/mine.txt"), "mine\n"));
  } else if (kind == QLatin1String("other wc")) {
    const QString otherRepo = _base + QStringLiteral("/other-repo");
    QVERIFY(!makeRepo(otherRepo, Layout::Standard).isEmpty());
    QVERIFY(secondWc(fileUrl(otherRepo) + QStringLiteral("/trunk"), dest));
    QVERIFY(writeFile(dest + QStringLiteral("/mine.txt"), "mine\n"));
  } else {
    dest = _base + QStringLiteral("/missing/nested/dest");
  }

  resetLastError();
  const bool cloned =
      SvnRepo::cloneRepository(_rootUrl + QStringLiteral("/trunk"), dest);
  if (kind == QLatin1String("other wc")) {
    // Another working copy is never taken over.
    QVERIFY(!cloned);
    EXPECT_SVN_ERROR(SvnRepo::SvnError::CheckoutFailed);
    QVERIFY(svnInfo(dest, QStringLiteral("url")).endsWith(
        QStringLiteral("other-repo/trunk")));
  } else {
    QVERIFY2(cloned, qPrintable(lastErrorMessage()));
    QCOMPARE(svnInfo(dest, QStringLiteral("url")),
             _rootUrl + QStringLiteral("/trunk"));
    QCOMPARE(readFile(dest + QStringLiteral("/a.txt")), QByteArray("a\n"));
  }
  // Files that were there are left alone.
  if (kind != QLatin1String("missing parents")) {
    QCOMPARE(readFile(dest + QStringLiteral("/mine.txt")),
             QByteArray("mine\n"));
  }
  SvnRepo::closeRepository(dest);
}

void SvnRepoTest::cloneFailureCleansUp() {
  const QString dest = _base + QStringLiteral("/new/deeper/dest");
  resetLastError();
  QVERIFY(!SvnRepo::cloneRepository(
      fileUrl(_base + QStringLiteral("/no-such-repo")), dest));
  EXPECT_SVN_ERROR(SvnRepo::SvnError::CheckoutFailed);
  QVERIFY(!QFileInfo::exists(dest));
  QVERIFY(!QFileInfo::exists(_base + QStringLiteral("/new")));
}

void SvnRepoTest::cloneRepositoryRoot() {
  QVERIFY(!commitA("a\n", QStringLiteral("Add a")).isEmpty());

  // The root holds every branch and tag; it is not a branch (§7.4 item 8).
  const QString dest = _base + QStringLiteral("/root");
  QVERIFY2(SvnRepo::cloneRepository(_rootUrl, dest),
           qPrintable(lastErrorMessage()));
  QVERIFY(QFileInfo::exists(dest + QStringLiteral("/trunk/a.txt")));
  QVERIFY(QFileInfo(dest + QStringLiteral("/branches")).isDir());
  QVERIFY(QFileInfo(dest + QStringLiteral("/tags")).isDir());
  resetLastError();
  const QString active =
      SvnRepo::getActiveBranch(dest, SvnRepo::BranchType::Local, _quit);
  QCOMPARE(active, QString());
  QCOMPARE(lastError(), SvnRepo::SvnError::Unknown);
  SvnRepo::closeRepository(dest);
}

void SvnRepoTest::lastErrorIsPerThread() {
  resetLastError();
  SvnRepo::SvnError otherError = SvnRepo::SvnError::Unknown;
  std::thread worker([&]() {
    SvnRepo::initForCurrentThread();
    SvnRepo::openRepository(QString());
    otherError = SvnRepo::getLastError().error;
    SvnRepo::shutdownForCurrentThread();
  });
  worker.join();

  QCOMPARE(otherError, SvnRepo::SvnError::ErrorWhileOpening);
  QCOMPARE(lastError(), SvnRepo::SvnError::Unknown);
  QVERIFY(lastErrorMessage().isEmpty());
}

void SvnRepoTest::concurrentCallsOnTwoThreads() {
  for (int i = 0; i < 5; ++i) {
    QVERIFY(writeFile(file(QStringLiteral("f%1.txt").arg(i)), "x\n"));
    QVERIFY(commitAll(_wc, QStringLiteral("Commit %1").arg(i)));
  }
  const QString other = _base + QStringLiteral("/other");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk"), other));
  QVERIFY(writeFile(file("new.txt"), "n\n"));
  QVERIFY(writeFile(other + QStringLiteral("/f0.txt"), "changed\n"));

  const SvnRepo::SvnStatus expectedA = status();
  const SvnRepo::SvnStatus expectedB = SvnRepo::getStatus(other, _quit);
  const QVector<QString> expectedHistory = SvnRepo::getHistory(_wc).commits;

  // Status and history on both working copies at once, many times.
  std::atomic<int> failures{0};
  const auto work = [&](const QString &wc, const SvnRepo::SvnStatus &expected) {
    SvnRepo::initForCurrentThread();
    const bool quit = false;
    for (int i = 0; i < 20; ++i) {
      if (!(SvnRepo::getStatus(wc, quit) == expected) ||
          SvnRepo::getHistory(wc).commits != expectedHistory) {
        ++failures;
      }
    }
    SvnRepo::shutdownForCurrentThread();
  };
  std::thread first(work, _wc, expectedA);
  std::thread second(work, other, expectedB);
  first.join();
  second.join();
  QCOMPARE(failures.load(), 0);
}

void SvnRepoTest::successfulCallDoesNotResetTheError() {
  // Documents the contract: getLastError() describes the last failure, not
  // the last call. Callers check the return value first; functions that
  // return a value (getStatus, hasConflicts) can't be told apart from failures
  // through it.
  resetLastError();
  QVERIFY(SvnRepo::openRepository(QString()).isEmpty());
  QCOMPARE(lastError(), SvnRepo::SvnError::ErrorWhileOpening);
  QVERIFY(!SvnRepo::openRepository(_wc).isEmpty());
  QCOMPARE(lastError(), SvnRepo::SvnError::ErrorWhileOpening);
}

// --- 3.2 Status -------------------------------------------------------------

namespace {

// What SvnRepo::getStatus should report for an `svn status --xml` result:
// the oracle's items mapped onto the status lists, files only.
SvnRepo::SvnStatus statusFromOracle(const StatusMap &oracle, const QString &wc) {
  SvnRepo::SvnStatus expected;
  for (auto it = oracle.cbegin(); it != oracle.cend(); ++it) {
    const QString &path = it.key();
    const StatusEntry &entry = it.value();
    if (path == QLatin1String(".")) {
      continue;
    }
    const bool conflicted = entry.item == QLatin1String("conflicted") ||
                            entry.props == QLatin1String("conflicted") ||
                            entry.treeConflicted;
    if (conflicted) {
      expected.conflictedFiles += path;
    }
    const bool isDir = QFileInfo(wc + QLatin1Char('/') + path).isDir() &&
                       entry.item != QLatin1String("obstructed");
    if (isDir) {
      continue;
    }
    const bool staged = entry.changelist == kStagingChangelist;
    if (entry.item == QLatin1String("unversioned")) {
      expected.newFiles += path;
    } else if (entry.item == QLatin1String("added")) {
      if (!staged) {
        expected.newFiles += path;
      } else if (entry.copied) {
        expected.renamedStagedFiles += path;
      } else {
        expected.newStagedFiles += path;
      }
    } else if (entry.item == QLatin1String("modified") ||
               entry.item == QLatin1String("merged") ||
               (entry.item == QLatin1String("normal") &&
                entry.props == QLatin1String("modified"))) {
      (staged ? expected.modifiedStagedFiles : expected.modifiedFiles) += path;
    } else if (entry.item == QLatin1String("deleted")) {
      (staged ? expected.deletedStagedFiles : expected.deletedFiles) += path;
    } else if (entry.item == QLatin1String("missing")) {
      expected.deletedFiles += path;
    } else if (entry.item == QLatin1String("replaced")) {
      (staged ? expected.typeChangedStagedFiles : expected.typeChangedFiles) +=
          path;
    } else if (entry.item == QLatin1String("obstructed")) {
      expected.typeChangedFiles += path;
    }
  }
  return expected;
}

// The non-empty lists of a status, for failure messages.
QString describe(const SvnRepo::SvnStatus &st) {
  QStringList parts;
  const auto add = [&](const char *name, const QStringList &list) {
    if (!list.isEmpty()) {
      parts += QString::fromLatin1(name) + QStringLiteral(": ") +
               list.join(QStringLiteral(", "));
    }
  };
  add("new", st.newFiles);
  add("modified", st.modifiedFiles);
  add("deleted", st.deletedFiles);
  add("typeChanged", st.typeChangedFiles);
  add("newStaged", st.newStagedFiles);
  add("modifiedStaged", st.modifiedStagedFiles);
  add("deletedStaged", st.deletedStagedFiles);
  add("typeChangedStaged", st.typeChangedStagedFiles);
  add("renamedStaged", st.renamedStagedFiles);
  add("conflicted", st.conflictedFiles);
  add("ignored", st.ignoredFiles);
  return parts.join(QStringLiteral("; "));
}

QStringList sorted(QStringList list) {
  list.sort();
  return list;
}

#define COMPARE_STATUS_LIST(actual, expected, list)                            \
  QCOMPARE(sorted((actual).list), sorted((expected).list))

} // namespace

void SvnRepoTest::statusCategories() {
  const QStringList names{"mod", "del", "miss", "repl", "obst", "conf",
                          "propconf", "tree", "copysrc", "movesrc", "propmod",
                          "staged", "stageddel", "lockme", "sub/s"};
  for (const QString &name : names) {
    QVERIFY(writeFile(file(name + QStringLiteral(".txt")), "base\n"));
  }
  QVERIFY(commitAll(_wc, QStringLiteral("Base")));
  bool ok = false;
  svn({QStringLiteral("copy"), QStringLiteral("-m"), QStringLiteral("Branch"),
       _rootUrl + QStringLiteral("/trunk"),
       _rootUrl + QStringLiteral("/branches/x")},
      &ok);
  QVERIFY(ok);

  // Another working copy makes the incoming side of the conflicts.
  const QString other = _base + QStringLiteral("/other");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk"), other));
  QVERIFY(writeFile(other + QStringLiteral("/conf.txt"), "theirs\n"));
  svnIn(other, {QStringLiteral("propset"), QStringLiteral("p"),
                QStringLiteral("theirs"), QStringLiteral("propconf.txt")});
  svnIn(other, {QStringLiteral("delete"), QStringLiteral("tree.txt")});
  svnIn(other, {QStringLiteral("commit"), QStringLiteral("-m"),
                QStringLiteral("Theirs")},
        &ok);
  QVERIFY(ok);

  // Local sides, then an update that conflicts.
  QVERIFY(writeFile(file("conf.txt"), "mine\n"));
  svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("p"),
              QStringLiteral("mine"), QStringLiteral("propconf.txt")});
  QVERIFY(writeFile(file("tree.txt"), "mine\n"));
  svnIn(_wc, {QStringLiteral("update"), QStringLiteral("--accept"),
              QStringLiteral("postpone")});

  // Every other state.
  QVERIFY(writeFile(file("mod.txt"), "changed\n"));
  svnIn(_wc, {QStringLiteral("delete"), QStringLiteral("del.txt")});
  QVERIFY(QFile::remove(file("miss.txt")));
  svnIn(_wc, {QStringLiteral("delete"), QStringLiteral("repl.txt")});
  QVERIFY(writeFile(file("repl.txt"), "new node\n"));
  svnIn(_wc, {QStringLiteral("add"), QStringLiteral("repl.txt")});
  QVERIFY(QFile::remove(file("obst.txt")));
  QVERIFY(QDir().mkpath(file("obst.txt")));
  QVERIFY(writeFile(file("new.txt"), "new\n"));
  QVERIFY(writeFile(file("added.txt"), "added\n"));
  svnIn(_wc, {QStringLiteral("add"), QStringLiteral("added.txt")});
  svnIn(_wc, {QStringLiteral("copy"), QStringLiteral("copysrc.txt"),
              QStringLiteral("copied.txt")});
  svnIn(_wc, {QStringLiteral("copy"), QStringLiteral("copysrc.txt"),
              QStringLiteral("copiedstaged.txt")});
  svnIn(_wc, {QStringLiteral("move"), QStringLiteral("movesrc.txt"),
              QStringLiteral("moved.txt")});
  svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("q"),
              QStringLiteral("v"), QStringLiteral("propmod.txt")});
  QVERIFY(writeFile(file("staged.txt"), "staged\n"));
  QVERIFY(writeFile(file("stagednew.txt"), "staged new\n"));
  svnIn(_wc, {QStringLiteral("delete"), QStringLiteral("stageddel.txt")});
  svnIn(_wc, {QStringLiteral("lock"), QStringLiteral("lockme.txt")}, &ok);
  QVERIFY(ok);
  svnIn(_wc, {QStringLiteral("switch"),
              _rootUrl + QStringLiteral("/branches/x/sub"),
              QStringLiteral("sub")},
        &ok);
  QVERIFY(ok);
  QVERIFY2(SvnRepo::stageFiles(_wc, {QStringLiteral("staged.txt"),
                                     QStringLiteral("stagednew.txt"),
                                     QStringLiteral("stageddel.txt"),
                                     QStringLiteral("copiedstaged.txt")}),
           qPrintable(lastErrorMessage()));

  const StatusMap oracle = svnStatusXml(_wc);
  QCOMPARE(oracle.value("conf.txt").item, QStringLiteral("conflicted"));
  QCOMPARE(oracle.value("propconf.txt").props, QStringLiteral("conflicted"));
  QVERIFY(oracle.value("tree.txt").treeConflicted);
  QCOMPARE(oracle.value("repl.txt").item, QStringLiteral("replaced"));
  QCOMPARE(oracle.value("obst.txt").item, QStringLiteral("obstructed"));

  const SvnRepo::SvnStatus expected = statusFromOracle(oracle, _wc);
  const SvnRepo::SvnStatus actual = status();
  COMPARE_STATUS_LIST(actual, expected, newFiles);
  COMPARE_STATUS_LIST(actual, expected, modifiedFiles);
  COMPARE_STATUS_LIST(actual, expected, deletedFiles);
  COMPARE_STATUS_LIST(actual, expected, typeChangedFiles);
  COMPARE_STATUS_LIST(actual, expected, newStagedFiles);
  COMPARE_STATUS_LIST(actual, expected, modifiedStagedFiles);
  COMPARE_STATUS_LIST(actual, expected, deletedStagedFiles);
  COMPARE_STATUS_LIST(actual, expected, typeChangedStagedFiles);
  COMPARE_STATUS_LIST(actual, expected, renamedStagedFiles);
  COMPARE_STATUS_LIST(actual, expected, conflictedFiles);

  // Pins: a move is an addition and a deletion (B15); the locked file, the
  // switched folder and the clean files are not changes.
  QVERIFY(actual.newFiles.contains(QStringLiteral("moved.txt")));
  QVERIFY(actual.deletedFiles.contains(QStringLiteral("movesrc.txt")));
  QVERIFY(actual.renamedFiles.isEmpty());
  for (const QStringList *list :
       {&actual.newFiles, &actual.modifiedFiles, &actual.deletedFiles,
        &actual.typeChangedFiles}) {
    QVERIFY(!list->contains(QStringLiteral("lockme.txt")));
    QVERIFY(!list->contains(QStringLiteral("sub/s.txt")));
  }
}

void SvnRepoTest::statusIgnoredFilesAreReported() {
  bool ok = false;
  svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("svn:ignore"),
              QStringLiteral("*.log"), QStringLiteral(".")},
        &ok);
  QVERIFY(ok);
  svnIn(_wc, {QStringLiteral("commit"), QStringLiteral("-m"),
              QStringLiteral("Ignore logs")},
        &ok);
  QVERIFY(ok);
  QVERIFY(writeFile(file("x.log"), "log\n"));
  QVERIFY(writeFile(file("x.o"), "object\n"));

  const SvnRepo::SvnStatus st = status();
  QVERIFY(st.newFiles.isEmpty());
  QCOMPARE(sorted(st.ignoredFiles),
           (QStringList{QStringLiteral("x.log"), QStringLiteral("x.o")}));
}

void SvnRepoTest::statusDirectoryPropertyChanges() {
  QVERIFY(writeFile(file("dir/a.txt"), "a\n"));
  QVERIFY(commitAll(_wc, QStringLiteral("Add dir")));
  svnIn(_wc, {QStringLiteral("update")});
  svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("owner"),
              QStringLiteral("me"), QStringLiteral("dir")});
  svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("owner"),
              QStringLiteral("me"), QStringLiteral(".")});
  const StatusMap oracle = svnStatusXml(_wc);
  QCOMPARE(oracle.value("dir").props, QStringLiteral("modified"));
  QCOMPARE(oracle.value(".").props, QStringLiteral("modified"));

  // The changes must be visible and committable like file changes.
  QVERIFY(status().hasChanges());
  const QString before = head();
  commitAll(_wc, QStringLiteral("Properties"));
  QCOMPARE(changedPaths(_repoDir, head()),
           (QStringList{QStringLiteral("_U trunk"),
                        QStringLiteral("_U trunk/dir")}));
  Q_UNUSED(before);
}

void SvnRepoTest::statusUnversionedDirectories() {
  QVERIFY(writeFile(file("dir/a.txt"), "a\n"));
  QVERIFY(writeFile(file("dir/deeper/b.txt"), "b\n"));
  QVERIFY(QDir().mkpath(file("empty/nested")));
  QVERIFY(writeFile(file("objects/x.o"), "ignored by default\n"));
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk"), file("nested")));
#ifdef Q_OS_WIN
  QVERIFY(QDir().mkpath(_base + QStringLiteral("/target")));
  QVERIFY(writeFile(_base + QStringLiteral("/target/t.txt"), "t\n"));
  QVERIFY(makeJunction(file("junction"), _base + QStringLiteral("/target")));
#endif

  // Files are listed one by one; empty folders and folders holding only
  // ignored files are not listed; a nested working copy and a link are one
  // entry each and are not entered.
  QStringList expected{QStringLiteral("dir/a.txt"),
                       QStringLiteral("dir/deeper/b.txt"),
                       QStringLiteral("nested")};
#ifdef Q_OS_WIN
  expected += QStringLiteral("junction");
#endif
  QCOMPARE(sorted(status().newFiles), sorted(expected));
}

void SvnRepoTest::statusExternals() {
  QVERIFY(!commitA("a\n", QStringLiteral("Add a")).isEmpty());
  bool ok = false;
  // The external is a copy of trunk elsewhere: trunk itself, inside trunk,
  // would include itself again and again.
  svn({QStringLiteral("copy"), QStringLiteral("-m"),
       QStringLiteral("Copy for the external"),
       _rootUrl + QStringLiteral("/trunk"), _rootUrl + QStringLiteral("/ext")},
      &ok);
  QVERIFY(ok);
  // A directory's properties can only be committed at HEAD.
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  QVERIFY(ok);
  svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("svn:externals"),
              _rootUrl + QStringLiteral("/ext ext\n") +
                  fileUrl(_base + QStringLiteral("/gone")) +
                  QStringLiteral(" missing"),
              QStringLiteral(".")},
        &ok);
  QVERIFY(ok);
  svnIn(_wc, {QStringLiteral("commit"), QStringLiteral("-m"),
              QStringLiteral("Externals")},
        &ok);
  QVERIFY(ok);
  // The update fails on the external whose repository doesn't exist.
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  QVERIFY(QFileInfo::exists(file("ext/a.txt")));
  QVERIFY(writeFile(file("ext/a.txt"), "changed in the external\n"));

  // An external is a working copy of its own, like a Git submodule: its
  // changes are not the parent's, and a broken external doesn't break status.
  resetLastError();
  const SvnRepo::SvnStatus st = status();
  QCOMPARE(lastError(), SvnRepo::SvnError::Unknown);
  QVERIFY2(!st.hasChanges(), qPrintable(describe(st)));
}

void SvnRepoTest::statusUserConfigGlobalIgnores() {
  QVERIFY(writeFile(file("x.tmp"), "tmp\n"));
  // The isolated configuration has Subversion's default global-ignores,
  // which don't include *.tmp.
  QCOMPARE(status().newFiles, QStringList{QStringLiteral("x.tmp")});

  QVERIFY(writeFile(configDir() + QStringLiteral("/config"),
                    "[miscellany]\nglobal-ignores = *.tmp\n"));
  QVERIFY(status().newFiles.isEmpty());
  QVERIFY(svnStatusXml(_wc).isEmpty());
}

void SvnRepoTest::statusUnusualFileNames_data() { unusualNames(); }

void SvnRepoTest::statusUnusualFileNames() {
  QFETCH(QString, name);

  QVERIFY(writeFile(file(name), "x\n"));
  QCOMPARE(status().newFiles, QStringList{name});
  QVERIFY2(SvnRepo::stageFiles(_wc, {name}), qPrintable(lastErrorMessage()));
  QCOMPARE(status().newStagedFiles, QStringList{name});
  QVERIFY2(SvnRepo::commitStaged(_wc, QStringLiteral("Add"), QString(),
                                 kAuthor, kEmail, true, false, false),
           qPrintable(lastErrorMessage()));
  QVERIFY(changedPaths(_repoDir, head())
              .contains(QStringLiteral("A trunk/") + name));
  QVERIFY(!status().hasChanges());

  QVERIFY(writeFile(file(name), "changed\n"));
  QCOMPARE(status().modifiedFiles, QStringList{name});
}

void SvnRepoTest::statusMixedLineEndings_data() {
  QTest::addColumn<QString>("eolStyle");
  QTest::addColumn<QByteArray>("content");
  for (const char *style : {"native", "LF", "CRLF", ""}) {
    for (const char *content : {"one\ntwo\n", "one\r\ntwo\r\n"}) {
      QTest::addRow("%s %s", *style ? style : "none",
                    QByteArray(content).contains('\r') ? "crlf" : "lf")
          << QString::fromLatin1(style) << QByteArray(content);
    }
  }
}

void SvnRepoTest::statusMixedLineEndings() {
  QFETCH(QString, eolStyle);
  QFETCH(QByteArray, content);

  QVERIFY(writeFile(file("a.txt"), "one\ntwo\n"));
  bool ok = false;
  svnIn(_wc, {QStringLiteral("add"), QStringLiteral("a.txt")}, &ok);
  QVERIFY(ok);
  if (!eolStyle.isEmpty()) {
    svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("svn:eol-style"),
                eolStyle, QStringLiteral("a.txt")},
          &ok);
    QVERIFY(ok);
  }
  svnIn(_wc, {QStringLiteral("commit"), QStringLiteral("-m"),
              QStringLiteral("Add")},
        &ok);
  QVERIFY(ok);

  // What counts as modified is what Subversion says.
  QVERIFY(writeFile(file("a.txt"), content));
  const bool oracleModified =
      svnStatusXml(_wc).value("a.txt").item == QLatin1String("modified");
  QCOMPARE(status().modifiedFiles.contains(QStringLiteral("a.txt")),
           oracleModified);
}

void SvnRepoTest::statusKeywords() {
  QVERIFY(writeFile(file("k.txt"), "$Id$\n$Rev$\n"));
  bool ok = false;
  svnIn(_wc, {QStringLiteral("add"), QStringLiteral("k.txt")}, &ok);
  svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("svn:keywords"),
              QStringLiteral("Id Rev"), QStringLiteral("k.txt")},
        &ok);
  QVERIFY(ok);
  svnIn(_wc, {QStringLiteral("commit"), QStringLiteral("-m"),
              QStringLiteral("Keywords")},
        &ok);
  QVERIFY(ok);
  QVERIFY(readFile(file("k.txt")).contains("$Rev: 2 $"));

  // An expanded keyword is not a change.
  QVERIFY(!status().hasChanges());
}

void SvnRepoTest::statusNeedsLock() {
  QVERIFY(writeFile(file("n.txt"), "n\n"));
  bool ok = false;
  svnIn(_wc, {QStringLiteral("add"), QStringLiteral("n.txt")}, &ok);
  svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("svn:needs-lock"),
              QStringLiteral("*"), QStringLiteral("n.txt")},
        &ok);
  svnIn(_wc, {QStringLiteral("commit"), QStringLiteral("-m"),
              QStringLiteral("Needs lock")},
        &ok);
  QVERIFY(ok);
  QVERIFY(!QFileInfo(file("n.txt")).isWritable());

  // Another working copy holds the lock.
  const QString other = _base + QStringLiteral("/other");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk"), other));
  svnIn(other, {QStringLiteral("lock"), QStringLiteral("n.txt")}, &ok);
  QVERIFY(ok);

  resetLastError();
  QVERIFY(!status().hasChanges());
  QCOMPARE(lastError(), SvnRepo::SvnError::Unknown);
}

void SvnRepoTest::statusOtherChangelist() {
  QVERIFY(!commitA("a\n", QStringLiteral("Add a")).isEmpty());
  QVERIFY(writeFile(file("a.txt"), "changed\n"));
  bool ok = false;
  svnIn(_wc, {QStringLiteral("changelist"), QStringLiteral("mine"),
              QStringLiteral("a.txt")},
        &ok);
  QVERIFY(ok);

  // A user's own changelist is not the staging area, and status leaves it.
  QCOMPARE(status().modifiedFiles, QStringList{QStringLiteral("a.txt")});
  QVERIFY(status().modifiedStagedFiles.isEmpty());
  QCOMPARE(svnStatusXml(_wc).value("a.txt").changelist, QStringLiteral("mine"));
}

void SvnRepoTest::statusSorted() {
  for (const char *name : {"z.txt", "a.txt", "M.txt", "dir/b.txt", "b.txt",
                           "dir/A.txt", "_x.txt"}) {
    QVERIFY(writeFile(file(QString::fromLatin1(name)), "x\n"));
  }
  const QStringList first = status().newFiles;
  QCOMPARE(first, sorted(first));
  QCOMPARE(status().newFiles, first);
}

void SvnRepoTest::statusCancelled() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));

  // Cancelled before it starts.
  _quit = true;
  resetLastError();
  const SvnRepo::SvnStatus st = status();
  QVERIFY(!st.hasChanges());
  QCOMPARE(lastError(), SvnRepo::SvnError::Cancelled);
  _quit = false;

  // Cancelled from another thread while it runs.
  for (int i = 0; i < 3000; ++i) {
    QVERIFY(writeFile(file(QStringLiteral("many/f%1.txt").arg(i)), "x\n"));
  }
  bool quit = false;
  std::thread canceller([&]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    quit = true;
  });
  QElapsedTimer timer;
  timer.start();
  SvnRepo::getStatus(_wc, quit);
  const qint64 elapsed = timer.elapsed();
  canceller.join();
  QVERIFY2(elapsed < 5000, qPrintable(QString::number(elapsed)));
}

void SvnRepoTest::statusManyFiles() {
  for (int i = 0; i < 3000; ++i) {
    QVERIFY(writeFile(file(QStringLiteral("d%1/f%2.txt").arg(i % 30).arg(i)),
                      "x\n"));
  }
  QElapsedTimer timer;
  timer.start();
  const SvnRepo::SvnStatus st = status();
  const qint64 elapsed = timer.elapsed();
  QCOMPARE(st.newFiles.size(), 3000);
  QVERIFY2(elapsed < 10000, qPrintable(QString::number(elapsed)));
}

void SvnRepoTest::statusFailureIsNotClean() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  makeWritable(_wc);
  QVERIFY(QDir(_wc + QStringLiteral("/.svn")).removeRecursively());

  // A status that failed must be told apart from a clean one.
  resetLastError();
  const SvnRepo::SvnStatus st = status();
  EXPECT_SVN_ERROR(SvnRepo::SvnError::StatusFailed);
  QVERIFY(st.path.isEmpty());
}

// --- 3.3 Staging ------------------------------------------------------------

void SvnRepoTest::unstagedChangesAreNeverCommitted_data() {
  QTest::addColumn<QString>("operation");
  QTest::newRow("commitStaged") << QStringLiteral("commitStaged");
  QTest::newRow("merge") << QStringLiteral("merge");
  QTest::newRow("cherryPickCommit") << QStringLiteral("cherryPickCommit");
  QTest::newRow("revertCommit") << QStringLiteral("revertCommit");
  QTest::newRow("resolved conflict") << QStringLiteral("resolved conflict");
}

void SvnRepoTest::unstagedChangesAreNeverCommitted() {
  QFETCH(QString, operation);

  for (const char *name : {"op.txt", "keep.txt", "gone.txt", "prop.txt"}) {
    QVERIFY(writeFile(file(QString::fromLatin1(name)), "base\n"));
  }
  QVERIFY(commitAll(_wc, QStringLiteral("Base")));
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  QVERIFY(ok);

  // The change each operation brings in, made before the local changes.
  QString revision;
  if (operation == QLatin1String("merge") ||
      operation == QLatin1String("cherryPickCommit")) {
    svn({QStringLiteral("copy"), QStringLiteral("-m"),
         QStringLiteral("Branch"), _rootUrl + QStringLiteral("/trunk"),
         _rootUrl + QStringLiteral("/branches/feature")},
        &ok);
    QVERIFY(ok);
    const QString feature = _base + QStringLiteral("/feature");
    QVERIFY(secondWc(_rootUrl + QStringLiteral("/branches/feature"), feature));
    QVERIFY(writeFile(feature + QStringLiteral("/op.txt"), "feature\n"));
    svnIn(feature, {QStringLiteral("commit"), QStringLiteral("-m"),
                    QStringLiteral("Feature")},
          &ok);
    QVERIFY(ok);
    revision = head();
  } else if (operation == QLatin1String("revertCommit")) {
    QVERIFY(writeFile(file("op.txt"), "to revert\n"));
    QVERIFY(commitAll(_wc, QStringLiteral("To revert")));
    revision = head();
    svnIn(_wc, {QStringLiteral("update")}, &ok);
  } else if (operation == QLatin1String("resolved conflict")) {
    const QString other = _base + QStringLiteral("/other");
    QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk"), other));
    QVERIFY(writeFile(other + QStringLiteral("/op.txt"), "theirs\n"));
    svnIn(other, {QStringLiteral("commit"), QStringLiteral("-m"),
                  QStringLiteral("Theirs")},
          &ok);
    QVERIFY(ok);
  }

  // Unstaged work on files the operation doesn't touch.
  QVERIFY(writeFile(file("keep.txt"), "unstaged edit\n"));
  svnIn(_wc, {QStringLiteral("delete"), QStringLiteral("gone.txt")}, &ok);
  QVERIFY(ok);
  svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("p"),
              QStringLiteral("v"), QStringLiteral("prop.txt")},
        &ok);
  QVERIFY(ok);
  QVERIFY(writeFile(file("untracked.txt"), "untracked\n"));
  const QString before = head();

  QStringList expected{QStringLiteral("U trunk/op.txt")};
  if (operation == QLatin1String("commitStaged")) {
    QVERIFY(writeFile(file("op.txt"), "staged\n"));
    QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("op.txt")}));
    QVERIFY2(SvnRepo::commitStaged(_wc, QStringLiteral("Staged"), QString(),
                                   kAuthor, kEmail, true, false, false),
             qPrintable(lastErrorMessage()));
  } else if (operation == QLatin1String("merge")) {
    QVERIFY2(SvnRepo::merge(_wc, QStringLiteral("feature"),
                            QStringLiteral("trunk"), kAuthor, kEmail),
             qPrintable(lastErrorMessage()));
    expected += QStringLiteral("_U trunk");
  } else if (operation == QLatin1String("cherryPickCommit")) {
    QVERIFY2(SvnRepo::cherryPickCommit(_wc, revision, kAuthor, kEmail),
             qPrintable(lastErrorMessage()));
    expected += QStringLiteral("_U trunk");
  } else if (operation == QLatin1String("revertCommit")) {
    QVERIFY2(SvnRepo::revertCommit(_wc, revision, kAuthor, kEmail, QString()),
             qPrintable(lastErrorMessage()));
  } else {
    QVERIFY(writeFile(file("op.txt"), "mine\n"));
    svnIn(_wc, {QStringLiteral("update"), QStringLiteral("--accept"),
                QStringLiteral("postpone")});
    QVERIFY(SvnRepo::hasConflicts(_wc));
    QVERIFY(writeFile(file("op.txt"), "resolved\n"));
    QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("op.txt")}));
    QVERIFY2(SvnRepo::commitStaged(_wc, QStringLiteral("Resolved"), QString(),
                                   kAuthor, kEmail, true, false, false),
             qPrintable(lastErrorMessage()));
  }
  std::sort(expected.begin(), expected.end());

  // Exactly one revision with the operation's change.
  QCOMPARE(head().toInt(), before.toInt() + 1);
  const QStringList changed = changedPaths(_repoDir, head());
  QCOMPARE(changed, expected);

  // The unstaged work is all still there, unstaged.
  QCOMPARE(readFile(file("keep.txt")), QByteArray("unstaged edit\n"));
  QCOMPARE(readFile(file("untracked.txt")), QByteArray("untracked\n"));
  const StatusMap oracle = svnStatusXml(_wc);
  QCOMPARE(oracle.value("keep.txt").item, QStringLiteral("modified"));
  QCOMPARE(oracle.value("gone.txt").item, QStringLiteral("deleted"));
  QCOMPARE(oracle.value("prop.txt").props, QStringLiteral("modified"));
  QCOMPARE(oracle.value("untracked.txt").item, QStringLiteral("unversioned"));
  QVERIFY(changelistMembers(oracle).isEmpty());
}

void SvnRepoTest::stageUnstageRoundTrip_data() {
  QTest::addColumn<QString>("spelling");
  QTest::newRow("relative") << QStringLiteral("a.txt");
  QTest::newRow("dot segments") << QStringLiteral("./x/../a.txt");
  QTest::newRow("backslashes") << QStringLiteral("sub\\a.txt");
  QTest::newRow("absolute") << QStringLiteral("<wc>/a.txt");
  QTest::newRow("absolute native") << QStringLiteral("<native>");
#ifdef Q_OS_WIN
  QTest::newRow("lowercase drive") << QStringLiteral("<lower>");
#endif
}

void SvnRepoTest::stageUnstageRoundTrip() {
  QFETCH(QString, spelling);

  const QString relative = spelling.startsWith(QLatin1String("sub"))
                               ? QStringLiteral("sub/a.txt")
                               : QStringLiteral("a.txt");
  QVERIFY(writeFile(file(relative), "a\n"));
  QString path = spelling;
  if (spelling == QLatin1String("<wc>/a.txt")) {
    path = file("a.txt");
  } else if (spelling == QLatin1String("<native>")) {
    path = QDir::toNativeSeparators(file("a.txt"));
  } else if (spelling == QLatin1String("<lower>")) {
    path = file("a.txt");
    path[0] = path[0].toLower();
  }

  for (int round = 0; round < 2; ++round) {
    QVERIFY2(SvnRepo::stageFiles(_wc, {path}), qPrintable(lastErrorMessage()));
    QCOMPARE(changelistMembers(svnStatusXml(_wc)), QStringList{relative});
    QCOMPARE(status().newStagedFiles, QStringList{relative});
    QVERIFY2(SvnRepo::unstageFiles(_wc, {path}),
             qPrintable(lastErrorMessage()));
    QVERIFY(changelistMembers(svnStatusXml(_wc)).isEmpty());
    QCOMPARE(svnStatusXml(_wc).value(relative).item, QStringLiteral("added"));
  }
}

void SvnRepoTest::stageModifiedDeletedMissing() {
  for (const char *name : {"m.txt", "d.txt", "x.txt"}) {
    QVERIFY(writeFile(file(QString::fromLatin1(name)), "base\n"));
  }
  QVERIFY(commitAll(_wc, QStringLiteral("Base")));
  QVERIFY(writeFile(file("m.txt"), "changed\n"));
  bool ok = false;
  svnIn(_wc, {QStringLiteral("delete"), QStringLiteral("d.txt")}, &ok);
  QVERIFY(ok);
  QVERIFY(QFile::remove(file("x.txt")));
  QCOMPARE(svnStatusXml(_wc).value("x.txt").item, QStringLiteral("missing"));

  QVERIFY2(SvnRepo::stageFiles(_wc, {QStringLiteral("m.txt"),
                                     QStringLiteral("d.txt"),
                                     QStringLiteral("x.txt")}),
           qPrintable(lastErrorMessage()));
  const SvnRepo::SvnStatus st = status();
  QCOMPARE(st.modifiedStagedFiles, QStringList{QStringLiteral("m.txt")});
  QCOMPARE(sorted(st.deletedStagedFiles),
           (QStringList{QStringLiteral("d.txt"), QStringLiteral("x.txt")}));
  // The missing file is scheduled for deletion.
  QCOMPARE(svnStatusXml(_wc).value("x.txt").item, QStringLiteral("deleted"));

  QVERIFY2(SvnRepo::commitStaged(_wc, QStringLiteral("Stage all kinds"),
                                 QString(), kAuthor, kEmail, true, false,
                                 false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(changedPaths(_repoDir, head()),
           (QStringList{QStringLiteral("D trunk/d.txt"),
                        QStringLiteral("D trunk/x.txt"),
                        QStringLiteral("U trunk/m.txt")}));
}

void SvnRepoTest::stageDirectory() {
  QVERIFY(writeFile(file("dir/a.txt"), "a\n"));
  QVERIFY(writeFile(file("dir/b.txt"), "b\n"));
  QVERIFY(writeFile(file("other.txt"), "o\n"));
  QVERIFY(commitAll(_wc, QStringLiteral("Base")));
  QVERIFY(writeFile(file("dir/a.txt"), "changed\n"));
  QVERIFY(writeFile(file("dir/c.txt"), "new\n"));
  QVERIFY(writeFile(file("other.txt"), "changed\n"));

  // Every changed file below the folder is staged, nothing else.
  QVERIFY2(SvnRepo::stageFiles(_wc, {QStringLiteral("dir")}),
           qPrintable(lastErrorMessage()));
  SvnRepo::SvnStatus st = status();
  QCOMPARE(st.modifiedStagedFiles, QStringList{QStringLiteral("dir/a.txt")});
  QCOMPARE(st.newStagedFiles, QStringList{QStringLiteral("dir/c.txt")});
  QCOMPARE(st.modifiedFiles, QStringList{QStringLiteral("other.txt")});

  // Documented: the unchanged file joined the changelist too, so a later
  // edit of it shows up as staged.
  QVERIFY(changelistMembers(svnStatusXml(_wc, true))
              .contains(QStringLiteral("dir/b.txt")));
  QVERIFY(writeFile(file("dir/b.txt"), "later\n"));
  QVERIFY(status().modifiedStagedFiles.contains(QStringLiteral("dir/b.txt")));
}

void SvnRepoTest::stageEmptyAddedDirectory() {
  QVERIFY(QDir().mkpath(file("empty")));
  bool ok = false;
  svnIn(_wc, {QStringLiteral("add"), QStringLiteral("empty")}, &ok);
  QVERIFY(ok);

  resetLastError();
  SvnRepo::stageFiles(_wc, {QStringLiteral("empty")});
  const bool committed =
      SvnRepo::commitStaged(_wc, QStringLiteral("Empty folder"), QString(),
                            kAuthor, kEmail, true, false, false);
  QVERIFY2(committed, qPrintable(lastErrorMessage()));
  QCOMPARE(changedPaths(_repoDir, head()),
           QStringList{QStringLiteral("A trunk/empty")});
}

void SvnRepoTest::stageCopiedDirectory() {
  QVERIFY(writeFile(file("dir/a.txt"), "a\n"));
  QVERIFY(commitAll(_wc, QStringLiteral("Base")));
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  svnIn(_wc, {QStringLiteral("copy"), QStringLiteral("dir"),
              QStringLiteral("copy")},
        &ok);
  QVERIFY(ok);

  resetLastError();
  SvnRepo::stageFiles(_wc, {QStringLiteral("copy")});
  const bool committed =
      SvnRepo::commitStaged(_wc, QStringLiteral("Copied folder"), QString(),
                            kAuthor, kEmail, true, false, false);
  QVERIFY2(committed, qPrintable(lastErrorMessage()));
  QCOMPARE(changedPaths(_repoDir, head()),
           QStringList{QStringLiteral("A trunk/copy")});
}

void SvnRepoTest::stageInsideUnversionedParents() {
  QVERIFY(writeFile(file("dir/sub/b.txt"), "b\n"));
  QVERIFY(writeFile(file("dir/sub/c.txt"), "c\n"));
  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("dir/sub/b.txt")}));

  // The parents are added, only the file is staged.
  StatusMap oracle = svnStatusXml(_wc);
  QCOMPARE(oracle.value("dir").item, QStringLiteral("added"));
  QCOMPARE(oracle.value("dir/sub").item, QStringLiteral("added"));
  QCOMPARE(changelistMembers(oracle),
           QStringList{QStringLiteral("dir/sub/b.txt")});
  QCOMPARE(oracle.value("dir/sub/c.txt").item, QStringLiteral("unversioned"));

  // Unstaging leaves the parents added.
  QVERIFY(SvnRepo::unstageFiles(_wc, {QStringLiteral("dir/sub/b.txt")}));
  oracle = svnStatusXml(_wc);
  QCOMPARE(oracle.value("dir/sub").item, QStringLiteral("added"));
  QCOMPARE(oracle.value("dir/sub/b.txt").item, QStringLiteral("added"));
  QVERIFY(changelistMembers(oracle).isEmpty());
}

void SvnRepoTest::stageIgnoredFile() {
  QVERIFY(writeFile(file("x.o"), "object\n"));

  // Staging an ignored file either stages it or says why not; it must not
  // report success and do nothing.
  resetLastError();
  const bool staged = SvnRepo::stageFiles(_wc, {QStringLiteral("x.o")});
  if (staged) {
    QCOMPARE(status().newStagedFiles, QStringList{QStringLiteral("x.o")});
  } else {
    QVERIFY(!lastErrorMessage().isEmpty());
  }
}

void SvnRepoTest::stageConflictedFile_data() {
  QTest::addColumn<QString>("kind");
  QTest::newRow("text with markers") << QStringLiteral("text");
  QTest::newRow("text resolved") << QStringLiteral("resolved");
  QTest::newRow("property") << QStringLiteral("property");
  QTest::newRow("tree") << QStringLiteral("tree");
}

void SvnRepoTest::stageConflictedFile() {
  QFETCH(QString, kind);

  QVERIFY(!commitA("base\n", QStringLiteral("Base")).isEmpty());
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  const QString other = _base + QStringLiteral("/other");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk"), other));
  if (kind == QLatin1String("property")) {
    svnIn(other, {QStringLiteral("propset"), QStringLiteral("p"),
                  QStringLiteral("theirs"), QStringLiteral("a.txt")});
    svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("p"),
                QStringLiteral("mine"), QStringLiteral("a.txt")});
  } else if (kind == QLatin1String("tree")) {
    svnIn(other, {QStringLiteral("delete"), QStringLiteral("a.txt")});
    QVERIFY(writeFile(file("a.txt"), "mine\n"));
  } else {
    QVERIFY(writeFile(other + QStringLiteral("/a.txt"), "theirs\n"));
    QVERIFY(writeFile(file("a.txt"), "mine\n"));
  }
  svnIn(other, {QStringLiteral("commit"), QStringLiteral("-m"),
                QStringLiteral("Theirs")},
        &ok);
  QVERIFY(ok);
  svnIn(_wc, {QStringLiteral("update"), QStringLiteral("--accept"),
              QStringLiteral("postpone")});
  QVERIFY(SvnRepo::hasConflicts(_wc));

  if (kind == QLatin1String("resolved")) {
    // Without markers, staging is how the user marks the conflict resolved.
    QVERIFY(writeFile(file("a.txt"), "resolved\n"));
    QVERIFY2(SvnRepo::stageFiles(_wc, {QStringLiteral("a.txt")}),
             qPrintable(lastErrorMessage()));
    QVERIFY(!SvnRepo::hasConflicts(_wc));
    QCOMPARE(status().modifiedStagedFiles,
             QStringList{QStringLiteral("a.txt")});
    return;
  }

  // A file that still has markers or an unresolved property or tree conflict
  // is not silently resolved: staging fails, or the conflict stays.
  resetLastError();
  const bool staged = SvnRepo::stageFiles(_wc, {QStringLiteral("a.txt")});
  const bool stillConflicted = SvnRepo::hasConflicts(_wc);
  QVERIFY2(stillConflicted || !staged, qPrintable(lastErrorMessage()));
  if (!staged) {
    QVERIFY(!lastErrorMessage().isEmpty());
  }
}

void SvnRepoTest::stageMovesFileFromUserChangelist() {
  QVERIFY(!commitA("a\n", QStringLiteral("Add a")).isEmpty());
  QVERIFY(writeFile(file("a.txt"), "changed\n"));
  bool ok = false;
  svnIn(_wc, {QStringLiteral("changelist"), QStringLiteral("mine"),
              QStringLiteral("a.txt")},
        &ok);
  QVERIFY(ok);

  // Documented: staging moves the file from the user's changelist into the
  // staging changelist; a file can only be in one (B22).
  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("a.txt")}));
  QCOMPARE(svnStatusXml(_wc).value("a.txt").changelist, kStagingChangelist);
}

void SvnRepoTest::stageNothing() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(SvnRepo::stageFiles(_wc, {}));
  QVERIFY(SvnRepo::unstageFiles(_wc, {}));
  QCOMPARE(status().newFiles, QStringList{QStringLiteral("a.txt")});
  QCOMPARE(svnStatusXml(_wc).value("a.txt").item,
           QStringLiteral("unversioned"));
}

void SvnRepoTest::stageNonexistentAndOutsidePaths_data() {
  QTest::addColumn<QString>("kind");
  QTest::newRow("missing file") << QStringLiteral("missing");
  QTest::newRow("outside") << QStringLiteral("outside");
  QTest::newRow("other working copy") << QStringLiteral("other wc");
}

void SvnRepoTest::stageNonexistentAndOutsidePaths() {
  QFETCH(QString, kind);

  QVERIFY(writeFile(_base + QStringLiteral("/outside.txt"), "o\n"));
  const QString other = _base + QStringLiteral("/other");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk"), other));
  QVERIFY(writeFile(other + QStringLiteral("/theirs.txt"), "t\n"));

  QString path;
  if (kind == QLatin1String("missing")) {
    path = QStringLiteral("no-such-file.txt");
  } else if (kind == QLatin1String("outside")) {
    path = QStringLiteral("../outside.txt");
  } else {
    path = other + QStringLiteral("/theirs.txt");
  }

  resetLastError();
  const bool staged = SvnRepo::stageFiles(_wc, {path});
  QVERIFY(!staged);
  if (!staged) {
    QVERIFY(!lastErrorMessage().isEmpty());
    QVERIFY2(lastError() == SvnRepo::SvnError::AddFailed ||
                 lastError() == SvnRepo::SvnError::InvalidArgument,
             qPrintable(QString::number(static_cast<int>(lastError()))));
  }
  // Nothing changed here or in the other working copy.
  QVERIFY(svnStatusXml(_wc).isEmpty());
  QCOMPARE(svnStatusXml(other).value("theirs.txt").item,
           QStringLiteral("unversioned"));
}

void SvnRepoTest::stageFileInsideIgnoredDirectory() {
  bool ok = false;
  svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("svn:ignore"),
              QStringLiteral("build"), QStringLiteral(".")},
        &ok);
  svnIn(_wc, {QStringLiteral("commit"), QStringLiteral("-m"),
              QStringLiteral("Ignore build")},
        &ok);
  QVERIFY(ok);
  QVERIFY(writeFile(file("build/out.txt"), "out\n"));
  QVERIFY(status().newFiles.isEmpty());

  // Like an ignored file: staged, or refused with a reason.
  resetLastError();
  const bool staged =
      SvnRepo::stageFiles(_wc, {QStringLiteral("build/out.txt")});
  if (staged) {
    QCOMPARE(status().newStagedFiles,
             QStringList{QStringLiteral("build/out.txt")});
  } else {
    QVERIFY(!lastErrorMessage().isEmpty());
  }
}

void SvnRepoTest::unstageKeepsAddAndDelete() {
  QVERIFY(!commitA("a\n", QStringLiteral("Add a")).isEmpty());
  QVERIFY(writeFile(file("new.txt"), "new\n"));
  bool ok = false;
  svnIn(_wc, {QStringLiteral("delete"), QStringLiteral("a.txt")}, &ok);
  QVERIFY(ok);
  QVERIFY(SvnRepo::stageFiles(
      _wc, {QStringLiteral("new.txt"), QStringLiteral("a.txt")}));
  QVERIFY(SvnRepo::unstageFiles(
      _wc, {QStringLiteral("new.txt"), QStringLiteral("a.txt")}));

  const SvnRepo::SvnStatus st = status();
  QCOMPARE(st.newFiles, QStringList{QStringLiteral("new.txt")});
  QCOMPARE(st.deletedFiles, QStringList{QStringLiteral("a.txt")});
  const StatusMap oracle = svnStatusXml(_wc);
  QCOMPARE(oracle.value("new.txt").item, QStringLiteral("added"));
  QCOMPARE(oracle.value("a.txt").item, QStringLiteral("deleted"));
  QVERIFY(changelistMembers(oracle).isEmpty());
}

void SvnRepoTest::unstageUnversionedOrUnstagedFile() {
  QVERIFY(!commitA("a\n", QStringLiteral("Add a")).isEmpty());
  QVERIFY(writeFile(file("a.txt"), "changed\n"));
  QVERIFY(writeFile(file("new.txt"), "new\n"));

  // Nothing to unstage: no error, or one that explains it; never Subversion's
  // raw "node not found".
  for (const QString &name :
       {QStringLiteral("a.txt"), QStringLiteral("new.txt")}) {
    resetLastError();
    if (!SvnRepo::unstageFiles(_wc, {name})) {
      QVERIFY(!lastErrorMessage().isEmpty());
      QVERIFY2(SvnRepo::getLastError().errorCode != 155010,
               qPrintable(lastErrorMessage()));
    }
  }
  QCOMPARE(status().modifiedFiles, QStringList{QStringLiteral("a.txt")});
  QCOMPARE(status().newFiles, QStringList{QStringLiteral("new.txt")});
}

// --- 3.4 Commit and sign-off -------------------------------------------------

void SvnRepoTest::commitStagedOnly() {
  QVERIFY(writeFile(file("b.txt"), "b\n"));
  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
  QVERIFY(writeFile(file("a.txt"), "staged\n"));
  QVERIFY(writeFile(file("b.txt"), "unstaged\n"));
  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("a.txt")}));

  QVERIFY2(SvnRepo::commitStaged(_wc, QStringLiteral("Only a"), QString(),
                                 kAuthor, kEmail, true, false, false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(changedPaths(_repoDir, head()),
           QStringList{QStringLiteral("U trunk/a.txt")});
  QCOMPARE(status().modifiedFiles, QStringList{QStringLiteral("b.txt")});
  // The committed file left the changelist.
  QVERIFY(changelistMembers(svnStatusXml(_wc, true)).isEmpty());
}

void SvnRepoTest::commitStagedDeletedDirectory() {
  QVERIFY(writeFile(file("dir/a.txt"), "a\n"));
  QVERIFY(writeFile(file("dir/sub/b.txt"), "b\n"));
  QVERIFY(commitAll(_wc, QStringLiteral("Base")));
  const QString base = head();
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  svnIn(_wc, {QStringLiteral("delete"), QStringLiteral("dir")}, &ok);
  QVERIFY(ok);

  QVERIFY(SvnRepo::stageFiles(_wc, status().deletedFiles));
  resetLastError();
  const bool committed =
      SvnRepo::commitStaged(_wc, QStringLiteral("Delete dir"), QString(),
                            kAuthor, kEmail, true, false, false);
  QVERIFY2(committed, qPrintable(lastErrorMessage()));
  QCOMPARE(changedPaths(_repoDir, head()),
           QStringList{QStringLiteral("D trunk/dir")});

  // The staged files below the deleted directory leave no changelist behind:
  // bringing the directory back would read it as "Corrupt data".
  const QString output =
      svnIn(_wc, {QStringLiteral("update"), QStringLiteral("-r"), base}, &ok);
  QVERIFY2(ok, qPrintable(output));
  QCOMPARE(readFile(file("dir/sub/b.txt")), QByteArray("b\n"));
}

void SvnRepoTest::revertCommitOfAddedDirectory() {
  QVERIFY(writeFile(file("dir/a.txt"), "a\n"));
  QVERIFY(writeFile(file("dir/sub/b.txt"), "b\n"));
  QVERIFY(commitAll(_wc, QStringLiteral("Add dir")));
  const QString added = head();
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  QVERIFY(ok);

  // The revert stages and commits the deletion of dir.
  resetLastError();
  QVERIFY2(SvnRepo::revertCommit(_wc, added, kAuthor, kEmail, QString()),
           qPrintable(lastErrorMessage()));
  QCOMPARE(changedPaths(_repoDir, head()),
           QStringList{QStringLiteral("D trunk/dir")});
  QVERIFY(!QFile::exists(file("dir")));

  const QString output =
      svnIn(_wc, {QStringLiteral("update"), QStringLiteral("-r"), added}, &ok);
  QVERIFY2(ok, qPrintable(output));
  QCOMPARE(readFile(file("dir/sub/b.txt")), QByteArray("b\n"));
}

void SvnRepoTest::commitStagedReplacedDirectory() {
  QVERIFY(writeFile(file("dir/old.txt"), "old\n"));
  QVERIFY(commitAll(_wc, QStringLiteral("Base")));
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  svnIn(_wc, {QStringLiteral("delete"), QStringLiteral("dir")}, &ok);
  QVERIFY(ok);
  QVERIFY(writeFile(file("dir/new.txt"), "new\n"));
  svnIn(_wc, {QStringLiteral("add"), QStringLiteral("dir")}, &ok);
  QVERIFY(ok);
  QCOMPARE(svnStatusXml(_wc).value("dir").item, QStringLiteral("replaced"));

  const SvnRepo::SvnStatus st = status();
  QVERIFY(SvnRepo::stageFiles(_wc, st.newFiles + st.deletedFiles +
                                       st.typeChangedFiles));
  resetLastError();
  const bool committed =
      SvnRepo::commitStaged(_wc, QStringLiteral("Replace dir"), QString(),
                            kAuthor, kEmail, true, false, false);
  QVERIFY2(committed, qPrintable(lastErrorMessage()));
  QCOMPARE(changedPaths(_repoDir, head()),
           (QStringList{QStringLiteral("A trunk/dir/new.txt"),
                        QStringLiteral("R trunk/dir")}));
}

void SvnRepoTest::commitStagedAddedTree() {
  for (const char *name : {"a/b/c.txt", "a/b/d.txt", "a/e.txt"}) {
    QVERIFY(writeFile(file(QString::fromLatin1(name)), "x\n"));
  }
  QVERIFY(SvnRepo::stageFiles(_wc, status().newFiles));
  QVERIFY2(SvnRepo::commitStaged(_wc, QStringLiteral("Tree"), QString(),
                                 kAuthor, kEmail, true, false, false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(changedPaths(_repoDir, head()),
           (QStringList{QStringLiteral("A trunk/a"),
                        QStringLiteral("A trunk/a/b"),
                        QStringLiteral("A trunk/a/b/c.txt"),
                        QStringLiteral("A trunk/a/b/d.txt"),
                        QStringLiteral("A trunk/a/e.txt")}));
  QVERIFY(!status().hasChanges());
}

void SvnRepoTest::commitStagedMoveBothSides() {
  QVERIFY(writeFile(file("src.txt"), "content\n"));
  QVERIFY(commitAll(_wc, QStringLiteral("Base")));
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  svnIn(_wc, {QStringLiteral("move"), QStringLiteral("src.txt"),
              QStringLiteral("dst.txt")},
        &ok);
  QVERIFY(ok);

  QVERIFY(SvnRepo::stageFiles(
      _wc, {QStringLiteral("dst.txt"), QStringLiteral("src.txt")}));
  QVERIFY2(SvnRepo::commitStaged(_wc, QStringLiteral("Move"), QString(),
                                 kAuthor, kEmail, true, false, false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(changedPaths(_repoDir, head()),
           (QStringList{QStringLiteral("A trunk/dst.txt"),
                        QStringLiteral("D trunk/src.txt")}));
  QVERIFY(!status().hasChanges());
}

void SvnRepoTest::commitStagedMoveOneSide() {
  QVERIFY(writeFile(file("src.txt"), "content\n"));
  QVERIFY(commitAll(_wc, QStringLiteral("Base")));
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  svnIn(_wc, {QStringLiteral("move"), QStringLiteral("src.txt"),
              QStringLiteral("dst.txt")},
        &ok);
  QVERIFY(ok);
  const QString before = head();

  // Only the new side is staged: both sides are committed, or the commit is
  // refused with a clear error and nothing changes (B15).
  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("dst.txt")}));
  resetLastError();
  if (SvnRepo::commitStaged(_wc, QStringLiteral("Move"), QString(), kAuthor,
                            kEmail, true, false, false)) {
    QCOMPARE(changedPaths(_repoDir, head()),
             (QStringList{QStringLiteral("A trunk/dst.txt"),
                          QStringLiteral("D trunk/src.txt")}));
  } else {
    EXPECT_SVN_ERROR(SvnRepo::SvnError::CommitFailed);
    QVERIFY2(lastErrorMessage().contains(QStringLiteral("move"),
                                         Qt::CaseInsensitive),
             qPrintable(lastErrorMessage()));
    QCOMPARE(head(), before);
    QCOMPARE(changelistMembers(svnStatusXml(_wc)),
             QStringList{QStringLiteral("dst.txt")});
  }
}

void SvnRepoTest::commitWithoutStagedFiles() {
  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
  const QString before = head();
  QVERIFY(writeFile(file("a.txt"), "unstaged\n"));
  QVERIFY(writeFile(file("new.txt"), "new\n"));

  resetLastError();
  QVERIFY(!SvnRepo::commitStaged(_wc, QStringLiteral("Nothing"), QString(),
                                 kAuthor, kEmail, true, false, false));
  EXPECT_SVN_ERROR(SvnRepo::SvnError::CommitFailed);
  QCOMPARE(head(), before);
  QCOMPARE(status().modifiedFiles, QStringList{QStringLiteral("a.txt")});
}

void SvnRepoTest::commitStagedFileRevertedExternally() {
  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
  const QString before = head();
  QVERIFY(writeFile(file("a.txt"), "staged\n"));
  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("a.txt")}));
  bool ok = false;
  svnIn(_wc, {QStringLiteral("revert"), QStringLiteral("a.txt")}, &ok);
  QVERIFY(ok);

  // Nothing is left to commit: no success without a revision.
  resetLastError();
  const bool committed =
      SvnRepo::commitStaged(_wc, QStringLiteral("Gone"), QString(), kAuthor,
                            kEmail, true, false, false);
  QVERIFY(!committed);
  EXPECT_SVN_ERROR(SvnRepo::SvnError::CommitFailed);
  QCOMPARE(head(), before);
}

void SvnRepoTest::commitMessageComposition_data() {
  QTest::addColumn<QString>("summary");
  QTest::addColumn<QString>("message");
  QTest::addColumn<bool>("cleanup");
  // The svn:log stored; null when the commit must fail.
  QTest::addColumn<QByteArray>("expected");

  QTest::newRow("summary only")
      << QStringLiteral("Fix") << QString() << true << QByteArray("Fix");
  QTest::newRow("summary and body") << QStringLiteral("Fix")
                                    << QStringLiteral("Body") << true
                                    << QByteArray("Fix\n\nBody");
  QTest::newRow("multi-line body")
      << QStringLiteral("Fix") << QStringLiteral("Line 1\nLine 2") << true
      << QByteArray("Fix\n\nLine 1\nLine 2");
  QTest::newRow("trailing whitespace")
      << QStringLiteral("Fix  ") << QStringLiteral("Body \t\n") << true
      << QByteArray("Fix\n\nBody");
  QTest::newRow("unicode") << QString::fromUtf16(u"Fïx 日本")
                           << QString() << true
                           << QByteArray("F\xc3\xafx \xe6\x97\xa5\xe6\x9c\xac");
  QTest::newRow("comment cleanup on")
      << QStringLiteral("Fix") << QStringLiteral("# comment\nBody") << true
      << QByteArray("Fix\n\nBody");
  QTest::newRow("comment cleanup off")
      << QStringLiteral("Fix") << QStringLiteral("# comment\nBody") << false
      << QByteArray("Fix\n\n# comment\nBody");
  QTest::newRow("issue number summary")
      << QStringLiteral("#42 fixes") << QString() << true
      << QByteArray("#42 fixes");
  QTest::newRow("long summary") << QString(300, QLatin1Char('x')) << QString()
                                << true << QByteArray(300, 'x');
  QTest::newRow("empty summary")
      << QString() << QString() << true << QByteArray();
  QTest::newRow("blank summary")
      << QStringLiteral("  ") << QString() << true << QByteArray();
  QTest::newRow("crlf summary") << QStringLiteral("Fix\r\nmore") << QString()
                                << false << QByteArray("Fix\nmore");
  QTest::newRow("crlf body") << QStringLiteral("Fix")
                             << QStringLiteral("Line 1\r\nLine 2") << false
                             << QByteArray("Fix\n\nLine 1\nLine 2");
  QTest::newRow("lone cr") << QStringLiteral("Fix")
                           << QStringLiteral("a\rb") << false
                           << QByteArray("Fix\n\na\nb");
}

void SvnRepoTest::commitMessageComposition() {
  QFETCH(QString, summary);
  QFETCH(QString, message);
  QFETCH(bool, cleanup);
  QFETCH(QByteArray, expected);

  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("a.txt")}));
  const QString before = head();
  resetLastError();
  const bool committed = SvnRepo::commitStaged(
      _wc, summary, message, kAuthor, kEmail, cleanup, false, false);
  if (expected.isNull()) {
    QVERIFY(!committed);
    EXPECT_SVN_ERROR(SvnRepo::SvnError::CommitFailed);
    QCOMPARE(head(), before);
    return;
  }
  QVERIFY2(committed, qPrintable(lastErrorMessage()));
  QCOMPARE(svnlookRevprop(_repoDir, head(), QStringLiteral("svn:log")),
           expected);
}

void SvnRepoTest::commitSignoff() {
  const QByteArray line = "Signed-off-by: Tester <tester@example.com>";
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("a.txt")}));
  QVERIFY(SvnRepo::commitStaged(_wc, QStringLiteral("Fix"),
                                QStringLiteral("Body"), kAuthor, kEmail, true,
                                true, false));
  QByteArray log = svnlookRevprop(_repoDir, head(), QStringLiteral("svn:log"));
  QCOMPARE(log, QByteArray("Fix\n\nBody\n\n") + line);

  // No second line when the body has it already.
  QVERIFY(writeFile(file("a.txt"), "b\n"));
  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("a.txt")}));
  QVERIFY(SvnRepo::commitStaged(_wc, QStringLiteral("Fix"),
                                QString::fromUtf8(line), kAuthor, kEmail,
                                true, true, false));
  log = svnlookRevprop(_repoDir, head(), QStringLiteral("svn:log"));
  QCOMPARE(log.count(line), 1);

  // Documented: without an author there is no sign-off line.
  QVERIFY(writeFile(file("a.txt"), "c\n"));
  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("a.txt")}));
  QVERIFY(SvnRepo::commitStaged(_wc, QStringLiteral("Fix"), QString(),
                                QString(), QString(), true, true, false));
  QCOMPARE(svnlookRevprop(_repoDir, head(), QStringLiteral("svn:log")),
           QByteArray("Fix"));
}

void SvnRepoTest::commitAuthor() {
  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());

  // Documented: for file:// repositories svn:author is the name of the
  // logged in user; the author and email arguments only reach the sign-off.
  // svn:// and http(s):// use the authenticated user (tst_svnauth).
  const QString user = qEnvironmentVariable("USERNAME",
                                            qEnvironmentVariable("USER"));
  QCOMPARE(svnlookAuthor(_repoDir, head()), user);
}

void SvnRepoTest::commitOutOfDate() {
  QVERIFY(!commitA("base\n", QStringLiteral("Base")).isEmpty());
  const QString other = _base + QStringLiteral("/other");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk"), other));
  QVERIFY(writeFile(other + QStringLiteral("/a.txt"), "theirs\n"));
  bool ok = false;
  svnIn(other, {QStringLiteral("commit"), QStringLiteral("-m"),
                QStringLiteral("Theirs")},
        &ok);
  QVERIFY(ok);
  const QString before = head();

  QVERIFY(writeFile(file("a.txt"), "mine\n"));
  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("a.txt")}));
  resetLastError();
  QVERIFY(!SvnRepo::commitStaged(_wc, QStringLiteral("Mine"), QString(),
                                 kAuthor, kEmail, true, false, false));
  EXPECT_SVN_ERROR(SvnRepo::SvnError::CommitFailed);
  QVERIFY2(lastErrorMessage().contains(QStringLiteral("out of date"),
                                       Qt::CaseInsensitive),
           qPrintable(lastErrorMessage()));
  QVERIFY(SvnRepo::getLastError().errorCode > 0);
  QCOMPARE(head(), before);
  // Still staged, for a retry after an update.
  QCOMPARE(changelistMembers(svnStatusXml(_wc)),
           QStringList{QStringLiteral("a.txt")});
}

void SvnRepoTest::commitNeedsLockAndForeignLock() {
  QVERIFY(writeFile(file("n.txt"), "n\n"));
  bool ok = false;
  svnIn(_wc, {QStringLiteral("add"), QStringLiteral("n.txt")}, &ok);
  svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("svn:needs-lock"),
              QStringLiteral("*"), QStringLiteral("n.txt")},
        &ok);
  svnIn(_wc, {QStringLiteral("commit"), QStringLiteral("-m"),
              QStringLiteral("Needs lock")},
        &ok);
  QVERIFY(ok);
  const QString other = _base + QStringLiteral("/other");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk"), other));
  svnIn(other, {QStringLiteral("lock"), QStringLiteral("n.txt")}, &ok);
  QVERIFY(ok);
  const QString before = head();

  // Edited anyway; the commit fails because another working copy holds the
  // lock.
  QFile::setPermissions(file("n.txt"), QFile::ReadOwner | QFile::WriteOwner);
  QVERIFY(writeFile(file("n.txt"), "mine\n"));
  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("n.txt")}));
  resetLastError();
  QVERIFY(!SvnRepo::commitStaged(_wc, QStringLiteral("Locked"), QString(),
                                 kAuthor, kEmail, true, false, false));
  EXPECT_SVN_ERROR(SvnRepo::SvnError::CommitFailed);
  QVERIFY2(lastErrorMessage().contains(QStringLiteral("lock"),
                                       Qt::CaseInsensitive),
           qPrintable(lastErrorMessage()));
  QCOMPARE(head(), before);
}

void SvnRepoTest::commitDoesNotIncludeUnstagedPropertyChanges() {
  QVERIFY(writeFile(file("b.txt"), "b\n"));
  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  QVERIFY(writeFile(file("a.txt"), "staged\n"));
  svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("p"),
              QStringLiteral("v"), QStringLiteral("b.txt")},
        &ok);
  QVERIFY(ok);
  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("a.txt")}));

  QVERIFY2(SvnRepo::commitStaged(_wc, QStringLiteral("Only a"), QString(),
                                 kAuthor, kEmail, true, false, false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(changedPaths(_repoDir, head()),
           QStringList{QStringLiteral("U trunk/a.txt")});
  QCOMPARE(svnStatusXml(_wc).value("b.txt").props, QStringLiteral("modified"));
}

void SvnRepoTest::commitSignCommitIsReported() {
  // Documented: Subversion revisions can't be signed; the commit is made
  // unsigned and a warning is logged.
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("a.txt")}));
  QVERIFY2(SvnRepo::commitStaged(_wc, QStringLiteral("Signed?"), QString(),
                                 kAuthor, kEmail, true, false, true),
           qPrintable(lastErrorMessage()));
  QCOMPARE(changedPaths(_repoDir, head()),
           QStringList{QStringLiteral("A trunk/a.txt")});
}

// --- 3.5 Discard, ignore, checkout, patch -----------------------------------

void SvnRepoTest::discardCategories_data() {
  QTest::addColumn<QString>("kind");
  for (const char *kind :
       {"modified", "added", "added staged", "unversioned file",
        "unversioned dir", "deleted", "missing", "property", "replaced",
        "file in added dir"}) {
    QTest::newRow(kind) << QString::fromLatin1(kind);
  }
}

void SvnRepoTest::discardCategories() {
  QFETCH(QString, kind);

  QVERIFY(!commitA("base\n", QStringLiteral("Base")).isEmpty());
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  QString path = QStringLiteral("a.txt");
  QByteArray content = "base\n";
  // Discard is the Git backend's (§7.4 item 2): tracked files go back to
  // BASE; added and unversioned files stay on disk, unversioned.
  bool staysUnversioned = false;
  if (kind == QLatin1String("modified")) {
    QVERIFY(writeFile(file(path), "changed\n"));
  } else if (kind == QLatin1String("added") ||
             kind == QLatin1String("added staged")) {
    path = QStringLiteral("new.txt");
    content = "new\n";
    QVERIFY(writeFile(file(path), content));
    svnIn(_wc, {QStringLiteral("add"), path}, &ok);
    if (kind == QLatin1String("added staged")) {
      QVERIFY(SvnRepo::stageFiles(_wc, {path}));
    }
    staysUnversioned = true;
  } else if (kind == QLatin1String("unversioned file")) {
    path = QStringLiteral("new.txt");
    content = "new\n";
    QVERIFY(writeFile(file(path), content));
    staysUnversioned = true;
  } else if (kind == QLatin1String("unversioned dir")) {
    path = QStringLiteral("newdir");
    QVERIFY(writeFile(file("newdir/x.o"), "ignored\n"));
    QVERIFY(writeFile(file("newdir/y.txt"), "y\n"));
    staysUnversioned = true;
  } else if (kind == QLatin1String("deleted")) {
    svnIn(_wc, {QStringLiteral("delete"), path}, &ok);
  } else if (kind == QLatin1String("missing")) {
    QVERIFY(QFile::remove(file(path)));
  } else if (kind == QLatin1String("property")) {
    svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("p"),
                QStringLiteral("v"), path},
          &ok);
  } else if (kind == QLatin1String("replaced")) {
    svnIn(_wc, {QStringLiteral("delete"), path}, &ok);
    QVERIFY(writeFile(file(path), "replacement\n"));
    svnIn(_wc, {QStringLiteral("add"), path}, &ok);
  } else if (kind == QLatin1String("file in added dir")) {
    QVERIFY(writeFile(file("adir/f.txt"), "f\n"));
    svnIn(_wc, {QStringLiteral("add"), QStringLiteral("adir")}, &ok);
    path = QStringLiteral("adir/f.txt");
    content = "f\n";
    staysUnversioned = true;
  }

  QVERIFY2(SvnRepo::discardFiles(_wc, {path}), qPrintable(lastErrorMessage()));
  const StatusMap oracle = svnStatusXml(_wc);
  QVERIFY(changelistMembers(oracle).isEmpty());
  if (!staysUnversioned) {
    QCOMPARE(readFile(file(path)), content);
    QVERIFY2(!oracle.contains(path), qPrintable(oracle.value(path).item));
    return;
  }

  if (kind == QLatin1String("unversioned dir")) {
    QCOMPARE(readFile(file("newdir/y.txt")), QByteArray("y\n"));
    QCOMPARE(readFile(file("newdir/x.o")), QByteArray("ignored\n"));
    return;
  }
  QCOMPARE(readFile(file(path)), content);
  if (kind == QLatin1String("file in added dir")) {
    // Nothing in the folder is added any more, so it is unversioned too, and
    // Subversion lists only the folder.
    QCOMPARE(oracle.value("adir").item, QStringLiteral("unversioned"));
  } else {
    QCOMPARE(oracle.value(path).item, QStringLiteral("unversioned"));
  }
}

void SvnRepoTest::discardStagedChanges() {
  QVERIFY(!commitA("base\n", QStringLiteral("Base")).isEmpty());
  QVERIFY(writeFile(file("a.txt"), "staged\n"));
  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("a.txt")}));
  QVERIFY(writeFile(file("a.txt"), "staged and more\n"));

  // Back to BASE, as in Git, and out of the staging area.
  QVERIFY(SvnRepo::discardFiles(_wc, {QStringLiteral("a.txt")}));
  QCOMPARE(readFile(file("a.txt")), QByteArray("base\n"));
  QVERIFY(!status().hasChanges());
  QVERIFY(changelistMembers(svnStatusXml(_wc, true)).isEmpty());
}

void SvnRepoTest::discardDeletedChildOfDeletedDirectory() {
  QVERIFY(writeFile(file("dir/a.txt"), "a\n"));
  QVERIFY(writeFile(file("dir/b.txt"), "b\n"));
  QVERIFY(commitAll(_wc, QStringLiteral("Base")));
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  svnIn(_wc, {QStringLiteral("delete"), QStringLiteral("dir")}, &ok);
  QVERIFY(ok);

  // Discarding one file of a deleted folder brings that file back.
  const bool discarded =
      SvnRepo::discardFiles(_wc, {QStringLiteral("dir/a.txt")});
  QVERIFY2(discarded, qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("dir/a.txt")), QByteArray("a\n"));
  QCOMPARE(svnStatusXml(_wc).value("dir/b.txt").item,
           QStringLiteral("deleted"));
}

void SvnRepoTest::discardOneSideOfMove() {
  QVERIFY(writeFile(file("src.txt"), "content\n"));
  QVERIFY(commitAll(_wc, QStringLiteral("Base")));
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  svnIn(_wc, {QStringLiteral("move"), QStringLiteral("src.txt"),
              QStringLiteral("dst.txt")},
        &ok);
  QVERIFY(ok);

  // Discarding the source brings it back; the copy stays, like a copy.
  QVERIFY2(SvnRepo::discardFiles(_wc, {QStringLiteral("src.txt")}),
           qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("src.txt")), QByteArray("content\n"));
  QCOMPARE(readFile(file("dst.txt")), QByteArray("content\n"));
  QCOMPARE(svnStatusXml(_wc).value("dst.txt").item, QStringLiteral("added"));
}

void SvnRepoTest::discardTreeConflict() {
  QVERIFY(!commitA("base\n", QStringLiteral("Base")).isEmpty());
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  const QString other = _base + QStringLiteral("/other");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk"), other));
  svnIn(other, {QStringLiteral("delete"), QStringLiteral("a.txt")});
  svnIn(other, {QStringLiteral("commit"), QStringLiteral("-m"),
                QStringLiteral("Delete")},
        &ok);
  QVERIFY(ok);
  QVERIFY(writeFile(file("a.txt"), "mine\n"));
  svnIn(_wc, {QStringLiteral("update"), QStringLiteral("--accept"),
              QStringLiteral("postpone")});
  QVERIFY(SvnRepo::isTreeConflict(_wc, QStringLiteral("a.txt")));

  // Discarding the file ends its tree conflict. (B19, a tree conflict whose
  // node status is normal, needs another victim than this edit vs delete.)
  QVERIFY2(SvnRepo::discardFiles(_wc, {QStringLiteral("a.txt")}),
           qPrintable(lastErrorMessage()));
  QVERIFY(!SvnRepo::hasConflicts(_wc));
}

void SvnRepoTest::discardRootAndOutsidePaths_data() {
  QTest::addColumn<QString>("path");
  QTest::newRow("dot") << QStringLiteral(".");
  QTest::newRow("empty") << QString();
  QTest::newRow("parent") << QStringLiteral("..");
  QTest::newRow("outside") << QStringLiteral("<outside>");
}

void SvnRepoTest::discardRootAndOutsidePaths() {
  QFETCH(QString, path);

  QVERIFY(!commitA("base\n", QStringLiteral("Base")).isEmpty());
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  // A root property change makes the root itself a change.
  svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("p"),
              QStringLiteral("v"), QStringLiteral(".")},
        &ok);
  QVERIFY(writeFile(file("a.txt"), "precious\n"));
  QVERIFY(writeFile(file("new.txt"), "precious\n"));
  QVERIFY(writeFile(_base + QStringLiteral("/outside/o.txt"), "outside\n"));
  if (path == QLatin1String("<outside>")) {
    path = _base + QStringLiteral("/outside");
  }

  // Refused: the whole working copy, or something outside it, is never a
  // file to discard.
  resetLastError();
  const bool discarded = SvnRepo::discardFiles(_wc, {path});
  QVERIFY(!discarded);
  QCOMPARE(readFile(file("a.txt")), QByteArray("precious\n"));
  QCOMPARE(readFile(file("new.txt")), QByteArray("precious\n"));
  QCOMPARE(readFile(_base + QStringLiteral("/outside/o.txt")),
           QByteArray("outside\n"));
}

void SvnRepoTest::discardPartialFailureLeavesConsistentState() {
  QVERIFY(!commitA("base\n", QStringLiteral("Base")).isEmpty());
  QVERIFY(writeFile(file("a.txt"), "changed\n"));
  QVERIFY(writeFile(file("new.txt"), "new\n"));
  QVERIFY(writeFile(_base + QStringLiteral("/outside.txt"), "o\n"));

  // One bad path: the call fails, and nothing was done before it failed.
  resetLastError();
  QVERIFY(!SvnRepo::discardFiles(
      _wc, {QStringLiteral("new.txt"), QStringLiteral("a.txt"),
            QStringLiteral("../outside.txt")}));
  QVERIFY(!lastErrorMessage().isEmpty());
  QCOMPARE(readFile(file("new.txt")), QByteArray("new\n"));
  QCOMPARE(readFile(file("a.txt")), QByteArray("changed\n"));
}

void SvnRepoTest::ignoreFileNonRecursive() {
  QVERIFY(writeFile(file("x.log"), "log\n"));
  QVERIFY(writeFile(file("keep.txt"), "keep\n"));
  QVERIFY2(SvnRepo::ignoreFiles(_wc, {QStringLiteral("x.log")}, false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(svnPropget(QStringLiteral("svn:ignore"), _wc),
           QStringLiteral("x.log"));

  const SvnRepo::SvnStatus st = status();
  QCOMPARE(st.newFiles, QStringList{QStringLiteral("keep.txt")});
  QCOMPARE(st.ignoredFiles, QStringList{QStringLiteral("x.log")});
  // The property change is a change to commit.
  QCOMPARE(svnStatusXml(_wc).value(".").props, QStringLiteral("modified"));
  QVERIFY(!st.modifiedFiles.isEmpty());
}

void SvnRepoTest::ignoreRecursive_data() {
  QTest::addColumn<QString>("path");
  QTest::addColumn<QString>("parent");
  QTest::newRow("in folder") << QStringLiteral("dir/x.tmp")
                             << QStringLiteral("dir");
  QTest::newRow("at root") << QStringLiteral("x.tmp") << QString();
  QTest::newRow("space") << QStringLiteral("dir/a b.tmp")
                         << QStringLiteral("dir");
}

void SvnRepoTest::ignoreRecursive() {
  QFETCH(QString, path);
  QFETCH(QString, parent);

  QVERIFY(writeFile(file("dir/keep.txt"), "keep\n"));
  QVERIFY(commitAll(_wc, QStringLiteral("Base")));
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  QVERIFY(writeFile(file(path), "x\n"));

  QVERIFY2(SvnRepo::ignoreFiles(_wc, {path}, true),
           qPrintable(lastErrorMessage()));
  // The file's own name goes into the parent's svn:global-ignores (§7.4
  // item 7).
  const QString name = QFileInfo(path).fileName();
  const QString target = parent.isEmpty() ? _wc : file(parent);
  QCOMPARE(svnPropget(QStringLiteral("svn:global-ignores"), target), name);

  // The name is ignored at any depth below; other new files stay visible.
  const QString prefix = parent.isEmpty() ? QString() : parent + '/';
  QVERIFY(writeFile(file(prefix + QStringLiteral("deep/") + name), "x\n"));
  QVERIFY(writeFile(file(prefix + QStringLiteral("deep/other.txt")), "o\n"));
  const QStringList newFiles = status().newFiles;
  QVERIFY2(!newFiles.contains(path), qPrintable(newFiles.join(", ")));
  QVERIFY2(!newFiles.contains(prefix + QStringLiteral("deep/") + name),
           qPrintable(newFiles.join(", ")));
  QVERIFY2(newFiles.contains(prefix + QStringLiteral("deep/other.txt")),
           qPrintable(newFiles.join(", ")));
}

void SvnRepoTest::ignoreGlobCharacters_data() {
  QTest::addColumn<QString>("name");
  QTest::addColumn<QString>("lookalike");
  QTest::newRow("brackets") << QStringLiteral("file[1].txt")
                            << QStringLiteral("file1.txt");
#ifndef Q_OS_WIN
  QTest::newRow("star") << QStringLiteral("a*b") << QStringLiteral("axxb");
  QTest::newRow("question") << QStringLiteral("a?b") << QStringLiteral("axb");
#endif
}

void SvnRepoTest::ignoreGlobCharacters() {
  QFETCH(QString, name);
  QFETCH(QString, lookalike);

  QVERIFY(writeFile(file(name), "x\n"));
  QVERIFY(writeFile(file(lookalike), "y\n"));
  QVERIFY(SvnRepo::ignoreFiles(_wc, {name}, false));

  // The pattern matches only that file.
  const QStringList newFiles = status().newFiles;
  QVERIFY(!newFiles.contains(name));
  QVERIFY2(newFiles.contains(lookalike), qPrintable(newFiles.join(", ")));
}

void SvnRepoTest::ignoreExistingPropertyValues() {
  bool ok = false;
  svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("svn:ignore"),
              QStringLiteral("a.log\nb.log  \nb.log\n"), QStringLiteral(".")},
        &ok);
  QVERIFY(ok);

  QVERIFY(SvnRepo::ignoreFiles(_wc, {QStringLiteral("c.log")}, false));
  QVERIFY(SvnRepo::ignoreFiles(_wc, {QStringLiteral("b.log")}, false));
  const QStringList lines =
      svnPropget(QStringLiteral("svn:ignore"), _wc).split('\n');
  QStringList trimmed;
  for (const QString &line : lines) {
    trimmed += line.trimmed();
  }
  // The entries that were there survive, c.log is added once, b.log isn't
  // added again.
  QVERIFY2(trimmed.contains(QStringLiteral("a.log")),
           qPrintable(trimmed.join('|')));
  QCOMPARE(trimmed.count(QStringLiteral("c.log")), 1);
  QCOMPARE(trimmed.count(QStringLiteral("b.log")), 2);
}

void SvnRepoTest::ignoreInsideUnversionedTree() {
  QVERIFY(writeFile(file("new/dir/x.txt"), "x\n"));
  QVERIFY(writeFile(file("other.txt"), "o\n"));

  // Documented: an unversioned folder can't hold properties, so its topmost
  // unversioned folder is ignored instead.
  QVERIFY(SvnRepo::ignoreFiles(_wc, {QStringLiteral("new/dir/x.txt")}, false));
  QCOMPARE(svnPropget(QStringLiteral("svn:ignore"), _wc),
           QStringLiteral("new"));
  QCOMPARE(status().newFiles, QStringList{QStringLiteral("other.txt")});
}

void SvnRepoTest::ignoreOutsideWorkingCopyTerminates() {
  // B10 (fixed): ignoreFiles walked up the parents of a path outside the
  // working copy and never stopped at the drive root.
  QTemporaryDir outside;
  QVERIFY(outside.isValid());
  const QString path = outside.filePath(QStringLiteral("dir/x.log"));
  QVERIFY(!SvnRepo::ignoreFiles(_wc, {path}, false));
  QCOMPARE(lastError(), SvnRepo::SvnError::InvalidArgument);
  QVERIFY(!SvnRepo::ignoreFiles(_wc, {path}, true));
  QCOMPARE(lastError(), SvnRepo::SvnError::InvalidArgument);
  QVERIFY(!SvnRepo::ignoreFiles(_wc, {QStringLiteral("../x.log")}, false));
  QCOMPARE(lastError(), SvnRepo::SvnError::InvalidArgument);
  QVERIFY(!SvnRepo::ignoreFiles(_wc, {QDir::rootPath()}, true));
  QCOMPARE(lastError(), SvnRepo::SvnError::InvalidArgument);
  QVERIFY(svnPropget(QStringLiteral("svn:ignore"), _wc).isEmpty());
  QVERIFY(svnPropget(QStringLiteral("svn:global-ignores"), _wc).isEmpty());
}

void SvnRepoTest::ignoreVersionedFile() {
  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
  QVERIFY(writeFile(file("a.txt"), "changed\n"));

  // Documented: svn:ignore only affects unversioned files, so ignoring a
  // versioned one records the name, and the root's property change is the
  // only other change.
  QVERIFY(SvnRepo::ignoreFiles(_wc, {QStringLiteral("a.txt")}, false));
  QCOMPARE(svnPropget(QStringLiteral("svn:ignore"), _wc),
           QStringLiteral("a.txt"));
  QCOMPARE(status().modifiedFiles,
           (QStringList{QStringLiteral("."), QStringLiteral("a.txt")}));
}

void SvnRepoTest::checkoutCommit_data() {
  QTest::addColumn<QString>("commit");
  QTest::addColumn<QString>("revision"); // empty when it must fail
  QTest::newRow("past") << QStringLiteral("2") << QStringLiteral("2");
  QTest::newRow("r prefix") << QStringLiteral("r3") << QStringLiteral("3");
  QTest::newRow("above head") << QStringLiteral("99") << QString();
  QTest::newRow("r0") << QStringLiteral("0") << QString();
  QTest::newRow("before branch") << QStringLiteral("<before>") << QString();
  QTest::newRow("not a number") << QStringLiteral("abc") << QString();
  QTest::newRow("HEAD") << QStringLiteral("HEAD") << QString();
}

void SvnRepoTest::checkoutCommit() {
  QFETCH(QString, commit);
  QFETCH(QString, revision);

  QVERIFY(!commitA("2\n", QStringLiteral("Two")).isEmpty());
  QVERIFY(!commitA("3\n", QStringLiteral("Three")).isEmpty());
  QVERIFY(!commitA("4\n", QStringLiteral("Four")).isEmpty());
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  if (commit == QLatin1String("<before>")) {
    // A branch made at r5: r2 is older than the branch.
    QVERIFY(SvnRepo::createBranchFromCommit(_wc, QString(),
                                            QStringLiteral("b")));
    QVERIFY(SvnRepo::checkoutBranch(_wc, QStringLiteral("b"),
                                    SvnRepo::BranchType::Local));
    commit = QStringLiteral("2");
  }
  const QString before = svnInfo(_wc, QStringLiteral("revision"));

  resetLastError();
  const bool done = SvnRepo::checkoutCommit(_wc, commit);
  if (!revision.isEmpty()) {
    QVERIFY2(done, qPrintable(lastErrorMessage()));
    QCOMPARE(svnInfo(_wc, QStringLiteral("revision")), revision);
    QCOMPARE(readFile(file("a.txt")), (revision + QLatin1Char('\n')).toUtf8());
    return;
  }
  QVERIFY(!done);
  QCOMPARE(lastError(), SvnRepo::SvnError::InvalidRevision);
  QVERIFY(!lastErrorMessage().isEmpty());
  QCOMPARE(svnInfo(_wc, QStringLiteral("revision")), before);
}

void SvnRepoTest::checkoutCommitWithConflictingLocalEdit() {
  QVERIFY(!commitA("one\n", QStringLiteral("One")).isEmpty());
  const QString first = head();
  QVERIFY(!commitA("two\n", QStringLiteral("Two")).isEmpty());
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  QVERIFY(writeFile(file("a.txt"), "mine\n"));

  // The update to the older revision conflicts with the local edit; the
  // caller has to hear about it.
  resetLastError();
  const bool done = SvnRepo::checkoutCommit(_wc, first);
  QVERIFY(SvnRepo::hasConflicts(_wc));
  QVERIFY(!done);
}

namespace {

// Bytes of every file below dir except the administrative area.
QMap<QString, QByteArray> treeContent(const QString &dir) {
  QMap<QString, QByteArray> files;
  QDirIterator it(dir, QDir::Files | QDir::Hidden,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    const QString path = it.next();
    const QString relative = QDir(dir).relativeFilePath(path);
    if (!relative.startsWith(QLatin1String(".svn"))) {
      files.insert(relative, readFile(path));
    }
  }
  return files;
}

} // namespace

void SvnRepoTest::patchRoundTrips_data() {
  QTest::addColumn<QString>("source");
  QTest::addColumn<QString>("change");
  for (const char *source :
       {"createPatchFromCommit", "svn diff", "svn diff --git", "diffFiles"}) {
    for (const char *change : {"modify", "add", "delete", "no newline",
                               "crlf", "unicode", "b/ in path"}) {
      QTest::addRow("%s, %s", source, change)
          << QString::fromLatin1(source) << QString::fromLatin1(change);
    }
  }
}

void SvnRepoTest::patchRoundTrips() {
  QFETCH(QString, source);
  QFETCH(QString, change);

  // The file each change is about, before and after.
  QString path = QStringLiteral("a.txt");
  QByteArray before = "one\ntwo\nthree\n";
  QByteArray after = "one\nTWO\nthree\n";
  if (change == QLatin1String("add")) {
    before.clear();
    after = "new\nfile\n";
    path = QStringLiteral("added.txt");
  } else if (change == QLatin1String("delete")) {
    after.clear();
  } else if (change == QLatin1String("no newline")) {
    before = "one\ntwo";
    after = "one\nTWO";
  } else if (change == QLatin1String("crlf")) {
    before = "one\r\ntwo\r\nthree\r\n";
    after = "one\r\nTWO\r\nthree\r\n";
  } else if (change == QLatin1String("unicode")) {
    path = QString::fromUtf16(u"ünï 日本.txt");
  } else if (change == QLatin1String("b/ in path")) {
    path = QStringLiteral("x b/y.txt");
  }

  if (!before.isEmpty()) {
    QVERIFY(writeFile(file(path), before));
  }
  QVERIFY(writeFile(file("other.txt"), "other\n"));
  QVERIFY(commitAll(_wc, QStringLiteral("Before")));
  const QString base = head();
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  const auto makeChange = [&]() {
    if (after.isEmpty()) {
      svnIn(_wc, {QStringLiteral("delete"), path});
    } else {
      writeFile(file(path), after);
      if (before.isEmpty()) {
        svnIn(_wc, {QStringLiteral("add"), QStringLiteral("--parents"), path});
      }
    }
  };
  const QMap<QString, QByteArray> beforeTree = treeContent(_wc);

  if (change == QLatin1String("unicode") && source.startsWith("svn diff")) {
    QSKIP("The CLI writes paths in the console's code page");
  }

  QString patch;
  if (source == QLatin1String("createPatchFromCommit")) {
    makeChange();
    svnIn(_wc, {QStringLiteral("commit"), QStringLiteral("-m"),
                QStringLiteral("After")},
          &ok);
    QVERIFY(ok);
    patch = SvnRepo::createPatchFromCommit(_wc, head());
    svnIn(_wc, {QStringLiteral("update"), QStringLiteral("-r"), base}, &ok);
    QVERIFY(ok);
  } else {
    makeChange();
    if (source == QLatin1String("svn diff")) {
      patch = QString::fromUtf8(runTool(
          QStringLiteral("svn"), _wc, {QStringLiteral("diff")}, nullptr,
          Output::Raw));
    } else if (source == QLatin1String("svn diff --git")) {
      patch = QString::fromUtf8(runTool(
          QStringLiteral("svn"), _wc,
          {QStringLiteral("diff"), QStringLiteral("--git")}, nullptr,
          Output::Raw));
    } else {
      patch = SvnRepo::diffFiles(_wc, {path}).diff;
    }
    svnIn(_wc, {QStringLiteral("revert"), QStringLiteral("-R"),
                QStringLiteral("."), QStringLiteral("--remove-added")},
          &ok);
    QVERIFY(ok);
    if (before.isEmpty()) {
      QFile::remove(file(path));
    }
  }
  QCOMPARE(treeContent(_wc), beforeTree);
  QVERIFY2(!patch.isEmpty(), qPrintable(lastErrorMessage()));

  // Applying the patch makes the change again, as an unstaged change.
  resetLastError();
  const bool applied = SvnRepo::applyPatch(_wc, patch);
  const bool changed = after.isEmpty() ? !QFileInfo::exists(file(path))
                                       : readFile(file(path)) == after;
  QVERIFY2(applied && changed,
           qPrintable(lastErrorMessage() + QLatin1Char('\n') + patch));
  QCOMPARE(readFile(file("other.txt")), QByteArray("other\n"));
  QVERIFY(changelistMembers(svnStatusXml(_wc)).isEmpty());
}

void SvnRepoTest::patchSkippedTargetFails() {
  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
  const QString patch = QStringLiteral(
      "Index: missing.txt\n"
      "===================================================================\n"
      "--- missing.txt\t(revision 1)\n"
      "+++ missing.txt\t(working copy)\n"
      "@@ -1 +1 @@\n"
      "-old\n"
      "+new\n");

  // Nothing could be applied, so the call fails.
  resetLastError();
  const bool applied = SvnRepo::applyPatch(_wc, patch);
  QVERIFY(!applied);
  QVERIFY(!lastErrorMessage().isEmpty());
  QVERIFY(!QFileInfo::exists(file("missing.txt")));
}

void SvnRepoTest::patchRejectedHunks() {
  QVERIFY(!commitA("one\ntwo\nthree\n", QStringLiteral("Base")).isEmpty());
  const QString patch = QStringLiteral(
      "Index: a.txt\n"
      "===================================================================\n"
      "--- a.txt\t(revision 2)\n"
      "+++ a.txt\t(working copy)\n"
      "@@ -1,3 +1,3 @@\n"
      " uno\n"
      "-dos\n"
      "+DOS\n"
      " tres\n");

  resetLastError();
  QVERIFY(!SvnRepo::applyPatch(_wc, patch));
  EXPECT_SVN_ERROR(SvnRepo::SvnError::PatchFailed);
  QVERIFY(lastErrorMessage().contains(QStringLiteral("a.txt")));
  QCOMPARE(readFile(file("a.txt")), QByteArray("one\ntwo\nthree\n"));
  // The rejected hunks are kept where the user can see them.
  QVERIFY(QFileInfo::exists(file("a.txt.svnpatch.rej")));
  QVERIFY(status().newFiles.contains(QStringLiteral("a.txt.svnpatch.rej")));
}

void SvnRepoTest::patchAlreadyAppliedEmptyGarbage_data() {
  QTest::addColumn<QString>("patch");
  QTest::newRow("already applied") << QStringLiteral(
      "Index: a.txt\n"
      "===================================================================\n"
      "--- a.txt\t(revision 2)\n"
      "+++ a.txt\t(working copy)\n"
      "@@ -1,3 +1,3 @@\n"
      " one\n"
      "-zwei\n"
      "+two\n"
      " three\n");
  QTest::newRow("empty") << QString();
  QTest::newRow("garbage") << QStringLiteral("this is not a patch\n");
}

void SvnRepoTest::patchAlreadyAppliedEmptyGarbage() {
  QFETCH(QString, patch);

  QVERIFY(!commitA("one\ntwo\nthree\n", QStringLiteral("Base")).isEmpty());

  // Nothing to apply: refused with a reason; the file is untouched.
  resetLastError();
  const bool applied = SvnRepo::applyPatch(_wc, patch);
  QVERIFY(!applied);
  if (!applied) {
    QVERIFY(!lastErrorMessage().isEmpty());
  }
  QCOMPARE(readFile(file("a.txt")), QByteArray("one\ntwo\nthree\n"));
  QVERIFY(!status().hasChanges());
}

void SvnRepoTest::patchCrlfEmptyContextLine() {
  QVERIFY(!commitA("a\r\n\r\nb\r\nc\r\n", QStringLiteral("Base")).isEmpty());

  // An LF patch whose empty context line has lost its leading space, as some
  // editors write it.
  const QString patch = QStringLiteral(
      "Index: a.txt\n"
      "===================================================================\n"
      "--- a.txt\t(revision 2)\n"
      "+++ a.txt\t(working copy)\n"
      "@@ -1,4 +1,4 @@\n"
      " a\n"
      "\n"
      "-b\n"
      "+B\n"
      " c\n");
  resetLastError();
  const bool applied = SvnRepo::applyPatch(_wc, patch);
  QVERIFY2(applied, qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("a.txt")), QByteArray("a\r\n\r\nB\r\nc\r\n"));
}

void SvnRepoTest::patchNonUtf8Content() {
  QVERIFY(!commitA("caf\xe9\ntwo\n", QStringLiteral("Base")).isEmpty());

  // A Latin-1 line in the context: its bytes must survive.
  QByteArray patch =
      "Index: a.txt\n"
      "===================================================================\n"
      "--- a.txt\t(revision 2)\n"
      "+++ a.txt\t(working copy)\n"
      "@@ -1,2 +1,2 @@\n"
      " caf\xe9\n"
      "-two\n"
      "+TWO\n";
  resetLastError();
  const bool applied = SvnRepo::applyPatch(_wc, QString::fromLatin1(patch));
  QVERIFY2(applied, qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("a.txt")), QByteArray("caf\xe9\nTWO\n"));
}

// --- 3.6 Conflicts, merge state, tree conflicts -----------------------------

void SvnRepoTest::conflictKinds_data() {
  QTest::addColumn<QString>("kind");
  QTest::addColumn<bool>("tree");
  QTest::newRow("text") << QStringLiteral("text") << false;
  QTest::newRow("property") << QStringLiteral("property") << false;
  QTest::newRow("edit vs delete") << QStringLiteral("edit vs delete") << true;
  QTest::newRow("delete vs edit") << QStringLiteral("delete vs edit") << true;
  QTest::newRow("add vs add") << QStringLiteral("add vs add") << true;
  QTest::newRow("directory victim") << QStringLiteral("directory") << true;
}

void SvnRepoTest::conflictKinds() {
  QFETCH(QString, kind);
  QFETCH(bool, tree);

  QVERIFY(writeFile(file("dir/x.txt"), "x\n"));
  QVERIFY(!commitA("base\n", QStringLiteral("Base")).isEmpty());
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  const QString other = _base + QStringLiteral("/other");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk"), other));

  QString victim = QStringLiteral("a.txt");
  if (kind == QLatin1String("text")) {
    QVERIFY(writeFile(other + QStringLiteral("/a.txt"), "theirs\n"));
    QVERIFY(writeFile(file("a.txt"), "mine\n"));
  } else if (kind == QLatin1String("property")) {
    svnIn(other, {QStringLiteral("propset"), QStringLiteral("p"),
                  QStringLiteral("theirs"), QStringLiteral("a.txt")});
    svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("p"),
                QStringLiteral("mine"), QStringLiteral("a.txt")});
  } else if (kind == QLatin1String("edit vs delete")) {
    svnIn(other, {QStringLiteral("delete"), QStringLiteral("a.txt")});
    QVERIFY(writeFile(file("a.txt"), "mine\n"));
  } else if (kind == QLatin1String("delete vs edit")) {
    QVERIFY(writeFile(other + QStringLiteral("/a.txt"), "theirs\n"));
    svnIn(_wc, {QStringLiteral("delete"), QStringLiteral("a.txt")});
  } else if (kind == QLatin1String("add vs add")) {
    victim = QStringLiteral("both.txt");
    QVERIFY(writeFile(other + QStringLiteral("/both.txt"), "theirs\n"));
    svnIn(other, {QStringLiteral("add"), QStringLiteral("both.txt")});
    QVERIFY(writeFile(file("both.txt"), "mine\n"));
    svnIn(_wc, {QStringLiteral("add"), QStringLiteral("both.txt")});
  } else {
    victim = QStringLiteral("dir");
    svnIn(other, {QStringLiteral("delete"), QStringLiteral("dir")});
    QVERIFY(writeFile(file("dir/x.txt"), "mine\n"));
  }
  svnIn(other, {QStringLiteral("commit"), QStringLiteral("-m"),
                QStringLiteral("Theirs")},
        &ok);
  QVERIFY(ok);
  svnIn(_wc, {QStringLiteral("update"), QStringLiteral("--accept"),
              QStringLiteral("postpone")});

  const StatusMap oracle = svnStatusXml(_wc);
  QStringList oracleConflicts;
  for (auto it = oracle.cbegin(); it != oracle.cend(); ++it) {
    if (it->item == QLatin1String("conflicted") ||
        it->props == QLatin1String("conflicted") || it->treeConflicted) {
      oracleConflicts += it.key();
    }
  }
  QCOMPARE(oracleConflicts, QStringList{victim});
  QVERIFY(SvnRepo::hasConflicts(_wc));
  QCOMPARE(status().conflictedFiles, QStringList{victim});
  QCOMPARE(SvnRepo::isTreeConflict(_wc, victim), tree);
  if (kind == QLatin1String("text")) {
    const QString markers =
        SvnRepo::listConflictsForAFile(_wc, QStringLiteral("a.txt"));
    QVERIFY(markers.contains(QStringLiteral("<<<<<<<")));
    QVERIFY(markers.contains(QStringLiteral("mine")));
    QVERIFY(markers.contains(QStringLiteral("theirs")));
  }
}

void SvnRepoTest::listConflictsForNonConflictedFile() {
  QVERIFY(!commitA("plain\n", QStringLiteral("Base")).isEmpty());

  // Not the raw file content: there are no conflicts to list.
  resetLastError();
  const QString conflicts =
      SvnRepo::listConflictsForAFile(_wc, QStringLiteral("a.txt"));
  QVERIFY2(conflicts.isEmpty(), qPrintable(conflicts));
}

namespace {

// Conflicting changes to a.txt on trunk ("main") and branches/feature
// ("feature") from a common "base". Returns the feature revision.
QString makeConflictingBranches(const QString &wc, const QString &rootUrl,
                                const QString &base) {
  if (!writeFile(wc + QStringLiteral("/a.txt"), "base\n") ||
      !writeFile(wc + QStringLiteral("/other.txt"), "other\n") ||
      !svnCommitAll(wc, QStringLiteral("Base"))) {
    return {};
  }
  bool ok = false;
  svn({QStringLiteral("copy"), QStringLiteral("-m"), QStringLiteral("Branch"),
       rootUrl + QStringLiteral("/trunk"),
       rootUrl + QStringLiteral("/branches/feature")},
      &ok);
  const QString feature = base + QStringLiteral("/feature");
  if (!ok || !secondWc(rootUrl + QStringLiteral("/branches/feature"),
                       feature) ||
      !writeFile(feature + QStringLiteral("/a.txt"), "feature\n") ||
      !svnCommitAll(feature, QStringLiteral("Feature"))) {
    return {};
  }
  const QString featureRevision =
      svnInfo(rootUrl + QStringLiteral("/branches/feature"),
              QStringLiteral("last-changed-revision"));
  if (!writeFile(wc + QStringLiteral("/a.txt"), "main\n") ||
      !svnCommitAll(wc, QStringLiteral("Main"))) {
    return {};
  }
  svnIn(wc, {QStringLiteral("update")}, &ok);
  return ok ? featureRevision : QString();
}

} // namespace

void SvnRepoTest::abortMergeAfterConflictedOperation_data() {
  QTest::addColumn<QString>("operation");
  QTest::newRow("merge") << QStringLiteral("merge");
  QTest::newRow("cherry-pick") << QStringLiteral("cherry-pick");
  QTest::newRow("revert") << QStringLiteral("revert");
}

void SvnRepoTest::abortMergeAfterConflictedOperation() {
  QFETCH(QString, operation);

  const QString featureRevision = makeConflictingBranches(_wc, _rootUrl, _base);
  QVERIFY(!featureRevision.isEmpty());
  QString revertedRevision;
  if (operation == QLatin1String("revert")) {
    // Reverting the trunk revision that set "base" conflicts with "main".
    QVERIFY(writeFile(file("a.txt"), "later\n"));
    QVERIFY(svnCommitAll(_wc, QStringLiteral("Later")));
    revertedRevision = svnInfo(_rootUrl + QStringLiteral("/trunk"),
                               QStringLiteral("last-changed-revision"));
    QVERIFY(writeFile(file("a.txt"), "main again\n"));
    QVERIFY(svnCommitAll(_wc, QStringLiteral("Main again")));
    bool ok = false;
    svnIn(_wc, {QStringLiteral("update")}, &ok);
  }
  const QString before = head();
  const QString baseContent = QString::fromUtf8(readFile(file("a.txt")));
  // Local work outside the operation, and an untracked file.
  QVERIFY(writeFile(file("other.txt"), "local\n"));
  QVERIFY(writeFile(file("untracked.txt"), "untracked\n"));

  resetLastError();
  bool started = false;
  SvnRepo::SvnError expectedError = SvnRepo::SvnError::Unknown;
  if (operation == QLatin1String("merge")) {
    started = SvnRepo::merge(_wc, QStringLiteral("feature"),
                             QStringLiteral("trunk"), kAuthor, kEmail);
    expectedError = SvnRepo::SvnError::MergeFailedWithConflicts;
  } else if (operation == QLatin1String("cherry-pick")) {
    started = SvnRepo::cherryPickCommit(_wc, featureRevision, kAuthor, kEmail);
    expectedError = SvnRepo::SvnError::CherryPickFailedWithConflicts;
  } else {
    started = SvnRepo::revertCommit(_wc, revertedRevision, kAuthor, kEmail,
                                    QString());
    expectedError = SvnRepo::SvnError::RevertFailedWithConflicts;
  }
  // Stopped with conflicts, nothing committed, the operation in progress.
  QVERIFY(!started);
  QCOMPARE(lastError(), expectedError);
  QCOMPARE(head(), before);
  QVERIFY(SvnRepo::hasConflicts(_wc));
  QVERIFY(SvnRepo::getIsMergeInProgress(_wc));

  // Abort: the operation's files back to BASE, nothing in progress, local
  // work outside the operation and untracked files kept (§3.11.1).
  QVERIFY2(SvnRepo::abortMerge(_wc), qPrintable(lastErrorMessage()));
  QCOMPARE(QString::fromUtf8(readFile(file("a.txt"))), baseContent);
  QVERIFY(!SvnRepo::hasConflicts(_wc));
  QVERIFY(!SvnRepo::getIsMergeInProgress(_wc));
  QCOMPARE(readFile(file("untracked.txt")), QByteArray("untracked\n"));
  QCOMPARE(readFile(file("other.txt")), QByteArray("local\n"));
  const StatusMap oracle = svnStatusXml(_wc);
  QVERIFY(!oracle.contains(QStringLiteral(".")));
  QVERIFY(!oracle.contains(QStringLiteral("a.txt")));
  QVERIFY(changelistMembers(svnStatusXml(_wc, true)).isEmpty());
  QCOMPARE(head(), before);
}

void SvnRepoTest::abortMergeWithoutOperationFails_data() {
  QTest::addColumn<QString>("state");
  QTest::newRow("clean") << QStringLiteral("clean");
  QTest::newRow("local edits") << QStringLiteral("local edits");
  QTest::newRow("conflicted update") << QStringLiteral("conflicted update");
}

void SvnRepoTest::abortMergeWithoutOperationFails() {
  QFETCH(QString, state);

  QVERIFY(!commitA("base\n", QStringLiteral("Base")).isEmpty());
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  QByteArray content = "base\n";
  if (state == QLatin1String("local edits")) {
    content = "mine\n";
    QVERIFY(writeFile(file("a.txt"), content));
  } else if (state == QLatin1String("conflicted update")) {
    const QString other = _base + QStringLiteral("/other");
    QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk"), other));
    QVERIFY(writeFile(other + QStringLiteral("/a.txt"), "theirs\n"));
    svnIn(other, {QStringLiteral("commit"), QStringLiteral("-m"),
                  QStringLiteral("Theirs")},
          &ok);
    QVERIFY(writeFile(file("a.txt"), "mine\n"));
    svnIn(_wc, {QStringLiteral("update"), QStringLiteral("--accept"),
                QStringLiteral("postpone")});
    content = readFile(file("a.txt"));
  }

  // Nothing to abort: an error, and nothing is touched.
  resetLastError();
  const bool aborted = SvnRepo::abortMerge(_wc);
  QVERIFY(!aborted);
  QCOMPARE(lastError(), SvnRepo::SvnError::AbortMergeNothingToAbort);
  QCOMPARE(readFile(file("a.txt")), content);
}

void SvnRepoTest::abortMergeTwiceFails() {
  QVERIFY(!makeConflictingBranches(_wc, _rootUrl, _base).isEmpty());
  SvnRepo::merge(_wc, QStringLiteral("feature"), QStringLiteral("trunk"),
                 kAuthor, kEmail);
  QVERIFY(SvnRepo::hasConflicts(_wc));
  QVERIFY2(SvnRepo::abortMerge(_wc), qPrintable(lastErrorMessage()));

  resetLastError();
  QVERIFY(!SvnRepo::abortMerge(_wc));
  QCOMPARE(lastError(), SvnRepo::SvnError::AbortMergeNothingToAbort);
  QVERIFY(!lastErrorMessage().isEmpty());
}

void SvnRepoTest::mergeInProgressState() {
  QVERIFY(!makeConflictingBranches(_wc, _rootUrl, _base).isEmpty());
  QVERIFY(!SvnRepo::getIsMergeInProgress(_wc));
  SvnRepo::merge(_wc, QStringLiteral("feature"), QStringLiteral("trunk"),
                 kAuthor, kEmail);
  QVERIFY(SvnRepo::getIsMergeInProgress(_wc));

  // It survives closing and reopening the repository.
  SvnRepo::closeRepository(_wc);
  QVERIFY(!SvnRepo::openRepository(_wc).isEmpty());
  QVERIFY(SvnRepo::getIsMergeInProgress(_wc));

  // Resolved, staged and committed: the merge is over.
  QVERIFY(writeFile(file("a.txt"), "resolved\n"));
  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("a.txt")}));
  QVERIFY2(SvnRepo::commitStaged(_wc, QStringLiteral("Merged"), QString(),
                                 kAuthor, kEmail, true, false, false),
           qPrintable(lastErrorMessage()));
  QVERIFY(!SvnRepo::getIsMergeInProgress(_wc));
  // The merge's svn:mergeinfo is part of the resolution commit.
  QVERIFY(changedPaths(_repoDir, head()).contains(QStringLiteral("_U trunk")));
}

void SvnRepoTest::mergeInProgressFromCli() {
  QVERIFY(writeFile(file("a.txt"), "base\n"));
  QVERIFY(svnCommitAll(_wc, QStringLiteral("Base")));
  bool ok = false;
  svn({QStringLiteral("copy"), QStringLiteral("-m"), QStringLiteral("Branch"),
       _rootUrl + QStringLiteral("/trunk"),
       _rootUrl + QStringLiteral("/branches/feature")},
      &ok);
  const QString feature = _base + QStringLiteral("/feature");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/branches/feature"), feature));
  QVERIFY(writeFile(feature + QStringLiteral("/b.txt"), "b\n"));
  QVERIFY(svnCommitAll(feature, QStringLiteral("Feature")));
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  svnIn(_wc, {QStringLiteral("merge"),
              _rootUrl + QStringLiteral("/branches/feature")},
        &ok);
  QVERIFY(ok);

  // Documented: a clean merge made with the CLI leaves uncommitted changes
  // but no operation SvnRepo knows about.
  QVERIFY(!SvnRepo::getIsMergeInProgress(_wc));
  QVERIFY(status().hasChanges());
}

void SvnRepoTest::resolveTreeConflict_data() {
  QTest::addColumn<QString>("conflict");
  QTest::addColumn<bool>("keepLocal");
  QTest::newRow("edit vs delete, keep local")
      << QStringLiteral("edit vs delete") << true;
  QTest::newRow("edit vs delete, take incoming")
      << QStringLiteral("edit vs delete") << false;
  QTest::newRow("delete vs edit, keep local")
      << QStringLiteral("delete vs edit") << true;
  QTest::newRow("delete vs edit, take incoming")
      << QStringLiteral("delete vs edit") << false;
}

void SvnRepoTest::resolveTreeConflict() {
  QFETCH(QString, conflict);
  QFETCH(bool, keepLocal);

  QVERIFY(writeFile(file("unrelated.txt"), "u\n"));
  QVERIFY(!commitA("base\n", QStringLiteral("Base")).isEmpty());
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  const QString other = _base + QStringLiteral("/other");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk"), other));
  if (conflict == QLatin1String("edit vs delete")) {
    svnIn(other, {QStringLiteral("delete"), QStringLiteral("a.txt")});
    QVERIFY(writeFile(file("a.txt"), "mine\n"));
  } else {
    QVERIFY(writeFile(other + QStringLiteral("/a.txt"), "theirs\n"));
    svnIn(_wc, {QStringLiteral("delete"), QStringLiteral("a.txt")});
  }
  svnIn(other, {QStringLiteral("commit"), QStringLiteral("-m"),
                QStringLiteral("Theirs")},
        &ok);
  QVERIFY(ok);
  svnIn(_wc, {QStringLiteral("update"), QStringLiteral("--accept"),
              QStringLiteral("postpone")});
  QVERIFY(SvnRepo::isTreeConflict(_wc, QStringLiteral("a.txt")));
  // Another change in the repository the resolution must not pull in.
  QVERIFY(writeFile(other + QStringLiteral("/unrelated.txt"), "newer\n"));
  svnIn(other, {QStringLiteral("commit"), QStringLiteral("-m"),
                QStringLiteral("Unrelated")},
        &ok);
  QVERIFY(ok);

  QVERIFY2(SvnRepo::resolveTreeConflict(_wc, QStringLiteral("a.txt"),
                                        keepLocal),
           qPrintable(lastErrorMessage()));
  QVERIFY(!SvnRepo::hasConflicts(_wc));
  // No backup copy is left behind as an unversioned file.
  const QStringList newFiles = status().newFiles;
  QVERIFY2(!newFiles.contains(QStringLiteral("a.txt.local")),
           qPrintable(newFiles.join(", ")));
  QCOMPARE(readFile(file("unrelated.txt")), QByteArray("u\n"));
  if (conflict == QLatin1String("edit vs delete")) {
    QCOMPARE(QFileInfo::exists(file("a.txt")), keepLocal);
  } else {
    QCOMPARE(QFileInfo::exists(file("a.txt")), !keepLocal);
  }
}

// --- 3.7 Diff, blame, file size ---------------------------------------------

void SvnRepoTest::diffCategories_data() {
  QTest::addColumn<QString>("kind");
  for (const char *kind :
       {"modified", "added", "unversioned", "deleted", "missing", "property",
        "binary", "empty file", "no newline", "crlf native", "keywords",
        "unicode", "over the size limit"}) {
    QTest::newRow(kind) << QString::fromLatin1(kind);
  }
}

void SvnRepoTest::diffCategories() {
  QFETCH(QString, kind);

  QString path = QStringLiteral("a.txt");
  QVERIFY(writeFile(file(path), "one\ntwo\n"));
  bool ok = false;
  svnIn(_wc, {QStringLiteral("add"), path}, &ok);
  if (kind == QLatin1String("binary")) {
    svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("svn:mime-type"),
                QStringLiteral("application/octet-stream"), path});
  } else if (kind == QLatin1String("crlf native")) {
    svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("svn:eol-style"),
                QStringLiteral("native"), path});
  } else if (kind == QLatin1String("keywords")) {
    QVERIFY(writeFile(file(path), "$Rev$\none\ntwo\n"));
    svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("svn:keywords"),
                QStringLiteral("Rev"), path});
  }
  svnIn(_wc, {QStringLiteral("commit"), QStringLiteral("-m"),
              QStringLiteral("Base")},
        &ok);
  QVERIFY(ok);

  QStringList expected;
  QString repoContent = QStringLiteral("one\ntwo\n");
  QString filesContent;
  std::unique_ptr<ConfigOverride> limit;
  if (kind == QLatin1String("modified") || kind == QLatin1String("binary")) {
    QVERIFY(writeFile(file(path), "one\nTWO\n"));
    filesContent = QStringLiteral("one\nTWO\n");
    if (kind == QLatin1String("modified")) {
      expected = QStringList{QStringLiteral("-two"), QStringLiteral("+TWO")};
    }
  } else if (kind == QLatin1String("added") ||
             kind == QLatin1String("unversioned") ||
             kind == QLatin1String("empty file")) {
    path = QStringLiteral("new.txt");
    const QByteArray content =
        kind == QLatin1String("empty file") ? QByteArray()
                                            : QByteArray("x\ny\n");
    QVERIFY(writeFile(file(path), content));
    if (kind != QLatin1String("unversioned")) {
      svnIn(_wc, {QStringLiteral("add"), path});
    }
    repoContent.clear();
    filesContent = QString::fromUtf8(content);
    if (!content.isEmpty()) {
      expected = QStringList{QStringLiteral("+x"), QStringLiteral("+y")};
    }
  } else if (kind == QLatin1String("deleted") ||
             kind == QLatin1String("missing")) {
    if (kind == QLatin1String("deleted")) {
      svnIn(_wc, {QStringLiteral("delete"), path});
    } else {
      QFile::remove(file(path));
    }
    expected = QStringList{QStringLiteral("-one"), QStringLiteral("-two")};
  } else if (kind == QLatin1String("property")) {
    svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("p"),
                QStringLiteral("v"), path});
    filesContent = repoContent;
  } else if (kind == QLatin1String("no newline")) {
    QVERIFY(writeFile(file(path), "one\nTWO"));
    filesContent = QStringLiteral("one\nTWO");
    expected = QStringList{QStringLiteral("-two"), QStringLiteral("+TWO")};
  } else if (kind == QLatin1String("crlf native")) {
    QVERIFY(writeFile(file(path), "one\r\nTWO\r\n"));
    repoContent = QString::fromUtf8(readFile(file(path))).replace(
        QStringLiteral("TWO"), QStringLiteral("two"));
    filesContent = QStringLiteral("one\r\nTWO\r\n");
    expected = QStringList{QStringLiteral("-two"), QStringLiteral("+TWO")};
  } else if (kind == QLatin1String("keywords")) {
    const QByteArray expanded = readFile(file(path));
    QVERIFY(expanded.startsWith("$Rev: "));
    QVERIFY(writeFile(file(path), QByteArray(expanded).replace("two", "TWO")));
    repoContent = QString::fromUtf8(expanded);
    filesContent = QString::fromUtf8(readFile(file(path)));
    expected = QStringList{QStringLiteral("-two"), QStringLiteral("+TWO")};
  } else if (kind == QLatin1String("unicode")) {
    QVERIFY(writeFile(file(path), "one\n\xe6\x97\xa5\xe6\x9c\xac\n"));
    filesContent = QString::fromUtf16(u"one\n日本\n");
    expected = QStringList{QStringLiteral("-two"),
                           QString::fromUtf16(u"+日本")};
  } else if (kind == QLatin1String("over the size limit")) {
    limit = std::make_unique<ConfigOverride>(
        ConfigManager::ConfigKey::GitRepoMaxDiffFileSize, qlonglong(4));
    QVERIFY(writeFile(file(path), "one\nTWO\n"));
    // Contents over the limit are left out.
    repoContent.clear();
    filesContent.clear();
    expected = QStringList{QStringLiteral("-two"), QStringLiteral("+TWO")};
  }

  const SvnRepo::FilesDiff diff = SvnRepo::diffFiles(_wc, {path});
  QVERIFY2(diff.valid, qPrintable(lastErrorMessage()));
  QStringList actual;
  for (const SvnRepo::FilesDiffDelta &delta : diff.deltas) {
    QCOMPARE(delta.newFilePath, path);
    actual += changedLines(delta);
  }
  QCOMPARE(actual, expected);
  QCOMPARE(diff.repoContent.value(path), repoContent);
  QCOMPARE(diff.filesContent.value(path), filesContent);
}

void SvnRepoTest::diffUnusualFileNames_data() { unusualNames(); }

void SvnRepoTest::diffUnusualFileNames() {
  QFETCH(QString, name);

  QVERIFY(writeFile(file(name), "old\n"));
  QVERIFY2(commitAll(_wc, QStringLiteral("Base")),
           qPrintable(lastErrorMessage()));
  QVERIFY(writeFile(file(name), "new\n"));

  const SvnRepo::FilesDiff diff = SvnRepo::diffFiles(_wc, {name});
  QVERIFY2(diff.valid, qPrintable(lastErrorMessage()));
  QCOMPARE(diff.deltas.size(), 1);
  QCOMPARE(diff.deltas.first().newFilePath, name);
  QCOMPARE(changedLines(diff.deltas.first()),
           (QStringList{QStringLiteral("-old"), QStringLiteral("+new")}));
  QCOMPARE(diff.repoContent.value(name), QStringLiteral("old\n"));
}

void SvnRepoTest::diffOutsideWorkingCopy() {
  QVERIFY(!commitA("old\n", QStringLiteral("Base")).isEmpty());
  QVERIFY(writeFile(file("a.txt"), "new\n"));
  QVERIFY(writeFile(_base + QStringLiteral("/outside.txt"), "o\n"));

  // One bad file doesn't hide the others' diffs, or the error names it.
  resetLastError();
  const SvnRepo::FilesDiff diff = SvnRepo::diffFiles(
      _wc, {QStringLiteral("a.txt"), QStringLiteral("../outside.txt")});
  if (diff.valid) {
    QVERIFY(!diff.deltas.isEmpty());
    QCOMPARE(diff.deltas.first().newFilePath, QStringLiteral("a.txt"));
  } else {
    QVERIFY2(lastErrorMessage().contains(QStringLiteral("outside.txt")),
             qPrintable(lastErrorMessage()));
  }
}

void SvnRepoTest::diffTextIsAValidPatch() {
  QVERIFY(!commitA("one\ntwo\n", QStringLiteral("Base")).isEmpty());
  QVERIFY(writeFile(file("a.txt"), "one\nTWO\n"));
  QVERIFY(writeFile(file("new.txt"), "new\n"));

  const QString text =
      SvnRepo::diffFiles(_wc, {QStringLiteral("a.txt"),
                               QStringLiteral("new.txt")})
          .diff;
  QVERIFY(text.contains(QStringLiteral("diff --git a/a.txt b/a.txt")));
  QVERIFY2(text.contains(QStringLiteral("--- a/a.txt\n+++ b/a.txt")),
           qPrintable(text));
  QVERIFY2(text.contains(QStringLiteral("--- /dev/null\n+++ b/new.txt")),
           qPrintable(text));
}

void SvnRepoTest::diffFromCommit_data() {
  QTest::addColumn<QString>("change");
  for (const char *change :
       {"added", "modified", "deleted", "property", "copied", "r0",
        "not a revision", "HEAD"}) {
    QTest::newRow(change) << QString::fromLatin1(change);
  }
}

void SvnRepoTest::diffFromCommit() {
  QFETCH(QString, change);

  QVERIFY(!commitA("one\ntwo\n", QStringLiteral("Base")).isEmpty());
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  QString path = QStringLiteral("a.txt");
  QString commit;
  QStringList expected;
  if (change == QLatin1String("added")) {
    path = QStringLiteral("new.txt");
    QVERIFY(writeFile(file(path), "x\ny\n"));
    QVERIFY(commitAll(_wc, QStringLiteral("Add")));
    expected = QStringList{QStringLiteral("+x"), QStringLiteral("+y")};
  } else if (change == QLatin1String("modified")) {
    QVERIFY(writeFile(file(path), "one\nTWO\n"));
    QVERIFY(commitAll(_wc, QStringLiteral("Modify")));
    expected = QStringList{QStringLiteral("-two"), QStringLiteral("+TWO")};
  } else if (change == QLatin1String("deleted")) {
    svnIn(_wc, {QStringLiteral("delete"), path});
    QVERIFY(commitAll(_wc, QStringLiteral("Delete")));
    expected = QStringList{QStringLiteral("-one"), QStringLiteral("-two")};
  } else if (change == QLatin1String("property")) {
    svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("p"),
                QStringLiteral("v"), path});
    QVERIFY(commitAll(_wc, QStringLiteral("Property")));
  } else if (change == QLatin1String("copied")) {
    svnIn(_wc, {QStringLiteral("copy"), QStringLiteral("a.txt"),
                QStringLiteral("copy.txt")});
    path = QStringLiteral("copy.txt");
    QVERIFY(SvnRepo::stageFiles(_wc, {path}));
    QVERIFY(SvnRepo::commitStaged(_wc, QStringLiteral("Copy"), QString(),
                                  kAuthor, kEmail, true, false, false));
  } else {
    commit = change == QLatin1String("r0") ? QStringLiteral("0")
             : change == QLatin1String("HEAD") ? QStringLiteral("HEAD")
                                               : QStringLiteral("nonsense");
  }
  if (commit.isEmpty()) {
    commit = head();
  }

  resetLastError();
  const SvnRepo::FilesDiff diff =
      SvnRepo::diffFilesFromCommit(_wc, {path}, commit);
  if (change == QLatin1String("r0") ||
      change == QLatin1String("not a revision") ||
      change == QLatin1String("HEAD")) {
    // Nothing to compare: refused, with a reason.
    QVERIFY(!diff.valid);
    QVERIFY(!lastErrorMessage().isEmpty());
    return;
  }
  QVERIFY2(diff.valid, qPrintable(lastErrorMessage()));
  QStringList actual;
  for (const SvnRepo::FilesDiffDelta &delta : diff.deltas) {
    actual += changedLines(delta);
  }
  QCOMPARE(actual, expected);
}

void SvnRepoTest::diffFromCommitAbsolutePaths() {
  const QStringList names{QStringLiteral("a b.txt"),
                          QString::fromUtf16(u"ünï.txt"),
                          QStringLiteral("100%.txt")};
  for (const QString &name : names) {
    QVERIFY(writeFile(file(name), "x\n"));
  }
  QVERIFY(commitAll(_wc, QStringLiteral("Names")));

  // The file lists are relative and feed straight back into the diff.
  const SvnRepo::CommitStatus commit = SvnRepo::getCommitStatus(_wc, head());
  QCOMPARE(sorted(commit.newFiles), sorted(names));
  const SvnRepo::FilesDiff diff =
      SvnRepo::diffFilesFromCommit(_wc, commit.newFiles, head());
  QVERIFY(diff.valid);
  QCOMPARE(diff.deltas.size(), names.size());
  for (const SvnRepo::FilesDiffDelta &delta : diff.deltas) {
    QCOMPARE(changedLines(delta), QStringList{QStringLiteral("+x")});
  }
}

void SvnRepoTest::createPatchFromCommitLayouts_data() {
  QTest::addColumn<QString>("checkout");
  QTest::addColumn<QString>("header");
  QTest::newRow("trunk") << QStringLiteral("trunk")
                         << QStringLiteral("a/a.txt");
  QTest::newRow("branch") << QStringLiteral("branches/b")
                          << QStringLiteral("a/a.txt");
  QTest::newRow("branch with space") << QStringLiteral("branches/my branch")
                                     << QStringLiteral("a/a.txt");
  QTest::newRow("repository root") << QString()
                                   << QStringLiteral("a/trunk/a.txt");
}

void SvnRepoTest::createPatchFromCommitLayouts() {
  QFETCH(QString, checkout);
  QFETCH(QString, header);

  QVERIFY(writeFile(file("a.txt"), "one\n"));
  QVERIFY(svnCommitAll(_wc, QStringLiteral("Base")));
  bool ok = false;
  for (const char *branch : {"b", "my branch"}) {
    svn({QStringLiteral("copy"), QStringLiteral("-m"), QStringLiteral("Branch"),
         _rootUrl + QStringLiteral("/trunk"),
         _rootUrl + QStringLiteral("/branches/") + QString::fromLatin1(branch)},
        &ok);
    QVERIFY(ok);
  }
  const QString wc = _base + QStringLiteral("/co");
  QVERIFY(secondWc(_rootUrl + QLatin1Char('/') + checkout, wc));
  const QString target =
      wc + (checkout.isEmpty() ? QStringLiteral("/trunk/a.txt")
                               : QStringLiteral("/a.txt"));
  QVERIFY(writeFile(target, "one\ntwo\n"));
  QVERIFY(svnCommitAll(wc, QStringLiteral("Change")));

  const QString patch = SvnRepo::createPatchFromCommit(wc, head());
  QVERIFY2(patch.contains(QStringLiteral("diff --git ") + header + " b" +
                          header.mid(1)),
           qPrintable(patch));
  QVERIFY(patch.contains(QStringLiteral("+two")));
}

void SvnRepoTest::blameCases_data() {
  QTest::addColumn<QString>("kind");
  for (const char *kind :
       {"committed", "locally modified", "locally added", "binary",
        "empty file", "older revision", "shelf name", "crlf"}) {
    QTest::newRow(kind) << QString::fromLatin1(kind);
  }
}

void SvnRepoTest::blameCases() {
  QFETCH(QString, kind);

  QString path = QStringLiteral("a.txt");
  QByteArray content = "one\ntwo\n";
  if (kind == QLatin1String("crlf")) {
    content = "one\r\ntwo\r\n";
  } else if (kind == QLatin1String("empty file")) {
    content.clear();
  }
  const QString first = commitA(content, QStringLiteral("First"));
  QVERIFY(!first.isEmpty());
  if (kind == QLatin1String("binary")) {
    bool ok = false;
    svnIn(_wc, {QStringLiteral("update")}, &ok);
    svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("svn:mime-type"),
                QStringLiteral("application/octet-stream"), path});
    QVERIFY(svnCommitAll(_wc, QStringLiteral("Binary")));
  }
  QString commit;
  int lines = content.count('\n');
  if (kind == QLatin1String("locally modified")) {
    QVERIFY(writeFile(file(path), "one\nTWO\n"));
  } else if (kind == QLatin1String("locally added")) {
    path = QStringLiteral("new.txt");
    QVERIFY(writeFile(file(path), "x\n"));
    QVERIFY(SvnRepo::stageFiles(_wc, {path}));
  } else if (kind == QLatin1String("older revision")) {
    QVERIFY(!commitA("one\ntwo\nthree\n", QStringLiteral("Second")).isEmpty());
    commit = first;
  } else if (kind == QLatin1String("shelf name")) {
    commit = QStringLiteral("no-such-shelf");
  }

  resetLastError();
  const SvnRepo::BlameReport report = SvnRepo::blame(_wc, {path}, commit);
  if (kind == QLatin1String("shelf name")) {
    // Not a revision and not a shelf: not the working copy's blame.
    QVERIFY(!report.valid || !report.fileInfos.first().errorMessage.isEmpty());
    return;
  }
  QVERIFY2(report.valid, qPrintable(lastErrorMessage()));
  QCOMPARE(report.fileInfos.size(), 1);
  const SvnRepo::BlameFileInfo &info = report.fileInfos.first();
  if (kind == QLatin1String("locally added") ||
      kind == QLatin1String("binary")) {
    // Reported per file.
    QVERIFY(!info.errorMessage.isEmpty());
    return;
  }
  QVERIFY2(info.errorMessage.isEmpty(), qPrintable(info.errorMessage));
  QCOMPARE(info.lineInfos.size(), lines);
  if (kind == QLatin1String("locally modified")) {
    QCOMPARE(info.lineInfos[0].commit, first);
    QVERIFY(info.lineInfos[1].commit.isEmpty());
    QCOMPARE(info.lineInfos[1].content, QStringLiteral("TWO"));
  } else if (lines > 0) {
    QCOMPARE(info.lineInfos[0].commit, first);
    QCOMPARE(info.lineInfos[1].content, QStringLiteral("two"));
  }
}

void SvnRepoTest::blameLineCountMatchesFile_data() {
  QTest::addColumn<QByteArray>("content");
  QTest::addColumn<int>("lines");
  QTest::newRow("newline at end") << QByteArray("a\nb\nc\n") << 3;
  QTest::newRow("no newline at end") << QByteArray("a\nb\nc") << 3;
  QTest::newRow("empty last line") << QByteArray("a\nb\n\n") << 3;
  QTest::newRow("one line") << QByteArray("a") << 1;
}

void SvnRepoTest::blameLineCountMatchesFile() {
  QFETCH(QByteArray, content);
  QFETCH(int, lines);

  QVERIFY(!commitA(content, QStringLiteral("Base")).isEmpty());
  const SvnRepo::BlameReport report =
      SvnRepo::blame(_wc, {QStringLiteral("a.txt")}, QString());
  QVERIFY(report.valid);
  QCOMPARE(report.fileInfos.first().lineInfos.size(), lines);
}

void SvnRepoTest::fileSizeFromCommit_data() {
  QTest::addColumn<QString>("path");
  QTest::addColumn<QString>("commit");
  QTest::addColumn<qint64>("size"); // -1 when it must fail
  QTest::newRow("file") << QStringLiteral("a.txt") << QString() << qint64(6);
  QTest::newRow("folder") << QStringLiteral("dir") << QString() << qint64(-1);
  QTest::newRow("missing path") << QStringLiteral("none.txt") << QString()
                                << qint64(-1);
  QTest::newRow("invalid revision") << QStringLiteral("a.txt")
                                    << QStringLiteral("abc") << qint64(-1);
}

void SvnRepoTest::fileSizeFromCommit() {
  QFETCH(QString, path);
  QFETCH(QString, commit);
  QFETCH(qint64, size);

  QVERIFY(writeFile(file("dir/x.txt"), "x\n"));
  QVERIFY(!commitA("12345\n", QStringLiteral("Base")).isEmpty());
  if (commit.isEmpty()) {
    commit = head();
  }

  resetLastError();
  QCOMPARE(SvnRepo::getFileSizeFromCommit(_wc, path, commit), size);
  if (size < 0) {
    QVERIFY(!lastErrorMessage().isEmpty());
  }
}

// --- 3.8 History ------------------------------------------------------------

void SvnRepoTest::historyLinearParents() {
  QStringList revisions{QStringLiteral("1")};
  for (int i = 0; i < 4; ++i) {
    revisions.prepend(
        commitA(QByteArray::number(i) + '\n', QStringLiteral("C%1").arg(i)));
  }

  const SvnRepo::SvnHistory history = SvnRepo::getBranchHistory(
      _wc, QStringLiteral("trunk"), SvnRepo::BranchType::Local, -1, _quit);
  QVERIFY(history.valid);
  QCOMPARE(QStringList(history.commits.begin(), history.commits.end()),
           revisions);
  for (int i = 0; i < revisions.size(); ++i) {
    const SvnRepo::SvnCommitInfo &info = history.commitInfos[revisions[i]];
    QCOMPARE(info.revision, revisions[i].toLongLong());
    QCOMPARE(info.parents, i + 1 < revisions.size()
                               ? QVector<QString>{revisions[i + 1]}
                               : QVector<QString>());
    QCOMPARE(info.children,
             i > 0 ? QVector<QString>{revisions[i - 1]} : QVector<QString>());
  }
}

void SvnRepoTest::historyFollowsCopies() {
  const QString onTrunk = commitA("trunk\n", QStringLiteral("On trunk"));
  bool ok = false;
  svn({QStringLiteral("copy"), QStringLiteral("-m"), QStringLiteral("Branch"),
       _rootUrl + QStringLiteral("/trunk"),
       _rootUrl + QStringLiteral("/branches/b")},
      &ok);
  QVERIFY(ok);
  const QString branchWc = _base + QStringLiteral("/b");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/branches/b"), branchWc));
  QVERIFY(writeFile(branchWc + QStringLiteral("/a.txt"), "branch\n"));
  QVERIFY(commitAll(branchWc, QStringLiteral("On branch")));

  // The branch's history goes on into trunk before the copy.
  const SvnRepo::SvnHistory history = SvnRepo::getBranchHistory(
      branchWc, QString(), SvnRepo::BranchType::All, -1, _quit);
  QVERIFY(history.valid);
  QVERIFY(history.commits.contains(onTrunk));
  QVERIFY(history.commits.contains(QStringLiteral("1")));

  // And those trunk revisions can be looked at from the branch.
  const SvnRepo::FilesDiff diff = SvnRepo::diffFilesFromCommit(
      branchWc, {QStringLiteral("a.txt")}, onTrunk);
  QVERIFY(diff.valid);
  QStringList lines;
  for (const SvnRepo::FilesDiffDelta &delta : diff.deltas) {
    lines += changedLines(delta);
  }
  QCOMPARE(lines, QStringList{QStringLiteral("+trunk")});
  QCOMPARE(SvnRepo::getFileSizeFromCommit(branchWc, QStringLiteral("a.txt"),
                                          onTrunk),
           qint64(6));
  SvnRepo::closeRepository(branchWc);
}

void SvnRepoTest::historyPaging_data() {
  QTest::addColumn<int>("pageSize");
  QTest::addColumn<int>("page");
  QTest::addColumn<int>("count");
  QTest::addColumn<bool>("hasMore");
  // Six revisions on trunk: r1 (layout) and five commits.
  QTest::newRow("size 1, page 0") << 1 << 0 << 1 << true;
  QTest::newRow("size 2, page 1") << 2 << 1 << 2 << true;
  QTest::newRow("size 2, last page") << 2 << 2 << 2 << false;
  QTest::newRow("size 3, exact boundary") << 3 << 1 << 3 << false;
  QTest::newRow("past the end") << 2 << 5 << 0 << false;
  QTest::newRow("every revision") << 2 << -1 << 6 << false;
}

void SvnRepoTest::historyPaging() {
  QFETCH(int, pageSize);
  QFETCH(int, page);
  QFETCH(int, count);
  QFETCH(bool, hasMore);

  for (int i = 0; i < 5; ++i) {
    QVERIFY(!commitA(QByteArray::number(i) + '\n', QStringLiteral("C%1").arg(i))
                 .isEmpty());
  }
  const ConfigOverride size(ConfigManager::ConfigKey::GitRepoHistoryPageSize,
                            qlonglong(pageSize));
  const SvnRepo::SvnHistory all = SvnRepo::getBranchHistory(
      _wc, QStringLiteral("trunk"), SvnRepo::BranchType::Local, -1, _quit);
  QCOMPARE(all.commits.size(), 6);

  const SvnRepo::SvnHistory history = SvnRepo::getBranchHistory(
      _wc, QStringLiteral("trunk"), SvnRepo::BranchType::Local, page, _quit);
  QVERIFY(history.valid);
  QCOMPARE(history.commits.size(), count);
  QCOMPARE(history.hasMore, hasMore);
  if (page >= 0 && count > 0) {
    // The page is exactly its slice of the whole history.
    QCOMPARE(history.commits, all.commits.mid(page * pageSize, count));
  }
}

void SvnRepoTest::historyRevpropEdgeCases() {
  const QString url =
      loadDump(_base + QStringLiteral("/edges"), Dumps::revpropEdgeCases());
  QVERIFY(!url.isEmpty());
  const QString wc = _base + QStringLiteral("/edgeswc");
  QVERIFY(secondWc(url + QStringLiteral("/trunk"), wc));

  const SvnRepo::SvnHistory history = SvnRepo::getBranchHistory(
      wc, QString(), SvnRepo::BranchType::All, -1, _quit);
  QVERIFY2(history.valid, qPrintable(lastErrorMessage()));
  QCOMPARE(history.commits.size(), 6);
  const auto info = [&](int revision) {
    return history.commitInfos.value(QString::number(revision));
  };
  QCOMPARE(info(2).authorName, QString());
  QCOMPARE(info(3).timeStamp, qint64(-1));
  QCOMPARE(info(4).message, QString());
  QCOMPARE(info(4).summary, QString());
  // A CRLF log: the summary has no CR, and neither has the message.
  QCOMPARE(info(5).summary, QStringLiteral("Summary"));
  QVERIFY(!info(5).message.contains(QLatin1Char('\r')));
  // A log that isn't UTF-8 still gives a summary.
  QVERIFY(!info(6).summary.isEmpty());
  SvnRepo::closeRepository(wc);
}

void SvnRepoTest::historyTimestamps() {
  const QString revision = commitA("a\n", QStringLiteral("Now"));
  const SvnRepo::SvnHistory history = SvnRepo::getHistory(_wc, 1);
  const qint64 timeStamp = history.commitInfos.value(revision).timeStamp;
  // "2026-10-22T01:02:03.123456Z" with microseconds, cut to milliseconds.
  const QDateTime expected = QDateTime::fromString(
      svnLogXml(_rootUrl, revision).date.left(23) + QLatin1Char('Z'),
      Qt::ISODateWithMs);
  QVERIFY(expected.isValid());
  // Milliseconds since the epoch, UTC.
  QCOMPARE(timeStamp, expected.toMSecsSinceEpoch());
  QVERIFY(qAbs(timeStamp - QDateTime::currentMSecsSinceEpoch()) < 60 * 1000);
}

void SvnRepoTest::historyAfterBranchDeletedOrRenamed_data() {
  QTest::addColumn<bool>("rename");
  QTest::newRow("deleted") << false;
  QTest::newRow("renamed") << true;
}

void SvnRepoTest::historyAfterBranchDeletedOrRenamed() {
  QFETCH(bool, rename);

  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
  QVERIFY(SvnRepo::createBranchFromCommit(_wc, QString(), QStringLiteral("b")));
  QVERIFY(SvnRepo::checkoutBranch(_wc, QStringLiteral("b"),
                                  SvnRepo::BranchType::Local));
  bool ok = false;
  if (rename) {
    svn({QStringLiteral("move"), QStringLiteral("-m"), QStringLiteral("Move"),
         _rootUrl + QStringLiteral("/branches/b"),
         _rootUrl + QStringLiteral("/branches/c")},
        &ok);
  } else {
    svn({QStringLiteral("delete"), QStringLiteral("-m"),
         QStringLiteral("Delete"), _rootUrl + QStringLiteral("/branches/b")},
        &ok);
  }
  QVERIFY(ok);

  // The working copy's own history still works, or fails with a reason.
  resetLastError();
  const SvnRepo::SvnHistory history = SvnRepo::getBranchHistory(
      _wc, QString(), SvnRepo::BranchType::All, -1, _quit);
  QVERIFY2(history.valid, qPrintable(lastErrorMessage()));
  if (!history.valid) {
    QVERIFY(!lastErrorMessage().isEmpty());
  }
}

void SvnRepoTest::historyOtherBranchesOnly() {
  const QString onTrunk = commitA("a\n", QStringLiteral("Trunk"));
  bool ok = false;
  svn({QStringLiteral("copy"), QStringLiteral("-m"), QStringLiteral("Branch"),
       _rootUrl + QStringLiteral("/trunk"),
       _rootUrl + QStringLiteral("/branches/b")},
      &ok);
  const QString branchWc = _base + QStringLiteral("/b");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/branches/b"), branchWc));
  QVERIFY(writeFile(branchWc + QStringLiteral("/a.txt"), "branch\n"));
  QVERIFY(svnCommitAll(branchWc, QStringLiteral("Branch only")));
  const QString onBranch = head();

  const SvnRepo::SvnHistory history = SvnRepo::getBranchHistory(
      _wc, QStringLiteral("trunk"), SvnRepo::BranchType::Local, -1, _quit);
  QVERIFY(history.commits.contains(onTrunk));
  QVERIFY(!history.commits.contains(onBranch));
  QVERIFY(!history.commits.contains(QString::number(onBranch.toInt() - 1)));
}

void SvnRepoTest::historyCancelled() {
  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
  _quit = true;
  resetLastError();
  const SvnRepo::SvnHistory history = SvnRepo::getBranchHistory(
      _wc, QString(), SvnRepo::BranchType::All, -1, _quit);
  QVERIFY(!history.valid);
  QVERIFY(history.commits.isEmpty());
  QCOMPARE(lastError(), SvnRepo::SvnError::Cancelled);
  _quit = false;
}

void SvnRepoTest::historyProjectRootLayout() {
  const QString url = loadDump(_base + QStringLiteral("/project"),
                               Dumps::projectRootLayout());
  QVERIFY(!url.isEmpty());
  const QString wc = _base + QStringLiteral("/projwc");
  QVERIFY(secondWc(url + QStringLiteral("/proj/trunk"), wc));
  QVERIFY(writeFile(wc + QStringLiteral("/a.txt"), "changed\n"));
  QVERIFY(commitAll(wc, QStringLiteral("Change")));

  const SvnRepo::SvnHistory history = SvnRepo::getBranchHistory(
      wc, QString(), SvnRepo::BranchType::All, -1, _quit);
  QVERIFY(history.valid);
  QCOMPARE(history.commitInfos.value(history.commits.first()).branches,
           QVector<QString>{QStringLiteral("trunk")});
  QCOMPARE(SvnRepo::getActiveBranch(wc, SvnRepo::BranchType::Local, _quit),
           QStringLiteral("trunk"));
  SvnRepo::closeRepository(wc);
}

void SvnRepoTest::historyRepoWithSpaceInPath() {
  const QString repoDir = _base + QStringLiteral("/my repo");
  const QString url = makeRepo(repoDir, Layout::Standard);
  QVERIFY(!url.isEmpty());
  const QString wc = _base + QStringLiteral("/spacewc");
  QVERIFY(secondWc(url + QStringLiteral("/trunk"), wc));
  QVERIFY(writeFile(wc + QStringLiteral("/a.txt"), "a\n"));
  QVERIFY(commitAll(wc, QStringLiteral("Add")));

  const SvnRepo::SvnHistory history = SvnRepo::getBranchHistory(
      wc, QString(), SvnRepo::BranchType::All, -1, _quit);
  QVERIFY(history.valid);
  QCOMPARE(history.commitInfos.value(history.commits.first()).branches,
           QVector<QString>{QStringLiteral("trunk")});
  QCOMPARE(SvnRepo::getCommitStatus(wc, history.commits.first()).newFiles,
           QStringList{QStringLiteral("a.txt")});
  SvnRepo::closeRepository(wc);
}

void SvnRepoTest::commitStatusFileLists_data() {
  QTest::addColumn<QString>("change");
  QTest::addColumn<QStringList>("newFiles");
  QTest::addColumn<QStringList>("modifiedFiles");
  QTest::addColumn<QStringList>("deletedFiles");
  QTest::newRow("add") << QStringLiteral("add") << QStringList{"new.txt"}
                       << QStringList() << QStringList();
  QTest::newRow("modify") << QStringLiteral("modify") << QStringList()
                          << QStringList{"a.txt"} << QStringList();
  QTest::newRow("delete") << QStringLiteral("delete") << QStringList()
                          << QStringList() << QStringList{"a.txt"};
  QTest::newRow("replace") << QStringLiteral("replace") << QStringList()
                           << QStringList{"a.txt"} << QStringList();
  QTest::newRow("move") << QStringLiteral("move") << QStringList{"moved.txt"}
                        << QStringList() << QStringList{"a.txt"};
  QTest::newRow("copy") << QStringLiteral("copy") << QStringList{"copy.txt"}
                        << QStringList() << QStringList();
  QTest::newRow("several files")
      << QStringLiteral("several")
      << QStringList{"b.txt", "c.txt", "d/e.txt", "z.txt"} << QStringList()
      << QStringList();
  QTest::newRow("root property") << QStringLiteral("root property")
                                 << QStringList() << QStringList()
                                 << QStringList();
  QTest::newRow("branch creation") << QStringLiteral("branch") << QStringList()
                                   << QStringList() << QStringList();
  QTest::newRow("other branch")
      << QStringLiteral("other branch")
      << QStringList{"/branches/b/x.txt"} << QStringList() << QStringList();
}

void SvnRepoTest::commitStatusFileLists() {
  QFETCH(QString, change);
  QFETCH(QStringList, newFiles);
  QFETCH(QStringList, modifiedFiles);
  QFETCH(QStringList, deletedFiles);

  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  if (change == QLatin1String("add")) {
    QVERIFY(writeFile(file("new.txt"), "n\n"));
    svnIn(_wc, {QStringLiteral("add"), QStringLiteral("new.txt")});
  } else if (change == QLatin1String("modify")) {
    QVERIFY(writeFile(file("a.txt"), "changed\n"));
  } else if (change == QLatin1String("delete")) {
    svnIn(_wc, {QStringLiteral("delete"), QStringLiteral("a.txt")});
  } else if (change == QLatin1String("replace")) {
    svnIn(_wc, {QStringLiteral("delete"), QStringLiteral("a.txt")});
    QVERIFY(writeFile(file("a.txt"), "new node\n"));
    svnIn(_wc, {QStringLiteral("add"), QStringLiteral("a.txt")});
  } else if (change == QLatin1String("move")) {
    svnIn(_wc, {QStringLiteral("move"), QStringLiteral("a.txt"),
                QStringLiteral("moved.txt")});
  } else if (change == QLatin1String("copy")) {
    svnIn(_wc, {QStringLiteral("copy"), QStringLiteral("a.txt"),
                QStringLiteral("copy.txt")});
  } else if (change == QLatin1String("several")) {
    for (const QString &name : newFiles) {
      QVERIFY(writeFile(file(name), "x\n"));
    }
    svnIn(_wc, {QStringLiteral("add"), QStringLiteral("--force"),
                QStringLiteral(".")});
  } else if (change == QLatin1String("root property")) {
    svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("p"),
                QStringLiteral("v"), QStringLiteral(".")});
  }
  if (change == QLatin1String("branch") ||
      change == QLatin1String("other branch")) {
    svn({QStringLiteral("copy"), QStringLiteral("-m"),
         QStringLiteral("Branch"), _rootUrl + QStringLiteral("/trunk"),
         _rootUrl + QStringLiteral("/branches/b")},
        &ok);
    QVERIFY(ok);
    if (change == QLatin1String("other branch")) {
      const QString branchWc = _base + QStringLiteral("/b");
      QVERIFY(secondWc(_rootUrl + QStringLiteral("/branches/b"), branchWc));
      QVERIFY(writeFile(branchWc + QStringLiteral("/x.txt"), "x\n"));
      QVERIFY(svnCommitAll(branchWc, QStringLiteral("On the branch")));
    }
  } else {
    svnIn(_wc, {QStringLiteral("commit"), QStringLiteral("-m"),
                QStringLiteral("Change")},
          &ok);
    QVERIFY(ok);
  }

  // Files of the revision relative to the working copy, sorted; other
  // repository paths keep their leading slash. Folders are not files.
  const SvnRepo::CommitStatus commit = SvnRepo::getCommitStatus(_wc, head());
  QCOMPARE(commit.commit, head());
  QCOMPARE(commit.newFiles, newFiles);
  QCOMPARE(commit.modifiedFiles, modifiedFiles);
  QCOMPARE(commit.deletedFiles, deletedFiles);
}

void SvnRepoTest::commitStatusOfInvalidRevision_data() {
  QTest::addColumn<QString>("commit");
  QTest::newRow("above head") << QStringLiteral("99");
  QTest::newRow("letters") << QStringLiteral("abc");
  QTest::newRow("negative") << QStringLiteral("-1");
  QTest::newRow("bare r") << QStringLiteral("r");
  QTest::newRow("empty") << QString();
  QTest::newRow("plus sign") << QStringLiteral("+2");
  QTest::newRow("git-like id") << QStringLiteral("1234567");
  QTest::newRow("above 2^32") << QStringLiteral("4294967298");
}

void SvnRepoTest::commitStatusOfInvalidRevision() {
  QFETCH(QString, commit);

  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
  resetLastError();
  const SvnRepo::CommitStatus status = SvnRepo::getCommitStatus(_wc, commit);
  QVERIFY(status.commit.isEmpty());
  QCOMPARE(lastError(), SvnRepo::SvnError::InvalidRevision);
}

void SvnRepoTest::commitBranchesTagsHeads() {
  const QString url = loadDump(_base + QStringLiteral("/branches"),
                               Dumps::branchesAndTags());
  QVERIFY(!url.isEmpty());
  const QString wc = _base + QStringLiteral("/bwc");
  QVERIFY(secondWc(url + QStringLiteral("/trunk"), wc));

  // r4 changed both branches.
  QCOMPARE(SvnRepo::getCommitBranches(wc, QStringLiteral("4"), _quit),
           (QStringList{QStringLiteral("b1"), QStringLiteral("b2")}));
  QCOMPARE(SvnRepo::getCommitHeads(wc, QStringLiteral("4"), _quit),
           (QStringList{QStringLiteral("b1"), QStringLiteral("b2")}));
  // v1 was copied from trunk@1.
  QCOMPARE(SvnRepo::getCommitTags(wc, QStringLiteral("1"), _quit),
           QStringList{QStringLiteral("v1")});

  // A tag made at HEAD belongs to trunk's last change, not to HEAD.
  QVERIFY(SvnRepo::createTagFromCommit(wc, QString(), QStringLiteral("v2"),
                                       kAuthor, kEmail, QString()));
  QVERIFY(SvnRepo::getCommitTags(wc, QStringLiteral("1"), _quit)
              .contains(QStringLiteral("v2")));

  // A renamed tag keeps its link.
  QVERIFY(SvnRepo::renameTag(wc, QStringLiteral("v1"), QStringLiteral("v1b")));
  QVERIFY(SvnRepo::getCommitTags(wc, QStringLiteral("1"), _quit)
              .contains(QStringLiteral("v1b")));
  SvnRepo::closeRepository(wc);

  // Project-root layout.
  const QString project = loadDump(_base + QStringLiteral("/project"),
                                   Dumps::projectRootLayout());
  const QString projWc = _base + QStringLiteral("/projwc");
  QVERIFY(secondWc(project + QStringLiteral("/proj/trunk"), projWc));
  QVERIFY(SvnRepo::createTagFromCommit(projWc, QStringLiteral("1"),
                                       QStringLiteral("p1"), kAuthor, kEmail,
                                       QString()));
  QCOMPARE(SvnRepo::getCommitTags(projWc, QStringLiteral("1"), _quit),
           QStringList{QStringLiteral("p1")});
  SvnRepo::closeRepository(projWc);
}

void SvnRepoTest::commitIsHeadAfterOwnCommit() {
  const QString revision = commitA("a\n", QStringLiteral("Own"));
  QVERIFY(SvnRepo::getCommitIsHead(_wc, revision, _quit));
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  QVERIFY(SvnRepo::getCommitIsHead(_wc, revision, _quit));
}

// --- 3.9 Branches -----------------------------------------------------------

void SvnRepoTest::listBranchesLayouts_data() {
  QTest::addColumn<QString>("layout");
  QTest::addColumn<QString>("active");
  QTest::newRow("trunk") << QStringLiteral("trunk") << QStringLiteral("trunk");
  QTest::newRow("project trunk") << QStringLiteral("project trunk")
                                 << QStringLiteral("trunk");
  QTest::newRow("project root") << QStringLiteral("project root") << QString();
  QTest::newRow("no trunk") << QStringLiteral("no trunk") << QString();
  QTest::newRow("repository root") << QStringLiteral("repository root")
                                   << QString();
  QTest::newRow("branches folder") << QStringLiteral("branches folder")
                                   << QString();
}

void SvnRepoTest::listBranchesLayouts() {
  QFETCH(QString, layout);
  QFETCH(QString, active);

  QVERIFY(SvnRepo::createBranchFromCommit(_wc, QString(), QStringLiteral("b")));
  QString url = _rootUrl + QStringLiteral("/trunk");
  if (layout.startsWith(QLatin1String("project"))) {
    const QString project = loadDump(_base + QStringLiteral("/project"),
                                     Dumps::projectRootLayout());
    QVERIFY(!project.isEmpty());
    url = project + (layout == QLatin1String("project trunk")
                         ? QStringLiteral("/proj/trunk")
                         : QStringLiteral("/proj"));
  } else if (layout == QLatin1String("no trunk")) {
    url = loadDump(_base + QStringLiteral("/flat"), Dumps::noTrunkLayout());
  } else if (layout == QLatin1String("repository root")) {
    url = _rootUrl;
  } else if (layout == QLatin1String("branches folder")) {
    url = _rootUrl + QStringLiteral("/branches");
  }
  const QString wc = _base + QStringLiteral("/layoutwc");
  QVERIFY(secondWc(url, wc));

  // Outside trunk, branches and tags there is no active branch, and no error
  // (§7.4 item 8).
  resetLastError();
  const QString activeBranch =
      SvnRepo::getActiveBranch(wc, SvnRepo::BranchType::Local, _quit);
  QCOMPARE(activeBranch, active);
  QCOMPARE(lastError(), SvnRepo::SvnError::Unknown);
  if (layout == QLatin1String("trunk")) {
    QCOMPARE(SvnRepo::listLocalBranches(wc, _quit),
             (QStringList{QStringLiteral("trunk"), QStringLiteral("b")}));
    QCOMPARE(SvnRepo::listRemoteBranches(wc, _quit),
             QStringList{QStringLiteral("b")});
  }
  if (!active.isEmpty()) {
    SvnRepo::closeRepository(wc);
    return;
  }

  // Branch operations that need an active branch are refused.
  resetLastError();
  const bool created =
      SvnRepo::createBranchFromCommit(wc, QString(), QStringLiteral("x"));
  QVERIFY(!created);
  QCOMPARE(lastError(), SvnRepo::SvnError::InvalidArgument);
  SvnRepo::closeRepository(wc);
}

void SvnRepoTest::branchNames_data() {
  QTest::addColumn<QString>("name");
  QTest::addColumn<bool>("valid");
  QTest::newRow("plain") << QStringLiteral("feature") << true;
  // Subversion has no nested branches (B7h): a folder below branches/ is one
  // branch.
  QTest::newRow("nested") << QStringLiteral("feature/x") << false;
  QTest::newRow("space") << QStringLiteral("my branch") << true;
  QTest::newRow("latin") << QString::fromUtf16(u"été") << true;
  QTest::newRow("cjk") << QString::fromUtf16(u"日本") << true;
  QTest::newRow("at") << QStringLiteral("a@b") << true;
  QTest::newRow("percent") << QStringLiteral("100%") << true;
  QTest::newRow("hash") << QStringLiteral("#x") << true;
  QTest::newRow("arg marker") << QStringLiteral("%2") << true;
  QTest::newRow("trunk") << QStringLiteral("trunk") << false;
  QTest::newRow("empty") << QString() << false;
  QTest::newRow("dot dot") << QStringLiteral("..") << false;
  QTest::newRow("backslash") << QStringLiteral("a\\b") << false;
}

void SvnRepoTest::branchNames() {
  QFETCH(QString, name);
  QFETCH(bool, valid);

  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
  const QString before = head();

  resetLastError();
  const bool created = SvnRepo::createBranchFromCommit(_wc, QString(), name);
  const QStringList malfunctions = SvnRepo::Testing::takeMalfunctions();
  QVERIFY2(malfunctions.isEmpty(), qPrintable(malfunctions.join('\n')));
  if (!valid) {
    // Refused, and nothing is made or moved.
    QVERIFY(!created);
    QCOMPARE(lastError(), SvnRepo::SvnError::InvalidArgument);
    QCOMPARE(svn({QStringLiteral("ls"), _rootUrl}).split('\n'),
             (QStringList{QStringLiteral("branches/"), QStringLiteral("tags/"),
                          QStringLiteral("trunk/")}));
    return;
  }

  // Created, listed, checked out, renamed and deleted under its own name.
  QVERIFY2(created, qPrintable(lastErrorMessage()));
  QCOMPARE(head().toInt(), before.toInt() + 1);
  QVERIFY2(SvnRepo::listRemoteBranches(_wc, _quit).contains(name),
           qPrintable(SvnRepo::listRemoteBranches(_wc, _quit).join(", ")));
  const bool switched =
      SvnRepo::checkoutBranch(_wc, name, SvnRepo::BranchType::Local);
  const QStringList switchMalfunctions = SvnRepo::Testing::takeMalfunctions();
  // B2h (fixed): the branch URL is encoded.
  QVERIFY2(switched && switchMalfunctions.isEmpty(),
           qPrintable(lastErrorMessage()));
  QCOMPARE(SvnRepo::getActiveBranch(_wc, SvnRepo::BranchType::Local, _quit),
           name);
  QVERIFY(SvnRepo::checkoutBranch(_wc, QStringLiteral("trunk"),
                                  SvnRepo::BranchType::Local));
  const QString renamed = name + QStringLiteral("-2");
  QVERIFY2(SvnRepo::renameBranch(_wc, name, renamed),
           qPrintable(lastErrorMessage()));
  QCOMPARE(SvnRepo::listRemoteBranches(_wc, _quit), QStringList{renamed});
  QVERIFY2(SvnRepo::deleteBranch(_wc, renamed),
           qPrintable(lastErrorMessage()));
  QVERIFY(SvnRepo::listRemoteBranches(_wc, _quit).isEmpty());
}

void SvnRepoTest::createBranchFromCommitRevisions_data() {
  QTest::addColumn<QString>("commit");
  QTest::addColumn<QByteArray>("content"); // empty when it must fail
  QTest::newRow("HEAD") << QString() << QByteArray("two\n");
  QTest::newRow("r prefix") << QStringLiteral("r2") << QByteArray("one\n");
  QTest::newRow("number") << QStringLiteral("3") << QByteArray("two\n");
  QTest::newRow("r0") << QStringLiteral("0") << QByteArray();
  QTest::newRow("future") << QStringLiteral("99") << QByteArray();
  QTest::newRow("existing name") << QStringLiteral("<existing>")
                                 << QByteArray();
}

void SvnRepoTest::createBranchFromCommitRevisions() {
  QFETCH(QString, commit);
  QFETCH(QByteArray, content);

  QVERIFY(!commitA("one\n", QStringLiteral("One")).isEmpty());
  QVERIFY(!commitA("two\n", QStringLiteral("Two")).isEmpty());
  if (commit == QLatin1String("<existing>")) {
    QVERIFY(
        SvnRepo::createBranchFromCommit(_wc, QString(), QStringLiteral("b")));
    commit.clear();
  }
  const QString before = head();

  resetLastError();
  const bool created =
      SvnRepo::createBranchFromCommit(_wc, commit, QStringLiteral("b"));
  if (content.isEmpty()) {
    QVERIFY(!created);
    QVERIFY(!lastErrorMessage().isEmpty());
    QCOMPARE(head(), before);
    return;
  }
  QVERIFY2(created, qPrintable(lastErrorMessage()));
  QCOMPARE(QString::fromUtf8(content).trimmed(),
           svn({QStringLiteral("cat"),
                _rootUrl + QStringLiteral("/branches/b/a.txt")}));
}

void SvnRepoTest::branchesFromSubdirectoryWc() {
  QVERIFY(writeFile(file("src/main.c"), "main\n"));
  QVERIFY(writeFile(file("README"), "readme\n"));
  QVERIFY(svnCommitAll(_wc, QStringLiteral("Base")));
  bool ok = false;
  svn({QStringLiteral("copy"), QStringLiteral("-m"), QStringLiteral("Branch"),
       _rootUrl + QStringLiteral("/trunk"),
       _rootUrl + QStringLiteral("/branches/b")},
      &ok);
  QVERIFY(ok);
  const QString srcWc = _base + QStringLiteral("/src");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk/src"), srcWc));

  // A working copy of trunk/src switches to branches/b/src.
  QCOMPARE(SvnRepo::getActiveBranch(srcWc, SvnRepo::BranchType::Local, _quit),
           QStringLiteral("trunk"));
  const bool switched = SvnRepo::checkoutBranch(srcWc, QStringLiteral("b"),
                                                SvnRepo::BranchType::Local);
  QVERIFY2(switched, qPrintable(lastErrorMessage()));
  QCOMPARE(svnInfo(srcWc, QStringLiteral("url")),
           _rootUrl + QStringLiteral("/branches/b/src"));

  // A branch or tag made there holds the whole branch, not just src.
  QVERIFY(SvnRepo::createBranchFromCommit(srcWc, QString(),
                                          QStringLiteral("c")));
  QVERIFY(SvnRepo::createTagFromCommit(srcWc, QString(), QStringLiteral("t"),
                                       kAuthor, kEmail, QString()));
  QVERIFY(svn({QStringLiteral("ls"), _rootUrl + QStringLiteral("/branches/c")})
              .contains(QStringLiteral("README")));
  QVERIFY(svn({QStringLiteral("ls"), _rootUrl + QStringLiteral("/tags/t")})
              .contains(QStringLiteral("README")));
  SvnRepo::closeRepository(srcWc);
}

void SvnRepoTest::checkoutBranchCases() {
  QVERIFY(!commitA("base\n", QStringLiteral("Base")).isEmpty());
  QVERIFY(SvnRepo::createBranchFromCommit(_wc, QString(), QStringLiteral("b")));
  bool ok = false;
  svn({QStringLiteral("mkdir"), QStringLiteral("-m"), QStringLiteral("Unrelated"),
       _rootUrl + QStringLiteral("/branches/unrelated")},
      &ok);
  QVERIFY(ok);

  // The current branch: nothing to do.
  QVERIFY(SvnRepo::checkoutBranch(_wc, QStringLiteral("trunk"),
                                  SvnRepo::BranchType::Local));
  QCOMPARE(SvnRepo::getActiveBranch(_wc, SvnRepo::BranchType::Local, _quit),
           QStringLiteral("trunk"));

  // A branch that doesn't exist, and one without common history: refused,
  // and the working copy stays on trunk.
  for (const QString &name :
       {QStringLiteral("missing"), QStringLiteral("unrelated")}) {
    resetLastError();
    QVERIFY(!SvnRepo::checkoutBranch(_wc, name, SvnRepo::BranchType::Local));
    EXPECT_SVN_ERROR(SvnRepo::SvnError::SwitchFailed);
    QCOMPARE(svnInfo(_wc, QStringLiteral("url")),
             _rootUrl + QStringLiteral("/trunk"));
  }

  // Remote and All mean the same branch as Local.
  QVERIFY(SvnRepo::checkoutBranch(_wc, QStringLiteral("b"),
                                  SvnRepo::BranchType::Remote));
  QCOMPARE(svnInfo(_wc, QStringLiteral("url")),
           _rootUrl + QStringLiteral("/branches/b"));
  QVERIFY(SvnRepo::checkoutBranch(_wc, QStringLiteral("trunk"),
                                  SvnRepo::BranchType::All));
  QCOMPARE(svnInfo(_wc, QStringLiteral("url")),
           _rootUrl + QStringLiteral("/trunk"));

  // A local edit that conflicts with the branch is reported.
  const QString branchWc = _base + QStringLiteral("/bwc");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/branches/b"), branchWc));
  QVERIFY(writeFile(branchWc + QStringLiteral("/a.txt"), "branch\n"));
  QVERIFY(svnCommitAll(branchWc, QStringLiteral("On b")));
  QVERIFY(writeFile(file("a.txt"), "local\n"));
  resetLastError();
  const bool switched = SvnRepo::checkoutBranch(_wc, QStringLiteral("b"),
                                                SvnRepo::BranchType::Local);
  QVERIFY(SvnRepo::hasConflicts(_wc));
  QVERIFY(!switched);
  SvnRepo::closeRepository(branchWc);
}

void SvnRepoTest::deleteBranchGuards_data() {
  QTest::addColumn<QString>("name");
  QTest::newRow("empty") << QString();
  QTest::newRow("trunk") << QStringLiteral("trunk");
  QTest::newRow("active") << QStringLiteral("b");
}

void SvnRepoTest::deleteBranchGuards() {
  QFETCH(QString, name);

  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
  QVERIFY(SvnRepo::createBranchFromCommit(_wc, QString(), QStringLiteral("b")));
  QVERIFY(SvnRepo::checkoutBranch(_wc, QStringLiteral("b"),
                                  SvnRepo::BranchType::Local));
  const QString before = head();

  // None of them may go: not trunk, not /branches, not the checked out one.
  resetLastError();
  const bool deleted = SvnRepo::deleteBranch(_wc, name);
  QVERIFY(!deleted);
  QCOMPARE(head(), before);
  QCOMPARE(svn({QStringLiteral("ls"), _rootUrl + QStringLiteral("/branches")}),
           QStringLiteral("b/"));
}

void SvnRepoTest::renameActiveBranch() {
  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
  QVERIFY(SvnRepo::createBranchFromCommit(_wc, QString(), QStringLiteral("b")));
  QVERIFY(SvnRepo::checkoutBranch(_wc, QStringLiteral("b"),
                                  SvnRepo::BranchType::Local));

  // The working copy follows its branch.
  QVERIFY2(SvnRepo::renameBranch(_wc, QStringLiteral("b"), QStringLiteral("c")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(svnInfo(_wc, QStringLiteral("url")),
           _rootUrl + QStringLiteral("/branches/c"));
  QCOMPARE(SvnRepo::getActiveBranch(_wc, SvnRepo::BranchType::Local, _quit),
           QStringLiteral("c"));
  QVERIFY(svnStatusXml(_wc).isEmpty());
}

void SvnRepoTest::renameOntoExistingBranch() {
  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
  QVERIFY(SvnRepo::createBranchFromCommit(_wc, QString(), QStringLiteral("b")));
  QVERIFY(SvnRepo::createBranchFromCommit(_wc, QString(), QStringLiteral("c")));
  const QString before = head();

  // Refused; neither branch changes.
  resetLastError();
  const bool renamed =
      SvnRepo::renameBranch(_wc, QStringLiteral("b"), QStringLiteral("c"));
  QVERIFY(!renamed);
  QVERIFY(!lastErrorMessage().isEmpty());
  QCOMPARE(head(), before);
  QCOMPARE(svn({QStringLiteral("ls"), _rootUrl + QStringLiteral("/branches")})
               .split('\n'),
           (QStringList{QStringLiteral("b/"), QStringLiteral("c/")}));
}

void SvnRepoTest::renameTrunk() {
  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
  const QString before = head();

  // trunk is the layout, not a branch that can be renamed.
  resetLastError();
  const bool renamed =
      SvnRepo::renameBranch(_wc, QStringLiteral("trunk"), QStringLiteral("x"));
  QVERIFY(!renamed);
  QCOMPARE(head(), before);
  QCOMPARE(svnInfo(_rootUrl + QStringLiteral("/trunk"), QStringLiteral("kind")),
           QStringLiteral("dir"));
}

void SvnRepoTest::remoteBranchesWithStatus() {
  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
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
  QStringList names;
  for (const SvnRepo::BranchInfo &info :
       SvnRepo::listRemoteBranchesWithStatus(_wc, _quit)) {
    names += info.name;
  }
  QVERIFY2(names.contains(QStringLiteral("trunk")), qPrintable(names.join(", ")));
  QCOMPARE(statusOf(QStringLiteral("b")).status,
           SvnRepo::BranchStatusType::UpToDate);

  // The user's own commit doesn't make the branch Behind.
  QVERIFY(writeFile(file("a.txt"), "mine\n"));
  QVERIFY(commitAll(_wc, QStringLiteral("Own")));
  QCOMPARE(statusOf(QStringLiteral("b")).status,
           SvnRepo::BranchStatusType::UpToDate);

  // Another working copy's commit does.
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  const QString other = _base + QStringLiteral("/other");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/branches/b"), other));
  QVERIFY(writeFile(other + QStringLiteral("/a.txt"), "theirs\n"));
  QVERIFY(svnCommitAll(other, QStringLiteral("Theirs")));
  const SvnRepo::BranchStatus behind = statusOf(QStringLiteral("b"));
  QCOMPARE(behind.status, SvnRepo::BranchStatusType::Behind);
  QCOMPARE(behind.behindCount, size_t(1));
}

// --- 3.10 Tags --------------------------------------------------------------

void SvnRepoTest::tagLifecycle_data() {
  QTest::addColumn<QString>("name");
  QTest::newRow("plain") << QStringLiteral("v1.0");
  QTest::newRow("space") << QStringLiteral("my tag");
  QTest::newRow("latin") << QString::fromUtf16(u"été");
}

void SvnRepoTest::tagLifecycle() {
  QFETCH(QString, name);

  const QString first = commitA("one\n", QStringLiteral("One"));
  QVERIFY(!commitA("two\n", QStringLiteral("Two")).isEmpty());

  resetLastError();
  const bool created = SvnRepo::createTagFromCommit(_wc, first, name, kAuthor,
                                                    kEmail, QString());
  const QStringList malfunctions = SvnRepo::Testing::takeMalfunctions();
  QVERIFY2(malfunctions.isEmpty(), qPrintable(malfunctions.join('\n')));
  QVERIFY2(created, qPrintable(lastErrorMessage()));
  QCOMPARE(svn({QStringLiteral("cat"),
                _rootUrl + QStringLiteral("/tags/") + name +
                    QStringLiteral("/a.txt")}),
           QStringLiteral("one"));
  QCOMPARE(SvnRepo::listTags(_wc), QStringList{name});
  QCOMPARE(SvnRepo::getCommitTags(_wc, first, _quit), QStringList{name});

  const bool switched =
      SvnRepo::checkoutBranch(_wc, name, SvnRepo::BranchType::Tag);
  const QStringList switchMalfunctions = SvnRepo::Testing::takeMalfunctions();
  // B2h (fixed): the tag URL is encoded.
  QVERIFY2(switched && switchMalfunctions.isEmpty(),
           qPrintable(lastErrorMessage()));
  QCOMPARE(SvnRepo::getActiveBranch(_wc, SvnRepo::BranchType::Tag, _quit),
           name);
  QCOMPARE(readFile(file("a.txt")), QByteArray("one\n"));
  QVERIFY(SvnRepo::checkoutBranch(_wc, QStringLiteral("trunk"),
                                  SvnRepo::BranchType::Local));

  const QString renamed = name + QStringLiteral("-b");
  QVERIFY(SvnRepo::renameTag(_wc, name, renamed));
  QCOMPARE(SvnRepo::listTags(_wc), QStringList{renamed});
  QVERIFY(SvnRepo::deleteTag(_wc, renamed));
  QVERIFY(SvnRepo::listTags(_wc).isEmpty());
}

void SvnRepoTest::deleteTagGuards() {
  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
  QVERIFY(SvnRepo::createTagFromCommit(_wc, QString(), QStringLiteral("v1"),
                                       kAuthor, kEmail, QString()));
  const QString before = head();

  resetLastError();
  const bool deleted = SvnRepo::deleteTag(_wc, QString());
  const QStringList malfunctions = SvnRepo::Testing::takeMalfunctions();
  QVERIFY(!deleted);
  QVERIFY(!lastErrorMessage().isEmpty());
  QVERIFY2(malfunctions.isEmpty(), qPrintable(malfunctions.join('\n')));
  QCOMPARE(head(), before);
  QCOMPARE(SvnRepo::listTags(_wc), QStringList{QStringLiteral("v1")});

  // A nested tag name is refused (B7h), like a nested branch name.
  resetLastError();
  QVERIFY(!SvnRepo::createTagFromCommit(_wc, QString(),
                                        QStringLiteral("release/1.0"),
                                        kAuthor, kEmail, QString()));
  QCOMPARE(lastError(), SvnRepo::SvnError::InvalidArgument);
  QCOMPARE(head(), before);
}

void SvnRepoTest::tagFromTagWc() {
  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
  QVERIFY(SvnRepo::createTagFromCommit(_wc, QString(), QStringLiteral("v1"),
                                       kAuthor, kEmail, QString()));
  QVERIFY(SvnRepo::checkoutBranch(_wc, QStringLiteral("v1"),
                                  SvnRepo::BranchType::Tag));

  // A tag made from a tag's working copy copies that tag.
  QVERIFY2(SvnRepo::createTagFromCommit(_wc, QString(), QStringLiteral("v2"),
                                        kAuthor, kEmail, QString()),
           qPrintable(lastErrorMessage()));
  QCOMPARE(SvnRepo::listTags(_wc),
           (QStringList{QStringLiteral("v1"), QStringLiteral("v2")}));
  QCOMPARE(svn({QStringLiteral("cat"),
                _rootUrl + QStringLiteral("/tags/v2/a.txt")}),
           QStringLiteral("a"));

  // Commits to a checked out tag are refused.
  QVERIFY(writeFile(file("a.txt"), "changed on a tag\n"));
  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("a.txt")}));
  resetLastError();
  const bool committed =
      SvnRepo::commitStaged(_wc, QStringLiteral("On a tag"), QString(),
                            kAuthor, kEmail, true, false, false);
  QVERIFY(!committed);
}

void SvnRepoTest::tagMessage() {
  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
  QVERIFY(SvnRepo::createTagFromCommit(_wc, QString(), QStringLiteral("v1"),
                                       kAuthor, kEmail,
                                       QStringLiteral("#42 release")));
  QCOMPARE(svnlookRevprop(_repoDir, head(), QStringLiteral("svn:log")),
           QByteArray("#42 release"));

  // Without a message there is a default one.
  QVERIFY(SvnRepo::createTagFromCommit(_wc, QString(), QStringLiteral("v2"),
                                       kAuthor, kEmail, QString()));
  QCOMPARE(svnlookRevprop(_repoDir, head(), QStringLiteral("svn:log")),
           QByteArray("Create tag v2"));
}

void SvnRepoTest::pushTagAndPushAllTags() {
  // Documented: tags are made in the repository, so there is nothing to push.
  QVERIFY(SvnRepo::createTagFromCommit(_wc, QString(), QStringLiteral("v1"),
                                       kAuthor, kEmail, QString()));
  const QString before = head();
  QVERIFY(SvnRepo::pushTag(_wc, QStringLiteral("v1")));
  QVERIFY(SvnRepo::pushAllTags(_wc));
  QCOMPARE(head(), before);
}

// --- 3.11 Merge, cherry-pick, revert ----------------------------------------

namespace {

// trunk with a.txt "base", and branches/feature that adds feature.txt and
// changes b.txt. Returns the feature revision.
QString makeFeatureBranch(const QString &wc, const QString &rootUrl,
                          const QString &base) {
  if (!writeFile(wc + QStringLiteral("/a.txt"), "base\n") ||
      !writeFile(wc + QStringLiteral("/b.txt"), "b\n") ||
      !svnCommitAll(wc, QStringLiteral("Base"))) {
    return {};
  }
  bool ok = false;
  svn({QStringLiteral("copy"), QStringLiteral("-m"), QStringLiteral("Branch"),
       rootUrl + QStringLiteral("/trunk"),
       rootUrl + QStringLiteral("/branches/feature")},
      &ok);
  const QString feature = base + QStringLiteral("/feature");
  if (!ok ||
      !secondWc(rootUrl + QStringLiteral("/branches/feature"), feature) ||
      !writeFile(feature + QStringLiteral("/feature.txt"), "feature\n") ||
      !writeFile(feature + QStringLiteral("/b.txt"), "b on feature\n") ||
      !svnCommitAll(feature, QStringLiteral("Feature work"))) {
    return {};
  }
  svnIn(wc, {QStringLiteral("update")}, &ok);
  return svnInfo(rootUrl + QStringLiteral("/branches/feature"),
                 QStringLiteral("last-changed-revision"));
}

} // namespace

void SvnRepoTest::mergeCreatesMergeRevision() {
  QVERIFY(!makeFeatureBranch(_wc, _rootUrl, _base).isEmpty());
  const QString before = head();

  QVERIFY2(SvnRepo::merge(_wc, QStringLiteral("feature"),
                          QStringLiteral("trunk"), kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  // One revision with the branch's changes and the mergeinfo.
  QCOMPARE(head().toInt(), before.toInt() + 1);
  QCOMPARE(changedPaths(_repoDir, head()),
           (QStringList{QStringLiteral("A trunk/feature.txt"),
                        QStringLiteral("U trunk/b.txt"),
                        QStringLiteral("_U trunk")}));
  QCOMPARE(svnlookRevprop(_repoDir, head(), QStringLiteral("svn:log")),
           QByteArray("Merge branch 'feature' into 'trunk'"));
  QVERIFY(svnPropget(QStringLiteral("svn:mergeinfo"),
                     _rootUrl + QStringLiteral("/trunk"))
              .contains(QStringLiteral("/branches/feature")));
  QVERIFY(svnStatusXml(_wc).isEmpty());
  QVERIFY(!SvnRepo::getIsMergeInProgress(_wc));
}

void SvnRepoTest::mergeWithLocalChangeInTheWayFails() {
  QVERIFY(!makeFeatureBranch(_wc, _rootUrl, _base).isEmpty());
  const QString before = head();
  // A local change to a file the merge changes.
  QVERIFY(writeFile(file("b.txt"), "local\n"));

  resetLastError();
  const bool merged = SvnRepo::merge(_wc, QStringLiteral("feature"),
                                     QStringLiteral("trunk"), kAuthor, kEmail);
  QVERIFY(!merged);
  QCOMPARE(lastError(), SvnRepo::SvnError::LocalChangesInTheWay);
  QCOMPARE(readFile(file("b.txt")), QByteArray("local\n"));
  QCOMPARE(head(), before);
}

void SvnRepoTest::mergeAlreadyUpToDate() {
  QVERIFY(!makeFeatureBranch(_wc, _rootUrl, _base).isEmpty());
  QVERIFY(SvnRepo::merge(_wc, QStringLiteral("feature"),
                         QStringLiteral("trunk"), kAuthor, kEmail));
  const QString before = head();

  // Nothing left to merge: no revision, an "is empty" error.
  resetLastError();
  const bool merged = SvnRepo::merge(_wc, QStringLiteral("feature"),
                                     QStringLiteral("trunk"), kAuthor, kEmail);
  QVERIFY(!merged);
  QCOMPARE(lastError(), SvnRepo::SvnError::MergeIsEmpty);
  QCOMPARE(head(), before);
}

void SvnRepoTest::mergeIntoItself() {
  QVERIFY(!makeFeatureBranch(_wc, _rootUrl, _base).isEmpty());
  const QString before = head();

  resetLastError();
  const bool merged = SvnRepo::merge(_wc, QStringLiteral("trunk"),
                                     QStringLiteral("trunk"), kAuthor, kEmail);
  QVERIFY(!merged);
  QCOMPARE(lastError(), SvnRepo::SvnError::InvalidArgument);
  QCOMPARE(head(), before);
  QVERIFY(svnStatusXml(_wc).isEmpty());
}

void SvnRepoTest::mergeIntoOtherBranchChecksItOut() {
  QVERIFY(!makeFeatureBranch(_wc, _rootUrl, _base).isEmpty());
  QVERIFY(SvnRepo::createBranchFromCommit(_wc, QString(),
                                          QStringLiteral("release")));

  // The destination is checked out first, then merged into.
  QVERIFY2(SvnRepo::merge(_wc, QStringLiteral("feature"),
                          QStringLiteral("release"), kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QCOMPARE(SvnRepo::getActiveBranch(_wc, SvnRepo::BranchType::Local, _quit),
           QStringLiteral("release"));
  QVERIFY(changedPaths(_repoDir, head())
              .contains(QStringLiteral("A branches/release/feature.txt")));
  QVERIFY(svnStatusXml(_wc).isEmpty());
}

void SvnRepoTest::mergeFromSubdirectoryWc() {
  QVERIFY(writeFile(file("src/a.txt"), "base\n"));
  QVERIFY(svnCommitAll(_wc, QStringLiteral("Base")));
  bool ok = false;
  svn({QStringLiteral("copy"), QStringLiteral("-m"), QStringLiteral("Branch"),
       _rootUrl + QStringLiteral("/trunk"),
       _rootUrl + QStringLiteral("/branches/feature")},
      &ok);
  const QString feature = _base + QStringLiteral("/feature");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/branches/feature"), feature));
  QVERIFY(writeFile(feature + QStringLiteral("/src/a.txt"), "feature\n"));
  QVERIFY(svnCommitAll(feature, QStringLiteral("Feature")));
  const QString srcWc = _base + QStringLiteral("/src");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk/src"), srcWc));

  // Merged into the subfolder the working copy has.
  resetLastError();
  const bool merged = SvnRepo::merge(srcWc, QStringLiteral("feature"),
                                     QStringLiteral("trunk"), kAuthor, kEmail);
  QVERIFY2(merged && readFile(srcWc + QStringLiteral("/a.txt")) == "feature\n",
           qPrintable(lastErrorMessage()));
  SvnRepo::closeRepository(srcWc);
}

void SvnRepoTest::cherryPickCases_data() {
  QTest::addColumn<QString>("revision");
  QTest::addColumn<QString>("expectedFile"); // empty when it is empty
  QTest::newRow("feature revision") << QStringLiteral("<feature>")
                                    << QStringLiteral("feature.txt");
  QTest::newRow("own revision") << QStringLiteral("<own>") << QString();
  QTest::newRow("layout revision") << QStringLiteral("1") << QString();
  QTest::newRow("branch creation") << QStringLiteral("<branch>") << QString();
  QTest::newRow("tag only") << QStringLiteral("<tag>") << QString();
}

void SvnRepoTest::cherryPickCases() {
  QFETCH(QString, revision);
  QFETCH(QString, expectedFile);

  const QString featureRevision = makeFeatureBranch(_wc, _rootUrl, _base);
  QVERIFY(!featureRevision.isEmpty());
  if (revision == QLatin1String("<feature>")) {
    revision = featureRevision;
  } else if (revision == QLatin1String("<own>")) {
    QVERIFY(writeFile(file("a.txt"), "own\n"));
    QVERIFY(svnCommitAll(_wc, QStringLiteral("Own")));
    revision = head();
  } else if (revision == QLatin1String("<branch>")) {
    revision = QString::number(featureRevision.toInt() - 1);
  } else if (revision == QLatin1String("<tag>")) {
    QVERIFY(SvnRepo::createTagFromCommit(_wc, QString(), QStringLiteral("v1"),
                                         kAuthor, kEmail, QString()));
    revision = head();
  }
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  const QString before = head();

  resetLastError();
  const bool picked =
      SvnRepo::cherryPickCommit(_wc, revision, kAuthor, kEmail);
  if (!expectedFile.isEmpty()) {
    QVERIFY2(picked, qPrintable(lastErrorMessage()));
    QCOMPARE(head().toInt(), before.toInt() + 1);
    QVERIFY(changedPaths(_repoDir, head())
                .contains(QStringLiteral("A trunk/") + expectedFile));
    const QByteArray log =
        svnlookRevprop(_repoDir, head(), QStringLiteral("svn:log"));
    QVERIFY2(log.startsWith("Feature work"), log.constData());
    QVERIFY(log.contains("(cherry picked from r" + revision.toUtf8() + ")"));
    return;
  }
  // Nothing to pick: no revision, an "is empty" error.
  QVERIFY(!picked);
  QCOMPARE(lastError(), SvnRepo::SvnError::CherryPickIsEmpty);
  QCOMPARE(head(), before);
  QVERIFY(svnStatusXml(_wc).isEmpty());
}

void SvnRepoTest::cherryPickWithLocalChangeInTheWayFails() {
  const QString featureRevision = makeFeatureBranch(_wc, _rootUrl, _base);
  QVERIFY(!featureRevision.isEmpty());
  const QString before = head();
  QVERIFY(writeFile(file("b.txt"), "local\n"));

  resetLastError();
  const bool picked =
      SvnRepo::cherryPickCommit(_wc, featureRevision, kAuthor, kEmail);
  QVERIFY(!picked);
  QCOMPARE(lastError(), SvnRepo::SvnError::LocalChangesInTheWay);
  QCOMPARE(readFile(file("b.txt")), QByteArray("local\n"));
  QCOMPARE(head(), before);
}

void SvnRepoTest::twoBranchRevision() {
  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
  QVERIFY(SvnRepo::createBranchFromCommit(_wc, QString(), QStringLiteral("b")));
  QVERIFY(SvnRepo::createBranchFromCommit(_wc, QString(), QStringLiteral("c")));

  // One revision that adds x.txt to b and to c, with different contents.
  const QString branches = _base + QStringLiteral("/branches");
  bool ok = false;
  svn({QStringLiteral("checkout"), _rootUrl + QStringLiteral("/branches"),
       branches},
      &ok);
  QVERIFY(ok);
  QVERIFY(writeFile(branches + QStringLiteral("/b/x.txt"), "from b\n"));
  QVERIFY(writeFile(branches + QStringLiteral("/c/x.txt"), "from c\n"));
  svnIn(branches, {QStringLiteral("add"), QStringLiteral("b/x.txt"),
                   QStringLiteral("c/x.txt")},
        &ok);
  QVERIFY(ok);
  QVERIFY(svnCommitAll(branches, QStringLiteral("Both")));
  const QString both = head();
  svnIn(_wc, {QStringLiteral("update")}, &ok);

  // B10h (fixed): picked onto trunk, the change comes from one branch, the
  // first by name, and not from a hash-ordered list.
  QVERIFY2(SvnRepo::cherryPickCommit(_wc, both, kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("x.txt")), QByteArray("from b\n"));

  // Reverted on c, it is c's own change that is undone.
  QVERIFY(SvnRepo::checkoutBranch(_wc, QStringLiteral("c"),
                                  SvnRepo::BranchType::Local));
  QVERIFY2(SvnRepo::revertCommit(_wc, both, kAuthor, kEmail, QString()),
           qPrintable(lastErrorMessage()));
  QCOMPARE(changedPaths(_repoDir, head()),
           QStringList{QStringLiteral("D branches/c/x.txt")});
  QVERIFY(!QFileInfo::exists(file("x.txt")));
}

void SvnRepoTest::revertCommitCases_data() {
  QTest::addColumn<QString>("source");
  QTest::newRow("own revision") << QStringLiteral("own");
  QTest::newRow("never merged branch") << QStringLiteral("other branch");
  QTest::newRow("own revision, custom message") << QStringLiteral("message");
}

void SvnRepoTest::revertCommitCases() {
  QFETCH(QString, source);

  const QString featureRevision = makeFeatureBranch(_wc, _rootUrl, _base);
  QVERIFY(!featureRevision.isEmpty());
  QVERIFY(writeFile(file("a.txt"), "changed\n"));
  QVERIFY(svnCommitAll(_wc, QStringLiteral("Change a")));
  const QString own = head();
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  const QString before = head();

  resetLastError();
  if (source == QLatin1String("other branch")) {
    // The revision's change isn't on trunk: reverting it is empty.
    const bool reverted = SvnRepo::revertCommit(_wc, featureRevision, kAuthor,
                                                kEmail, QString());
    QVERIFY(!reverted);
    QCOMPARE(lastError(), SvnRepo::SvnError::RevertIsEmpty);
    QCOMPARE(head(), before);
    return;
  }
  const QString message =
      source == QLatin1String("message") ? QStringLiteral("Undo a") : QString();
  QVERIFY2(SvnRepo::revertCommit(_wc, own, kAuthor, kEmail, message),
           qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("a.txt")), QByteArray("base\n"));
  QCOMPARE(changedPaths(_repoDir, head()),
           QStringList{QStringLiteral("U trunk/a.txt")});
  const QByteArray log =
      svnlookRevprop(_repoDir, head(), QStringLiteral("svn:log"));
  if (message.isEmpty()) {
    QCOMPARE(log, "Revert \"Change a\"\n\nThis reverts revision r" +
                      own.toUtf8() + ".");
  } else {
    QCOMPARE(log, QByteArray("Undo a"));
  }
}

void SvnRepoTest::operationStateSurvivesReopen() {
  const QString featureRevision =
      makeConflictingBranches(_wc, _rootUrl, _base);
  QVERIFY(!featureRevision.isEmpty());
  SvnRepo::cherryPickCommit(_wc, featureRevision, kAuthor, kEmail);
  QVERIFY(SvnRepo::getIsMergeInProgress(_wc));

  SvnRepo::closeRepository(_wc);
  QVERIFY(!SvnRepo::openRepository(_wc).isEmpty());
  QVERIFY(SvnRepo::getIsMergeInProgress(_wc));

  // Resolved, staged and committed after the reopen: it is over.
  QVERIFY(writeFile(file("a.txt"), "resolved\n"));
  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("a.txt")}));
  QVERIFY2(SvnRepo::commitStaged(_wc, QStringLiteral("Picked"), QString(),
                                 kAuthor, kEmail, true, false, false),
           qPrintable(lastErrorMessage()));
  QVERIFY(!SvnRepo::getIsMergeInProgress(_wc));
  QVERIFY(changedPaths(_repoDir, head()).contains(QStringLiteral("U trunk/a.txt")));
}

void SvnRepoTest::authorOfPickedRevision() {
  const QString featureRevision = makeFeatureBranch(_wc, _rootUrl, _base);
  QVERIFY(!featureRevision.isEmpty());
  QCOMPARE(svnlookAuthor(_repoDir, featureRevision), kAuthor);
  QVERIFY(SvnRepo::cherryPickCommit(_wc, featureRevision, kAuthor, kEmail));

  // Documented: Subversion has one author per revision, the committer; the
  // picked revision gets the same author as any other commit of this
  // working copy, not the picked revision's.
  const QString picked = svnlookAuthor(_repoDir, head());
  QVERIFY(writeFile(file("a.txt"), "after\n"));
  QVERIFY(commitAll(_wc, QStringLiteral("After")));
  QCOMPARE(picked, svnlookAuthor(_repoDir, head()));
}

// --- 3.12 Stash (shelves) ---------------------------------------------------

void SvnRepoTest::stashCategories_data() {
  QTest::addColumn<QString>("kind");
  for (const char *kind :
       {"modified", "added file", "added folder", "deleted", "property",
        "binary", "unversioned", "copied", "tree conflict"}) {
    QTest::newRow(kind) << QString::fromLatin1(kind);
  }
}

void SvnRepoTest::stashCategories() {
  QFETCH(QString, kind);

  QVERIFY(writeFile(file("bin.dat"), QByteArray("\x00\x01\x02", 3)));
  QVERIFY(!commitA("base\n", QStringLiteral("Base")).isEmpty());
  bool ok = false;
  svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("svn:mime-type"),
              QStringLiteral("application/octet-stream"),
              QStringLiteral("bin.dat")});
  QVERIFY(svnCommitAll(_wc, QStringLiteral("Binary")));
  svnIn(_wc, {QStringLiteral("update")}, &ok);

  // Each change and how to see that it is in the working copy.
  QString path = QStringLiteral("a.txt");
  if (kind == QLatin1String("modified")) {
    QVERIFY(writeFile(file(path), "changed\n"));
  } else if (kind == QLatin1String("added file")) {
    path = QStringLiteral("new.txt");
    QVERIFY(writeFile(file(path), "new\n"));
    svnIn(_wc, {QStringLiteral("add"), path});
  } else if (kind == QLatin1String("added folder")) {
    path = QStringLiteral("dir/inner.txt");
    QVERIFY(writeFile(file(path), "inner\n"));
    svnIn(_wc, {QStringLiteral("add"), QStringLiteral("dir")});
  } else if (kind == QLatin1String("deleted")) {
    svnIn(_wc, {QStringLiteral("delete"), path});
  } else if (kind == QLatin1String("property")) {
    svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("p"),
                QStringLiteral("v"), path});
  } else if (kind == QLatin1String("binary")) {
    path = QStringLiteral("bin.dat");
    QVERIFY(writeFile(file(path), QByteArray("\x03\x04\x05\x00", 4)));
  } else if (kind == QLatin1String("unversioned")) {
    path = QStringLiteral("loose.txt");
    QVERIFY(writeFile(file(path), "loose\n"));
  } else if (kind == QLatin1String("copied")) {
    path = QStringLiteral("copy.txt");
    svnIn(_wc, {QStringLiteral("copy"), QStringLiteral("a.txt"), path});
  } else if (kind == QLatin1String("tree conflict")) {
    const QString other = _base + QStringLiteral("/other");
    QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk"), other));
    svnIn(other, {QStringLiteral("delete"), QStringLiteral("a.txt")});
    QVERIFY(svnCommitAll(other, QStringLiteral("Delete")));
    QVERIFY(writeFile(file(path), "mine\n"));
    svnIn(_wc, {QStringLiteral("update"), QStringLiteral("--accept"),
                QStringLiteral("postpone")});
  }
  const QByteArray content = readFile(file(path));
  const StatusMap before = svnStatusXml(_wc);
  QVERIFY(!before.isEmpty());

  // Stashed, or refused with a reason; never a silent partial stash.
  resetLastError();
  const bool stashed = SvnRepo::stash(_wc, QStringLiteral("note"));
  if (kind == QLatin1String("unversioned") ||
      kind == QLatin1String("tree conflict")) {
    QVERIFY2(!stashed || !lastErrorMessage().isEmpty(),
             "stash reported success without stashing the change");
    if (!stashed) {
      QVERIFY(!lastErrorMessage().isEmpty());
      QCOMPARE(svnStatusXml(_wc).keys(), before.keys());
    }
    return;
  }
  QEXPECT_FAIL("copied", "B19h: a copy can't be stashed", Abort);
  QVERIFY2(stashed, qPrintable(lastErrorMessage()));
  QVERIFY2(svnStatusXml(_wc).isEmpty(),
           qPrintable(svnStatusXml(_wc).keys().join(", ")));

  // Applying it brings back exactly what there was: nothing is lost.
  QVERIFY2(SvnRepo::applyStash(_wc, 0), qPrintable(lastErrorMessage()));
  const StatusMap after = svnStatusXml(_wc);
  QCOMPARE(after.keys(), before.keys());
  for (const QString &key : before.keys()) {
    QCOMPARE(after.value(key).item, before.value(key).item);
  }
  QCOMPARE(readFile(file(path)), content);
}

void SvnRepoTest::stashKeepsStagingOrDocumentsLoss() {
  QVERIFY(!commitA("base\n", QStringLiteral("Base")).isEmpty());
  QVERIFY(writeFile(file("a.txt"), "staged\n"));
  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("a.txt")}));

  QVERIFY(SvnRepo::stash(_wc, QString()));
  QVERIFY(SvnRepo::applyStash(_wc, 0));
  QCOMPARE(readFile(file("a.txt")), QByteArray("staged\n"));
  // Documented: the shelf keeps the changelist, so the file comes back
  // staged.
  QCOMPARE(status().modifiedStagedFiles, QStringList{QStringLiteral("a.txt")});
  QCOMPARE(changelistMembers(svnStatusXml(_wc)),
           QStringList{QStringLiteral("a.txt")});
}

void SvnRepoTest::stashTwiceInARow() {
  QVERIFY(writeFile(file("b.txt"), "b\n"));
  QVERIFY(!commitA("base\n", QStringLiteral("Base")).isEmpty());

  // Two stashes made straight after each other are two stashes. (B19h needs
  // both in the same millisecond, which the test can't force.)
  QVERIFY(writeFile(file("a.txt"), "first\n"));
  QVERIFY(SvnRepo::stash(_wc, QStringLiteral("first")));
  QVERIFY(writeFile(file("b.txt"), "second\n"));
  QVERIFY(SvnRepo::stash(_wc, QStringLiteral("second")));
  QCOMPARE(SvnRepo::listStashes(_wc, _quit).size(), 2);
}

void SvnRepoTest::stashOrdering() {
  QVERIFY(!commitA("base\n", QStringLiteral("Base")).isEmpty());
  for (int i = 1; i <= 3; ++i) {
    QVERIFY(writeFile(file("a.txt"), QByteArray::number(i) + '\n'));
    QVERIFY(SvnRepo::stash(_wc, QStringLiteral("s%1").arg(i)));
  }

  // Index 0 is the newest.
  QCOMPARE(SvnRepo::listStashes(_wc, _quit),
           (QStringList{QStringLiteral("On trunk: s3"),
                        QStringLiteral("On trunk: s2"),
                        QStringLiteral("On trunk: s1")}));
  QVERIFY(SvnRepo::applyStash(_wc, 2));
  QCOMPARE(readFile(file("a.txt")), QByteArray("1\n"));
}

void SvnRepoTest::applyStashCases_data() {
  QTest::addColumn<QString>("state");
  QTest::newRow("clean") << QStringLiteral("clean");
  QTest::newRow("conflicting edit") << QStringLiteral("conflicting edit");
  QTest::newRow("added file exists") << QStringLiteral("added file exists");
  QTest::newRow("other branch") << QStringLiteral("other branch");
}

void SvnRepoTest::applyStashCases() {
  QFETCH(QString, state);

  QVERIFY(!commitA("one\ntwo\n", QStringLiteral("Base")).isEmpty());
  QVERIFY(SvnRepo::createBranchFromCommit(_wc, QString(), QStringLiteral("b")));
  QVERIFY(writeFile(file("a.txt"), "one\nstashed\n"));
  QVERIFY(writeFile(file("new.txt"), "stashed new\n"));
  bool ok = false;
  svnIn(_wc, {QStringLiteral("add"), QStringLiteral("new.txt")}, &ok);
  QVERIFY(SvnRepo::stash(_wc, QStringLiteral("work")));

  if (state == QLatin1String("conflicting edit")) {
    QVERIFY(writeFile(file("a.txt"), "one\nmine\n"));
  } else if (state == QLatin1String("added file exists")) {
    QVERIFY(writeFile(file("new.txt"), "already here\n"));
  } else if (state == QLatin1String("other branch")) {
    QVERIFY(SvnRepo::checkoutBranch(_wc, QStringLiteral("b"),
                                    SvnRepo::BranchType::Local));
  }

  resetLastError();
  const bool applied = SvnRepo::applyStash(_wc, 0);
  if (state == QLatin1String("clean") ||
      state == QLatin1String("other branch")) {
    QVERIFY2(applied, qPrintable(lastErrorMessage()));
    QCOMPARE(readFile(file("a.txt")), QByteArray("one\nstashed\n"));
    QCOMPARE(readFile(file("new.txt")), QByteArray("stashed new\n"));
    return;
  }
  // A clash: conflicts with markers, or a refusal; the local file is never
  // overwritten without a trace.
  if (applied) {
    QVERIFY(SvnRepo::hasConflicts(_wc));
  } else {
    QVERIFY(!lastErrorMessage().isEmpty());
  }
  const QByteArray local = state == QLatin1String("conflicting edit")
                               ? readFile(file("a.txt"))
                               : readFile(file("new.txt"));
  QVERIFY2(local.contains(state == QLatin1String("conflicting edit")
                              ? QByteArray("mine")
                              : QByteArray("already here")),
           local.constData());
  // The stash is still there to try again.
  QCOMPARE(SvnRepo::listStashes(_wc, _quit).size(), 1);
}

void SvnRepoTest::stashIndexes_data() {
  QTest::addColumn<int>("index");
  QTest::newRow("negative") << -1;
  QTest::newRow("out of range") << 2;
  QTest::newRow("after deletes") << 1;
}

void SvnRepoTest::stashIndexes() {
  QFETCH(int, index);

  QVERIFY(!commitA("base\n", QStringLiteral("Base")).isEmpty());
  for (int i = 0; i < 2; ++i) {
    QVERIFY(writeFile(file("a.txt"), QByteArray::number(i) + '\n'));
    QVERIFY(SvnRepo::stash(_wc, QStringLiteral("s%1").arg(i)));
  }
  if (QByteArray(QTest::currentDataTag()) == "after deletes") {
    QVERIFY(SvnRepo::deleteStash(_wc, 0));
  }
  const QStringList before = SvnRepo::listStashes(_wc, _quit);

  for (int call = 0; call < 3; ++call) {
    resetLastError();
    bool done = false;
    if (call == 0) {
      done = SvnRepo::applyStash(_wc, index);
    } else if (call == 1) {
      done = SvnRepo::deleteStash(_wc, index);
    } else {
      done = !SvnRepo::getStashStatus(_wc, index).commit.isEmpty();
    }
    QVERIFY(!done);
    QCOMPARE(lastError(), SvnRepo::SvnError::InvalidArgument);
  }
  QCOMPARE(SvnRepo::listStashes(_wc, _quit), before);
  QVERIFY(svnStatusXml(_wc).isEmpty());
}

void SvnRepoTest::stashStatus() {
  QVERIFY(writeFile(file("gone.txt"), "g\n"));
  QVERIFY(!commitA("base\n", QStringLiteral("Base")).isEmpty());
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  QVERIFY(writeFile(file("a.txt"), "changed\n"));
  QVERIFY(writeFile(file("new.txt"), "new\n"));
  svnIn(_wc, {QStringLiteral("add"), QStringLiteral("new.txt")});
  svnIn(_wc, {QStringLiteral("delete"), QStringLiteral("gone.txt")});
  const qint64 before = QDateTime::currentMSecsSinceEpoch();
  QVERIFY(SvnRepo::stash(_wc, QStringLiteral("status")));

  const SvnRepo::CommitStatus st = SvnRepo::getStashStatus(_wc, 0);
  QCOMPARE(st.message, QStringLiteral("On trunk: status"));
  QCOMPARE(st.modifiedFiles, QStringList{QStringLiteral("a.txt")});
  QCOMPARE(st.newFiles, QStringList{QStringLiteral("new.txt")});
  QCOMPARE(st.deletedFiles, QStringList{QStringLiteral("gone.txt")});
  // Milliseconds since the epoch.
  QVERIFY2(qAbs(st.timeStamp - before) < 60 * 1000,
           qPrintable(QString::number(st.timeStamp)));
}

void SvnRepoTest::stashDiffEolNative() {
  QVERIFY(writeFile(file("a.txt"), "one\ntwo\nthree\n"));
  bool ok = false;
  svnIn(_wc, {QStringLiteral("add"), QStringLiteral("a.txt")}, &ok);
  svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("svn:eol-style"),
              QStringLiteral("native"), QStringLiteral("a.txt")},
        &ok);
  QVERIFY(svnCommitAll(_wc, QStringLiteral("Native")));
  QByteArray content = readFile(file("a.txt"));
  content.replace("two", "TWO");
  QVERIFY(writeFile(file("a.txt"), content));
  QVERIFY(SvnRepo::stash(_wc, QStringLiteral("eol")));

  // Only the changed line differs, whatever the native line ending is.
  const QString shelf = SvnRepo::getStashStatus(_wc, 0).commit;
  const SvnRepo::FilesDiff diff =
      SvnRepo::diffFilesFromCommit(_wc, {QStringLiteral("a.txt")}, shelf);
  QVERIFY2(diff.valid, qPrintable(lastErrorMessage()));
  QStringList lines;
  for (const SvnRepo::FilesDiffDelta &delta : diff.deltas) {
    lines += changedLines(delta);
  }
#ifdef Q_OS_WIN
#endif
  QCOMPARE(lines, (QStringList{QStringLiteral("-two"), QStringLiteral("+TWO")}));
}

void SvnRepoTest::stashCliInterop() {
  QVERIFY(!commitA("base\n", QStringLiteral("Base")).isEmpty());
  QVERIFY(writeFile(file("a.txt"), "cli\n"));
  bool ok = false;
  svnIn(_wc, {QStringLiteral("x-shelve"), QStringLiteral("cli-shelf")}, &ok);
  QVERIFY(ok);

  // A shelf made by the 1.14 command line client is listed.
  QEXPECT_FAIL("", "B21h: shelves of the 1.14 CLI (v3) aren't listed",
               Continue);
  QCOMPARE(SvnRepo::listStashes(_wc, _quit).size(), 1);
}

void SvnRepoTest::stashNote_data() {
  QTest::addColumn<QString>("note");
  QTest::addColumn<QString>("message");
  QTest::newRow("empty") << QString() << QStringLiteral("WIP on trunk");
  QTest::newRow("unicode") << QString::fromUtf16(u"nöte 日本")
                           << QString::fromUtf16(u"On trunk: nöte "
                                                 u"日本");
  QTest::newRow("hash first") << QStringLiteral("#42 wip")
                              << QStringLiteral("On trunk: #42 wip");
}

void SvnRepoTest::stashNote() {
  QFETCH(QString, note);
  QFETCH(QString, message);

  QVERIFY(!commitA("base\n", QStringLiteral("Base")).isEmpty());
  QVERIFY(writeFile(file("a.txt"), "changed\n"));
  QVERIFY2(SvnRepo::stash(_wc, note), qPrintable(lastErrorMessage()));
  QCOMPARE(SvnRepo::listStashes(_wc, _quit), QStringList{message});
}

// --- 3.13 Remotes and synchronisation ---------------------------------------

void SvnRepoTest::updateRemoteRelocates_data() {
  QTest::addColumn<QString>("spelling");
  QTest::addColumn<bool>("valid");
  QTest::newRow("moved repository") << QStringLiteral("moved") << true;
  QTest::newRow("localhost") << QStringLiteral("localhost") << true;
  QTest::newRow("drive letter case") << QStringLiteral("drive") << true;
  QTest::newRow("trailing slash") << QStringLiteral("trailing") << true;
  QTest::newRow("space in path") << QStringLiteral("space") << true;
  QTest::newRow("other repository") << QStringLiteral("other uuid") << false;
  QTest::newRow("trunk instead of root") << QStringLiteral("trunk") << false;
}

void SvnRepoTest::updateRemoteRelocates() {
  QFETCH(QString, spelling);
  QFETCH(bool, valid);

  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
  // The repository moves, as it does when a server is renamed.
  QString newDir = _base + QStringLiteral("/moved-repo");
  if (spelling == QLatin1String("space")) {
    newDir = _base + QStringLiteral("/moved repo");
  }
  QString url;
  if (spelling == QLatin1String("other uuid")) {
    url = makeRepo(_base + QStringLiteral("/other-repo"), Layout::Standard);
  } else {
    QVERIFY(QDir().rename(_repoDir, newDir));
    _repoDir = newDir;
    url = fileUrl(newDir);
  }
  if (spelling == QLatin1String("localhost")) {
    url.replace(QStringLiteral("file:///"), QStringLiteral("file://localhost/"));
  } else if (spelling == QLatin1String("drive")) {
    const int colon = url.indexOf(QLatin1Char(':'), 8);
    url[colon - 1] = url[colon - 1].toLower();
  } else if (spelling == QLatin1String("trailing")) {
    url += QLatin1Char('/');
  } else if (spelling == QLatin1String("trunk")) {
    url += QStringLiteral("/trunk");
  }

  resetLastError();
  const bool relocated = SvnRepo::updateRemote(_wc, QStringLiteral("origin"),
                                               QString(), url, url);
  const QStringList malfunctions = SvnRepo::Testing::takeMalfunctions();
  QVERIFY2(malfunctions.isEmpty(), qPrintable(malfunctions.join('\n')));
  if (!valid) {
    QVERIFY(!relocated);
    EXPECT_SVN_ERROR(SvnRepo::SvnError::RelocateFailed);
    QCOMPARE(svnInfo(_wc, QStringLiteral("repos-root-url")), _rootUrl);
    return;
  }
  QVERIFY2(relocated, qPrintable(lastErrorMessage()));
  QCOMPARE(svnInfo(_wc, QStringLiteral("repos-root-url")), fileUrl(newDir));
  QCOMPARE(SvnRepo::listRemotes(_wc).first().fetchUrl, fileUrl(newDir));
  // The relocated working copy works.
  QVERIFY(writeFile(file("a.txt"), "after the move\n"));
  QVERIFY2(commitAll(_wc, QStringLiteral("After")),
           qPrintable(lastErrorMessage()));
}

void SvnRepoTest::remoteUnsupportedOperations_data() {
  QTest::addColumn<QString>("operation");
  for (const char *operation :
       {"addRemote", "removeRemote", "rename remote", "separate push URL",
        "setDefaultRemote", "addTarget", "editTarget", "deleteTarget",
        "setDefaultTarget", "setUpstream"}) {
    QTest::newRow(operation) << QString::fromLatin1(operation);
  }
}

void SvnRepoTest::remoteUnsupportedOperations() {
  QFETCH(QString, operation);

  const QString other = QStringLiteral("file:///nowhere");
  resetLastError();
  bool done = true;
  if (operation == QLatin1String("addRemote")) {
    done = SvnRepo::addRemote(_wc, QStringLiteral("upstream"), other, other);
  } else if (operation == QLatin1String("removeRemote")) {
    done = SvnRepo::removeRemote(_wc, QStringLiteral("origin"));
  } else if (operation == QLatin1String("rename remote")) {
    done = SvnRepo::updateRemote(_wc, QStringLiteral("origin"),
                                 QStringLiteral("renamed"), _rootUrl,
                                 _rootUrl);
  } else if (operation == QLatin1String("separate push URL")) {
    done = SvnRepo::updateRemote(_wc, QStringLiteral("origin"), QString(),
                                 _rootUrl, other);
  } else if (operation == QLatin1String("setDefaultRemote")) {
    QVERIFY(SvnRepo::setDefaultRemote(_wc, QStringLiteral("origin")));
    done = SvnRepo::setDefaultRemote(_wc, QStringLiteral("upstream"));
  } else if (operation == QLatin1String("addTarget")) {
    done = SvnRepo::addTarget(_wc, QStringLiteral("origin"),
                              QStringLiteral("t"), other, other);
  } else if (operation == QLatin1String("editTarget")) {
    done = SvnRepo::editTarget(_wc, QStringLiteral("origin"),
                               QStringLiteral("t"), other, other);
  } else if (operation == QLatin1String("deleteTarget")) {
    done = SvnRepo::deleteTarget(_wc, QStringLiteral("origin"),
                                 QStringLiteral("t"));
  } else if (operation == QLatin1String("setDefaultTarget")) {
    done = SvnRepo::setDefaultTarget(_wc, QStringLiteral("origin"),
                                     QStringLiteral("t"));
  } else {
    done = SvnRepo::setUpstream(_wc, QStringLiteral("origin"),
                                QStringLiteral("trunk"),
                                QStringLiteral("trunk"));
  }
  QVERIFY(!done);
  EXPECT_SVN_ERROR(SvnRepo::SvnError::UnsupportedOperation);
  QCOMPARE(SvnRepo::listRemotes(_wc).size(), 1);
  QCOMPARE(svnInfo(_wc, QStringLiteral("repos-root-url")), _rootUrl);
}

void SvnRepoTest::getUpstreamUnknownBranch_data() {
  QTest::addColumn<QString>("branch");
  QTest::newRow("unknown") << QStringLiteral("no-such-branch");
  QTest::newRow("empty") << QString();
  QTest::newRow("tag name") << QStringLiteral("v1");
}

void SvnRepoTest::getUpstreamUnknownBranch() {
  QFETCH(QString, branch);

  QVERIFY(SvnRepo::createTagFromCommit(_wc, QString(), QStringLiteral("v1"),
                                       kAuthor, kEmail, QString()));
  // Only an existing branch has an upstream: itself.
  const auto [found, remote, upstream] = SvnRepo::getUpstream(_wc, branch);
  QVERIFY(!found);
  Q_UNUSED(remote);
  Q_UNUSED(upstream);
}

void SvnRepoTest::pullUpdatesAndReportsConflicts() {
  QVERIFY(writeFile(file("b.txt"), "b\n"));
  QVERIFY(!commitA("base\n", QStringLiteral("Base")).isEmpty());
  const QString other = _base + QStringLiteral("/other");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk"), other));
  QVERIFY(writeFile(other + QStringLiteral("/b.txt"), "theirs\n"));
  QVERIFY(svnCommitAll(other, QStringLiteral("Theirs")));

  // An incoming change merges into the working copy.
  QVERIFY2(SvnRepo::pull(_wc, QStringLiteral("trunk"), false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("b.txt")), QByteArray("theirs\n"));
  QCOMPARE(svnInfo(_wc, QStringLiteral("revision")), head());

  // A conflicting one is reported.
  QVERIFY(writeFile(other + QStringLiteral("/a.txt"), "theirs\n"));
  QVERIFY(svnCommitAll(other, QStringLiteral("Theirs again")));
  QVERIFY(writeFile(file("a.txt"), "mine\n"));
  resetLastError();
  const bool pulled = SvnRepo::pull(_wc, QStringLiteral("trunk"), false);
  QVERIFY(SvnRepo::hasConflicts(_wc));
  QVERIFY(!pulled);
}

void SvnRepoTest::pullMixedRevisionWc() {
  QVERIFY(writeFile(file("b.txt"), "b\n"));
  QVERIFY(!commitA("one\n", QStringLiteral("One")).isEmpty());
  QVERIFY(!commitA("two\n", QStringLiteral("Two")).isEmpty());
  // After its own commits the working copy is at several revisions.
  QVERIFY(svnInfo(_wc, QStringLiteral("revision")) != head());

  QVERIFY2(SvnRepo::pull(_wc, QStringLiteral("trunk"), false),
           qPrintable(lastErrorMessage()));
  const StatusMap all = svnStatusXml(_wc, true);
  for (auto it = all.cbegin(); it != all.cend(); ++it) {
    QCOMPARE(it->revision, head().toLongLong());
  }
}

void SvnRepoTest::pullRebaseFlag() {
  QVERIFY(!commitA("base\n", QStringLiteral("Base")).isEmpty());
  const QString other = _base + QStringLiteral("/other");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk"), other));
  QVERIFY(writeFile(other + QStringLiteral("/b.txt"), "theirs\n"));
  QVERIFY(svnCommitAll(other, QStringLiteral("Theirs")));
  QVERIFY(writeFile(file("a.txt"), "local\n"));

  // Documented: there are no local commits to rebase; rebase is an update.
  QVERIFY2(SvnRepo::pull(_wc, QStringLiteral("trunk"), true),
           qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("b.txt")), QByteArray("theirs\n"));
  QCOMPARE(readFile(file("a.txt")), QByteArray("local\n"));
}

void SvnRepoTest::pushWithStagedWork() {
  QVERIFY(!commitA("base\n", QStringLiteral("Base")).isEmpty());
  QVERIFY(writeFile(file("a.txt"), "staged\n"));
  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("a.txt")}));
  QVERIFY(writeFile(file("b.txt"), "unstaged\n"));
  const QString before = head();

  // Documented: commits are made in the repository, so push has nothing to
  // send; uncommitted work stays where it is.
  QVERIFY(SvnRepo::push(_wc, QStringLiteral("trunk"), false));
  QVERIFY(SvnRepo::push(_wc, QStringLiteral("trunk"), true));
  QCOMPARE(head(), before);
  QCOMPARE(changelistMembers(svnStatusXml(_wc)),
           QStringList{QStringLiteral("a.txt")});
}

void SvnRepoTest::fetchAllAndFetchRemote() {
  QVERIFY(!commitA("base\n", QStringLiteral("Base")).isEmpty());
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  const QString other = _base + QStringLiteral("/other");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk"), other));
  QVERIFY(writeFile(other + QStringLiteral("/a.txt"), "theirs\n"));
  QVERIFY(svnCommitAll(other, QStringLiteral("Theirs")));
  const QString revision = svnInfo(_wc, QStringLiteral("revision"));

  // Nothing to fetch; the working copy isn't touched.
  QVERIFY(SvnRepo::fetchAll(_wc));
  QVERIFY(SvnRepo::fetchRemote(_wc, QStringLiteral("origin")));
  QCOMPARE(svnInfo(_wc, QStringLiteral("revision")), revision);
  QCOMPARE(readFile(file("a.txt")), QByteArray("base\n"));
}

// --- 3.14 Externals (submodules) --------------------------------------------

void SvnRepoTest::addSubmoduleCases_data() {
  QTest::addColumn<QString>("url");
  QTest::addColumn<QString>("name");
  QTest::newRow("plain") << QStringLiteral("<lib>") << QStringLiteral("ext");
  QTest::newRow("nested target") << QStringLiteral("<lib>")
                                 << QStringLiteral("third/ext");
  QTest::newRow("target with space") << QStringLiteral("<lib>")
                                     << QStringLiteral("my ext");
  QTest::newRow("url with space") << QStringLiteral("<lib with space>")
                                  << QStringLiteral("ext");
  QTest::newRow("pinned revision") << QStringLiteral("-r1 <lib>")
                                   << QStringLiteral("ext");
  QTest::newRow("relative url") << QStringLiteral("^/branches")
                                << QStringLiteral("ext");
}

void SvnRepoTest::addSubmoduleCases() {
  QFETCH(QString, url);
  QFETCH(QString, name);

  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  const QString lib = makeRepo(_base + QStringLiteral("/lib"), Layout::Standard);
  const QString libWithSpace =
      makeRepo(_base + QStringLiteral("/lib repo"), Layout::Standard);
  QVERIFY(!lib.isEmpty() && !libWithSpace.isEmpty());
  url.replace(QStringLiteral("<lib>"), lib + QStringLiteral("/trunk"));
  url.replace(QStringLiteral("<lib with space>"),
              libWithSpace + QStringLiteral("/trunk"));

  resetLastError();
  const bool added =
      SvnRepo::addSubmodule(_wc, url, name, QString(), QString());
  QVERIFY2(added, qPrintable(lastErrorMessage()));
  QCOMPARE(SvnRepo::listSubmodules(_wc), QStringList{name});
  QVERIFY(QFileInfo(file(name) + QStringLiteral("/.svn")).isDir() ||
          QFileInfo(file(name)).isDir());
  const QVector<SvnRepo::SubmoduleInfo> submodules =
      SvnRepo::listSubmodulesWithStatus(_wc, _quit);
  QCOMPARE(submodules.size(), 1);
  if (url.startsWith(QLatin1String("-r1"))) {
    QCOMPARE(submodules.first().commit, QStringLiteral("1"));
  }
}

void SvnRepoTest::addSubmoduleDoesNotUpdateWholeWc() {
  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  const QString revision = svnInfo(_wc, QStringLiteral("revision"));
  const QString other = _base + QStringLiteral("/other");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk"), other));
  QVERIFY(writeFile(other + QStringLiteral("/a.txt"), "theirs\n"));
  QVERIFY(svnCommitAll(other, QStringLiteral("Theirs")));
  QVERIFY(writeFile(file("b.txt"), "local\n"));
  const QString lib = makeRepo(_base + QStringLiteral("/lib"), Layout::Standard);

  QVERIFY2(SvnRepo::addSubmodule(_wc, lib + QStringLiteral("/trunk"),
                                 QStringLiteral("ext"), QString(), QString()),
           qPrintable(lastErrorMessage()));
  // Only the external is fetched; the rest keeps its revision.
  QCOMPARE(readFile(file("b.txt")), QByteArray("local\n"));
  QCOMPARE(readFile(file("a.txt")), QByteArray("a\n"));
  QCOMPARE(svnStatusXml(_wc, true).value("a.txt").revision,
           revision.toLongLong());
}

void SvnRepoTest::addSubmoduleCommitted() {
  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
  const QString lib = makeRepo(_base + QStringLiteral("/lib"), Layout::Standard);
  QVERIFY(SvnRepo::addSubmodule(_wc, lib + QStringLiteral("/trunk"),
                                QStringLiteral("ext"), QString(), QString()));

  // The svn:externals change shows up and can be committed.
  QVERIFY(status().hasChanges());
  commitAll(_wc, QStringLiteral("Add external"));
  QVERIFY(changedPaths(_repoDir, head()).contains(QStringLiteral("_U trunk")));
}

void SvnRepoTest::deleteSubmoduleKeepsOthers() {
  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
  const QString lib = makeRepo(_base + QStringLiteral("/lib"), Layout::Standard);
  QVERIFY(SvnRepo::addSubmodule(_wc, lib + QStringLiteral("/trunk"),
                                QStringLiteral("ext"), QString(), QString()));
  QVERIFY(SvnRepo::addSubmodule(_wc, lib + QStringLiteral("/trunk"),
                                QStringLiteral("ext2"), QString(), QString()));
  QVERIFY(SvnRepo::addSubmodule(_wc, lib + QStringLiteral("/trunk"),
                                QStringLiteral("next"), QString(), QString()));

  QVERIFY2(SvnRepo::deleteSubmodule(_wc, QStringLiteral("ext"), QString()),
           qPrintable(lastErrorMessage()));
  QCOMPARE(SvnRepo::listSubmodules(_wc),
           (QStringList{QStringLiteral("ext2"), QStringLiteral("next")}));
  QVERIFY(!QFileInfo::exists(file("ext")));
  QVERIFY(QFileInfo(file("ext2")).isDir());
  QVERIFY(QFileInfo(file("next")).isDir());
}

void SvnRepoTest::deleteSubmodulePathContainment_data() {
  QTest::addColumn<QString>("submodulePath");
  QTest::newRow("parent") << QStringLiteral("../sentinel");
  QTest::newRow("elsewhere") << QStringLiteral("<elsewhere>");
  QTest::newRow("working copy root") << QStringLiteral(".");
}

void SvnRepoTest::deleteSubmodulePathContainment() {
  QFETCH(QString, submodulePath);

  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
  const QString lib = makeRepo(_base + QStringLiteral("/lib"), Layout::Standard);
  QVERIFY(SvnRepo::addSubmodule(_wc, lib + QStringLiteral("/trunk"),
                                QStringLiteral("ext"), QString(), QString()));
  QVERIFY(writeFile(_base + QStringLiteral("/sentinel/keep.txt"), "keep\n"));
  QVERIFY(writeFile(_base + QStringLiteral("/elsewhere/keep.txt"), "keep\n"));
  if (submodulePath == QLatin1String("<elsewhere>")) {
    submodulePath = _base + QStringLiteral("/elsewhere");
  }

  // Only the external's own folder may be removed.
  resetLastError();
  const bool deleted =
      SvnRepo::deleteSubmodule(_wc, QStringLiteral("ext"), submodulePath);
  QVERIFY(!deleted);
  QCOMPARE(readFile(_base + QStringLiteral("/sentinel/keep.txt")),
           QByteArray("keep\n"));
  QCOMPARE(readFile(_base + QStringLiteral("/elsewhere/keep.txt")),
           QByteArray("keep\n"));
  QCOMPARE(readFile(file("a.txt")), QByteArray("a\n"));
}

void SvnRepoTest::submoduleStatus() {
  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
  const QString lib = makeRepo(_base + QStringLiteral("/lib"), Layout::Standard);
  const QString libWc = _base + QStringLiteral("/libwc");
  QVERIFY(secondWc(lib + QStringLiteral("/trunk"), libWc));
  QVERIFY(writeFile(libWc + QStringLiteral("/l.txt"), "l\n"));
  QVERIFY(svnCommitAll(libWc, QStringLiteral("Lib")));
  QVERIFY(SvnRepo::addSubmodule(_wc, lib + QStringLiteral("/trunk"),
                                QStringLiteral("ext"), QString(), QString()));
  QCOMPARE(SvnRepo::listSubmodulesWithStatus(_wc, _quit).first().statusInfo.status,
           SvnRepo::SubmoduleStatusType::UpToDate);

  // Local edits in the external.
  QVERIFY(writeFile(file("ext/l.txt"), "edited\n"));
  QVERIFY(SvnRepo::listSubmodulesWithStatus(_wc, _quit).first().statusInfo.status !=
          SvnRepo::SubmoduleStatusType::UpToDate);
  bool ok = false;
  svnIn(file("ext"), {QStringLiteral("revert"), QStringLiteral("-R"),
                      QStringLiteral(".")},
        &ok);

  // A newer revision upstream.
  QVERIFY(writeFile(libWc + QStringLiteral("/l.txt"), "newer\n"));
  QVERIFY(svnCommitAll(libWc, QStringLiteral("Newer")));
  QCOMPARE(SvnRepo::listSubmodulesWithStatus(_wc, _quit).first().statusInfo.status,
           SvnRepo::SubmoduleStatusType::Outdated);
  QVERIFY(SvnRepo::updateSubmodule(_wc, QStringLiteral("ext")));
  QCOMPARE(readFile(file("ext/l.txt")), QByteArray("newer\n"));
}

void SvnRepoTest::fileExternals() {
  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  svnIn(_wc, {QStringLiteral("propset"), QStringLiteral("svn:externals"),
              QStringLiteral("^/trunk/a.txt copy-of-a.txt"),
              QStringLiteral(".")},
        &ok);
  QVERIFY(ok);
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  QCOMPARE(readFile(file("copy-of-a.txt")), QByteArray("a\n"));

  // A file external is listed like a folder external.
  QCOMPARE(SvnRepo::listSubmodules(_wc),
           QStringList{QStringLiteral("copy-of-a.txt")});
  QCOMPARE(SvnRepo::listSubmodulesWithStatus(_wc, _quit).size(), 1);
  QVERIFY(!status().newFiles.contains(QStringLiteral("copy-of-a.txt")));
}

// --- 3.15 Git-only operations -----------------------------------------------

void SvnRepoTest::gitOnlyOperationsTable_data() {
  QTest::addColumn<QString>("operation");
  for (const char *operation :
       {"squashBranches", "abortRebase", "commitRebase", "removeWorktree",
        "lockWorktree", "unlockWorktree", "startGitLabFlowFeature",
        "finishGitLabFlowFeature", "promoteGitLabFlowEnvironment",
        "startGitLabFlowRelease", "cherryPickToGitLabFlowRelease",
        "updateGitHubFlowBranch", "finishGitHubFlowBranch", "initGitFlowNext",
        "startGitFlowNextBranch", "finishGitFlowNextBranch",
        "updateGitFlowNextBranch", "setWorkflowType",
        "applyGitFlowNextWorkflow"}) {
    QTest::newRow(operation) << QString::fromLatin1(operation);
  }
}

void SvnRepoTest::gitOnlyOperationsTable() {
  QFETCH(QString, operation);

  // The ones not in gitOnlyOperationsAreUnsupported; each fails with
  // UnsupportedOperation and a message, and null out-parameters are safe.
  QVERIFY(!commitA("a\n", QStringLiteral("Base")).isEmpty());
  const QString before = head();
  const QString b = QStringLiteral("b");
  const QString t = QStringLiteral("trunk");
  resetLastError();
  bool done = true;
  if (operation == QLatin1String("squashBranches")) {
    done = SvnRepo::squashBranches(_wc, b, t, kAuthor, kEmail, false);
  } else if (operation == QLatin1String("abortRebase")) {
    done = SvnRepo::abortRebase(_wc);
  } else if (operation == QLatin1String("commitRebase")) {
    done = SvnRepo::commitRebase(_wc, QStringLiteral("s"), QString(), kAuthor,
                                 kEmail, true, false, false);
  } else if (operation == QLatin1String("removeWorktree")) {
    done = SvnRepo::removeWorktree(_wc, QStringLiteral("w"), true);
  } else if (operation == QLatin1String("lockWorktree")) {
    done = SvnRepo::lockWorktree(_wc, QStringLiteral("w"), QStringLiteral("r"));
  } else if (operation == QLatin1String("unlockWorktree")) {
    done = SvnRepo::unlockWorktree(_wc, QStringLiteral("w"));
  } else if (operation == QLatin1String("startGitLabFlowFeature")) {
    done = SvnRepo::startGitLabFlowFeature(_wc, b, t);
  } else if (operation == QLatin1String("finishGitLabFlowFeature")) {
    done = SvnRepo::finishGitLabFlowFeature(_wc, b, t, kAuthor, kEmail);
  } else if (operation == QLatin1String("promoteGitLabFlowEnvironment")) {
    done = SvnRepo::promoteGitLabFlowEnvironment(
        _wc, {QStringLiteral("staging")}, b, kAuthor, kEmail);
  } else if (operation == QLatin1String("startGitLabFlowRelease")) {
    done = SvnRepo::startGitLabFlowRelease(_wc, b, t);
  } else if (operation == QLatin1String("cherryPickToGitLabFlowRelease")) {
    done = SvnRepo::cherryPickToGitLabFlowRelease(_wc, before, b, kAuthor,
                                                  kEmail, nullptr);
    bool conflicted = true;
    SvnRepo::cherryPickToGitLabFlowRelease(_wc, before, b, kAuthor, kEmail,
                                           &conflicted);
    QVERIFY(!conflicted);
  } else if (operation == QLatin1String("updateGitHubFlowBranch")) {
    done = SvnRepo::updateGitHubFlowBranch(_wc, b, t, kAuthor, kEmail);
  } else if (operation == QLatin1String("finishGitHubFlowBranch")) {
    done = SvnRepo::finishGitHubFlowBranch(_wc, b, t, kAuthor, kEmail);
  } else if (operation == QLatin1String("initGitFlowNext")) {
    done = SvnRepo::initGitFlowNext(_wc, t, QStringLiteral("develop"));
  } else if (operation == QLatin1String("startGitFlowNextBranch")) {
    done = SvnRepo::startGitFlowNextBranch(_wc, QStringLiteral("feature"), b);
  } else if (operation == QLatin1String("finishGitFlowNextBranch")) {
    done = SvnRepo::finishGitFlowNextBranch(_wc, b, kAuthor, kEmail);
  } else if (operation == QLatin1String("updateGitFlowNextBranch")) {
    done = SvnRepo::updateGitFlowNextBranch(_wc, b, kAuthor, kEmail);
  } else if (operation == QLatin1String("setWorkflowType")) {
    done = SvnRepo::setWorkflowType(_wc, GitRepo::WorkflowType::GitHubFlow);
  } else {
    done = SvnRepo::applyGitFlowNextWorkflow(_wc,
                                             GitRepo::GitFlowNextWorkflow());
  }
  QVERIFY(!done);
  EXPECT_SVN_ERROR(SvnRepo::SvnError::UnsupportedOperation);
  QCOMPARE(head(), before);
  QVERIFY(svnStatusXml(_wc).isEmpty());
}

void SvnRepoTest::workflowTypeIsConsistent() {
  // Documented: WorkflowType has no "none", so the default, GitFlow, is
  // reported; every flow action is unsupported and the Subversion bridge
  // doesn't offer them.
  QCOMPARE(SvnRepo::getWorkflowType(_wc), GitRepo::WorkflowType::GitFlow);
  QString id = QStringLiteral("x");
  int revision = 7;
  QVERIFY(!SvnRepo::getGitFlowNextWorkflowReference(_wc, &id, &revision));
  QVERIFY(id.isEmpty());
  QCOMPARE(revision, 0);
  QVERIFY(!SvnRepo::getGitFlowNextWorkflowReference(_wc, nullptr, nullptr));
  QCOMPARE(SvnRepo::getLfsStorageStatus(_wc).repositoryPath, _wc);
}

void SvnRepoTest::readGitFlowNextConfigSetsError() {
  GitRepo::GitFlowNextConfig config;
  resetLastError();
  QVERIFY(!SvnRepo::readGitFlowNextConfig(_wc, &config));
  QVERIFY(!SvnRepo::readGitFlowNextConfig(_wc, nullptr));
  QCOMPARE(lastError(), SvnRepo::SvnError::UnsupportedOperation);
}

QTEST_GUILESS_MAIN(SvnRepoTest)

#include "tst_svnrepo.moc"
