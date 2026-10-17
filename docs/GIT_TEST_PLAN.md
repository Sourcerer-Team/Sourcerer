# Git Test Plan

This plan gives Git (`lib/GitRepo` and `component/GitRepoManager`) the same test coverage that the `hg` branch has for Mercurial (`tests/hgrepo/tst_hgrepo.cpp` and `tst_hgrepomanager.cpp`). It also goes further on edge cases.

## 1. Current state

| Area | Covered today |
|---|---|
| `tests/gitrepo/tst_gitrepo.cpp` | 3 scenarios: commit heads and branches, default remote, `commitRebase` after a conflict |
| `tests/workflows` | Header-only helpers (WorkflowUtils, GitFlowNextWorkflow, Graphite). No repository I/O |
| `tests/commit_graph_builder` | Graph lanes. No libgit2 |
| GitRepoManager / bridge | **Nothing** |

Everything else in `GitRepo.h` is untested: status, staging, commit, diff, blame, history, branches, tags, merge, squash, cherry-pick, revert, stash, worktrees, bisect, submodules, remotes, fetch, pull, push, LFS, signing, credentials and workflows.

### Progress
- **Done:** §2 infrastructure for `gitrepo_test`: `GitTestHelpers.h`, isolated git config, `ConfigOverride`, and the `GitRepo::Testing::setCommercialPlanOverride` switch.
- **Done:** §3.1–3.7 in `tests/gitrepo/tst_gitrepo.cpp`. The suite has 136 passing tests (data rows counted), 1 skip (symlinks on Windows) and 25 `QEXPECT_FAIL` checks, one per confirmed defect (see §6.1).
- **Done:** the §6.1 (§3.1–3.7) defects are fixed in `lib/GitRepo`. Two `QEXPECT_FAIL` checks are left, both libgit2 MAX_PATH limits.
- **Done:** §3.8–3.14 (branches, local tags, merge/squash/rebase/cherry-pick/revert, stash, worktrees, bisect, remotes/targets/upstream). The suite now has 315 passing tests (data rows counted) and 1 skip.
- **Done:** the §6.1 (§3.8–3.14) defects are fixed in `lib/GitRepo`, and so are the problems found only in the source. Their 31 `QEXPECT_FAIL` checks are gone; the tests now check the fixed behaviour. The only `QEXPECT_FAIL` checks left are the two libgit2 MAX_PATH limits.
- **Done:** §3.15–3.19 (including `tagsOnRemote` from §3.9) in two new executables, so the network waits (1.5 s per transfer) don't slow down `gitrepo_test`:
  - `gitsync_test` (`tst_gitsync.cpp`): clone, fetch, pull, push, remote tags and branches, submodules and LFS. It has 80 passing tests (data rows counted) and takes about 4.5 minutes. LFS runs against `LfsStubServer.h`, an in-memory batch/download/upload server on 127.0.0.1.
  - `gitflows_test` (`tst_gitflows.cpp`): SSH and OpenPGP commit signing with real throwaway keys (each part is skipped when ssh-keygen or gpg is missing), Git Flow, git-flow-next, GitHub Flow and GitLab Flow. It has 29 passing tests.
  - The confirmed defects are `QEXPECT_FAIL` checks, listed in §6.1 under "§3.15–3.19" (fixed since, see the next item).
- **Done:** the §3.15–3.19 defects are fixed in `lib/GitRepo` (see "Fixes" under §6.1 "§3.15–3.19").
- **Done:** §4, §5 and HTTP authentication in two more executables:
  - `gitrepomanager_test` (`tst_gitrepomanager.cpp`): the GitRepoManager component through events, as RepoContainer uses it, including a round trip of every request type and HTTP Basic authentication against `GitHttpServer.h` (`git http-backend` behind an authenticating proxy). It has 101 passing tests (data rows counted) and takes about 1.5 minutes.
  - `gitcredentials_test` (`tst_gitcredentials.cpp`): GitCredentialStorage, the Basic authentication header, GitKeyStorage and the SSH key lookup with its passphrase prompt. It has 24 passing tests.
  - `FakeCredentialsOverlay.h` answers credential and passphrase prompts for both. `GitRepo::Testing::setCredentialsMasterKey` gives GitCredentialStorage a fixed key, so the tests don't touch the OS keychain (all suites that use credentials call `useTestCredentialsKey()`).
  - The confirmed defects are listed in §6.1 under "§4–5". All are fixed; no `QEXPECT_FAIL` checks are left in these suites.
- **Done:** `tests/workflows` and `tests/commit_graph_builder` run under `ctest` with the Git suites (`SOURCERER_BUILD_GITREPO_TESTS`). All §6 items are closed.
- **Done:** §6 item 5, the certificate policy, in `gitcertificates_test` (`tst_gitcertificates.cpp`, 23 tests). It calls the check directly with certificates it makes with OpenSSL, and clones over HTTPS from `GitHttpServer.h`, which can now serve TLS (Qt's TLS backend) and anonymous requests. All 8 ctest entries pass.
- **Done:** the rest of §4 and §5 that can run here:
  - `gitcredentials_test` (41 tests with data rows): `generateKeyPair` (RSA-4096, read back with `ssh-keygen`, with and without a passphrase, the passphrase prompt for the generated key, refused key names) and the default key lookup in `~/.ssh` (`defaultKeyFallback`). `GitRepo::Testing::setHomeDirectory` points `~/.ssh` at the test directory, since `QStandardPaths` test mode doesn't redirect the home.
  - `gitrepomanager_test` (104 tests): `credentialPromptsFinishOncePerUrl`, `closeDuringClone` (no cancellation; the close is answered after the clone and the container is free again; `GitHttpServer::setDelay` slows the clone down) and `stopWithOpenContexts` (stop with two open repositories and requests in flight).
  - The defects found are listed in §6.1 under "§4–5, second pass". All are fixed.
- **Not covered:** `bridgeSmoke` (GitRepoManagerBridgeImpl works on RepoContainer's widgets and `ui_RepoContainer.h`, so it needs most of GuiManager and a GUI test application), and SSH transport end to end (the passphrase prompt during a real connection, host keys from a server): there is no SSH server on Windows by default (Git for Windows ships no `sshd`, the OpenSSH server is an optional Windows feature). Both are covered below the transport: the passphrase prompt through `getSshAuthInfo`, host keys through the certificate check.

## 2. Infrastructure (do first)

### 2.1 Files and targets
```
tests/gitrepo/
  CMakeLists.txt          gitrepo_test + gitrepomanager_test (+ gitcredentials_test)
  GitTestHelpers.h        shared helpers (below)
  GitHttpServer.h         `git http-backend` behind a Basic-auth proxy (like HgAuthServer.h)
  LfsStubServer.h         minimal LFS batch API (QTcpServer, in-memory objects)
  tst_gitrepo.cpp         synchronous GitRepo:: API
  tst_gitrepomanager.cpp  event-driven GitRepoManager
  tst_gitcredentials.cpp  GitCredentialStorage / GitKeyStorage (QStandardPaths test mode)
  tst_gitcertificates.cpp server certificate policy, HTTPS clones from GitHttpServer
```
- Use the same CMake pattern as hgrepo: `RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}` and `add_test(NAME gitrepo ...)` / `gitrepomanager` / `gitcredentials`, all gated by `SOURCERER_BUILD_GITREPO_TESTS`.
- `gitrepomanager_test` also gets `add_dependencies(... SourcererAskPass)`.
- `tests/workflows` and `tests/commit_graph_builder` are added from `src/CMakeLists.txt` under the same option, so `ctest` runs them too (`workflows`, `commit_graph_builder`). They stay buildable as standalone projects.

### 2.2 Helpers (GitTestHelpers.h)
These mirror the hg helpers.
- `git(args, bool *ok)` / `gitIn(dir, args, ok)`: QProcess with `-c user.name=Tester -c user.email=tester@example.com -c core.autocrlf=false -c init.defaultBranch=main` and a 60 s timeout. Env: `GIT_CONFIG_NOSYSTEM=1`, `HOME`/`XDG_CONFIG_HOME` pointing into the temp dir (isolation from the user's global config), `GIT_TERMINAL_PROMPT=0`, `LC_ALL=C`.
- `writeFile`, `readFile`, `commitAll(summary)` (via the GitRepo API), `gitCommitAll` (via the CLI, for setup only), `tip()`, `revParse(rev)`, `porcelainStatus()`.
- `RepoPtr` (unique_ptr + `closeRepository`), `lastErrorMessage()`, `lastError()`.
- `makeBareRemote(QString *url)` creates `git init --bare` and returns both a plain path and a `file://` URL. `cloneRoot(QString *path)`.
- `changedLines(FilesDiffDelta)`, copied from the hg suite.
- `makeWritable(dir)` runs before `QTemporaryDir` cleanup. Git pack files are read-only on Windows.
- `ConfigOverride` RAII sets a ConfigManager key and restores it with `qScopeGuard`. Used for page size, max diff size, signing keys and git-flow branch names.
- `QStandardPaths::setTestModeEnabled(true)` in every `initTestCase`, so the credential and key stores never touch real user files.

### 2.3 Fixtures
- `initTestCase`: `QSKIP("git was not found")` if git is missing, then `GitRepo::initForCurrentThread()`.
- `init`: fresh `QTemporaryDir`, then `GitRepo::initRepository("repo", tmp, "main", kAuthor, kEmail)`, then open. This exercises the library's init instead of `git init`.
- `cleanup`: close the repo, `makeWritable`, delete the temp dir, reset `_quit`, and clear `GitCredentialStorage` memory credentials.

### 2.4 The commercial-plan gate
`Private::isCommercialPlan()` reads `Util::CloudCredentialtorage`. Tests need a seam:
- **Option A (recommended):** a test-only hook `GitRepo::Private::setCommercialPlanOverride(std::optional<bool>)`, compiled in when `SOURCERER_BUILD_GITREPO_TESTS` is on.
- **Option B:** seed `CloudCredentialtorage` with a fake, unexpired JWT in test mode.

Without one of these, HTTP authentication and SSH key paths cannot be reached. Only the "requires commercial plan" error paths can be tested.

## 3. tst_gitrepo.cpp: synchronous API

Each bullet is one test slot: a scenario with many asserts, including its own error path, in the hg style. Use `_data` rows where the same check runs over many inputs (marked **[data]**). Error asserts compare `getLastError().error` against the exact `GitError` value.

### 3.1 Repository lifecycle
- `initCreatesInitialCommitOnDefaultBranch`: `.git` exists; HEAD is `refs/heads/main` with one commit by kAuthor; status is clean. **[data]** default branch `main`, `trunk`, `feature/x` (slash), unicode `ветка`.
- `initRefusesExistingRepository`: `NO_REINIT`; the second init fails and the existing repo is untouched.
- `initIntoPathWithSpacesAndUnicode` **[data]**: `"my repo"`, `"répo ünï"`, a deep path close to MAX_PATH (Windows).
- `openRejectsNonRepository`: an empty dir or a missing path returns nullptr with `OpenRepositoryFailedToOpenRepository`.
- `openFromSubdirectory`: document the actual behaviour (libgit2 `git_repository_open` doesn't search upward). A trailing-slash path and backslash separators open the same repo.
- `openUnbornHeadCreatesInitialCommit`: `git init` by the CLI followed by `openRepository` makes one empty commit. Opening a second time does not add another.
- `openBareRepository` and `openLinkedWorktree`: open a bare repo and a `git worktree add` path; document what works.
- `openRepoWithGitFile`: `.git` is a gitdir file (as in a submodule or `--separate-git-dir`).
- `closeNullIsSafe`: `closeRepository(nullptr)`.
- `lastErrorIsPerThread`: an error on thread A is not seen from thread B. `initForCurrentThread` and `shutdownForCurrentThread` run on a worker thread.

### 3.2 Status
- `statusCategories`: new, modified, deleted, typechange (file to symlink, skipped on Windows without symlink rights), renamed, ignored (`.gitignore`), conflicted, each both staged and unstaged.
- `statusOfNestedUntrackedDirectory`: an untracked dir with files; whether it is reported as a dir or as files.
- `statusIgnoresNegatedPatterns`: `*.log` together with `!keep.log`.
- `statusCrlfAndAutocrlf` **[data]**: `core.autocrlf` true, false and input with CRLF and LF content; a renormalisation-only change; `.gitattributes text=auto`.
- `statusUnicodeAndSpacesInNames` **[data]**: `"a b.txt"`, `"ünï.txt"`, `"日本.txt"`, a leading dash `-x.txt`, `#hash`.
- `statusCaseOnlyRename` (Windows, `core.ignorecase`): `File.txt` to `file.txt`.
- `statusCancelled`: `_quit=true` returns empty results and does not crash.
- `statusLargeTree`: 5,000 files, status within a time budget (a smoke perf guard, not a benchmark).
- `statusExecutableBit` (non-Windows): a mode change counts as modified.
- `statusSubmoduleDirty`: a submodule with a new commit or a dirty workdir is reported.

### 3.3 Staging
- `stageUnstageRoundTrip`: new, then staged, then unstaged, then staged via an absolute path, via a relative path, and via backslashes.
- `stageDirectoryRecursively` / `unstageSubdirectory`.
- `stageDeletedFile`: delete from disk, stage, then it shows as deletedStaged.
- `stagePartiallyStagedFile`: the file has both staged and unstaged changes. Stage again and confirm both are reported correctly.
- `stageIgnoredFileFails` or `Forces`: document which.
- `stageNonexistentPathFails`: assert the error code.
- `stageEmptyListIsNoop`.
- `unstageInInitialCommitRepo`: HEAD has only the init commit.
- `unstageNewFileMakesUntracked`.
- `stageLfsTrackedFileStoresPointer`: `.gitattributes filter=lfs`; the index blob is a pointer and `.git/lfs/objects` holds the content.
- `stageSubmoduleGitlink`.
- `stageFileWithConflictMarkersResolvesConflict`.

### 3.4 Commit, amend, sign-off
- `commitStagedOnly`: only staged files are committed; the others stay modified.
- `commitWithoutStagedFilesFails`: assert the error, or document an allowed empty commit.
- `commitMessageComposition` **[data]**: summary only; summary + body; multi-line body; trailing whitespace; unicode; `#` lines; a very long summary (>72); a summary that is empty or whitespace only (fails?); CRLF in the body.
- `commitSignoffAppendsTrailer`: exactly one `Signed-off-by`, and no duplicate if the body already has it.
- `commitAuthorFallback`: an empty author or email falls back to repo config, then to the ctor values.
- `commitMergeHasTwoParents`: MERGE_HEAD present, the commit has 2 parents, state cleaned up.
- `commitCleanupFlag`: `cleanup=true` clears MERGE_MSG and similar files.
- `amendMessageOnly`: nothing staged; the message changes; the tree and parent are unchanged; there is still one commit.
- `amendWithStagedFiles`: adds the files and keeps the author date (assert the behaviour).
- `amendInitialCommit` and `amendMergeCommit`: keep both parents.
- `amendOnDetachedHead`.

### 3.5 Discard, reset, checkout, patch
- `discardModifiedRestoresHead`, `discardDeletedRestoresFile`, `discardStagedNewFileKeepsFileUntracked`, `discardStagedAndUnstagedMix`, `discardRenamedFile`, `discardInSubdirectoryWithSpaces`.
- `discardDoesNotTouchOtherFiles`.
- `resetSoftMixedHard` **[data]**: index and workdir state after each type. Hard reset removes tracked changes and keeps untracked files.
- `resetToUnknownCommitFails`.
- `resetDuringMergeClearsMergeState`.
- `checkoutCommitDetaches`: `getActiveBranch` reports a detached state. `checkoutCommit` with a dirty conflicting workdir fails (SAFE) and leaves files intact.
- `applyPatchModifyAddDelete`: a patch that modifies, creates (via PatchFileUtils) and deletes files.
- `applyPatchRoundTrip`: `createPatchFromCommit` then `resetToCommit Hard HEAD~1` then `applyPatch` gives an identical tree.
- `applyPatchFailsOnContextMismatch`: the workdir is unchanged afterwards.
- `applyPatchBinaryAndRename` / `applyPatchCrlf` / `applyPatchNoNewlineAtEof`.
- `applyPatchGarbageFails`: random text, an empty string, a truncated hunk.

### 3.6 Diff, blame, file size
- `diffWorkdirModifiedAndNew`: header, hunks, `repoContent` and `filesContent`.
- `diffDeletedFile`, `diffRenamedFile`, `diffBinaryFile` (no lines, binary flag), `diffEmptyFile`, `diffNoNewlineAtEof`, `diffCrlfOnlyChange`, `diffUnicodeContent`, `diffWhitespaceOnly`.
- `diffRespectsMaxFileSize`: set `GitRepoMaxDiffFileSize` small; a large file is reported as too big or truncated.
- `diffLfsFileShowsContentNotPointer` (or document that it shows the pointer).
- `diffFromCommitRootCommit` (no parent), `diffFromCommitMergeCommit` (against the second parent), `diffFromCommitUnknownShaInvalid`.
- `diffMultipleFilesSubset`: only the requested files.
- `blameCommittedAndUncommittedLines`: kNullCommit for uncommitted lines; an untracked file is all null; a missing file gives a per-file error.
- `blameAfterRenameFollows` (document), `blameMultipleAuthors`, `blameEmptyFile`, `blameBinaryFile`.
- `fileSizeAtCommitAndHead`: an empty commit means HEAD; a missing file returns -1; an LFS file returns the pointer size (document).

### 3.7 History
- `historyPaging`: page size 3 (ConfigOverride), 7 commits, total, hasMore, the last page, page beyond the end.
- `historyAllBranches`: `"*"` across branches is newest-first, merges included, with no duplicates.
- `historyRefTipsAndLabels`: local branches, remote branches, lightweight and annotated tags, HEAD; several refs on one commit.
- `historyBranchTypes` **[data]**: Local, Remote, Tag, and an unknown name (invalid, with error).
- `historyCursorInvalidatedAfterNewCommit`: page 0, then a commit, then page 0 again includes the new commit. Page 1 is stable after a reset or rebase.
- `historyCancelled`.
- `historyMergeParentsAndOctopus`.
- `historySignedCommitsReportSignature`: create one with `git commit -S` via an SSH key in a test-mode signing config. Skip if ssh-keygen is missing.
- `historyBisectMarksOnPageZero`.
- `historyEmptyRepoAfterOpen`.
- `commitStatusFields`: summary, author, parents, date, new/modified/deleted/renamed files. An unknown sha gives an empty status.
- `commitStatusMergeCommit` and `commitStatusRootCommit`.
- `commitBranchesTagsHeads`: `getCommitBranches`, `getCommitTags` (annotated and lightweight), `getCommitHeads`, `getCommitIsHead`. A deleted branch disappears from the results.

### 3.8 Branches
- `createBranchFromCommit`: it is not checked out. A duplicate name fails.
- `createBranchInvalidNames` **[data]**: `"a..b"`, `"a b"`, `"-x"`, `"x.lock"`, `"@"`, `"a/"`, `"HEAD"`, `""`, `"~^:"`. All fail with no ref created.
- `branchNamesWithSlashesAndUnicode` **[data]**: `feature/a/b`, `ünï`.
- `checkoutLocalBranch`, `checkoutRemoteBranchCreatesTracking` (upstream set), `checkoutRemoteBranchWhenLocalExists`, `checkoutTagDetaches`, `checkoutMissingBranchFails`.
- `checkoutWithDirtyConflictingWorkdirFails`: files are intact.
- `checkoutWithDirtyNonConflictingWorkdirCarriesChanges`.
- `renameBranch`: the active branch keeps HEAD attached; renaming onto an existing name fails; renaming a missing branch fails; upstream config moves with it.
- `deleteBranch`: deleting the active branch fails, an unmerged branch (document force semantics), a missing branch fails.
- `activeBranchQueries` **[data]**: Local, Remote (upstream), detached HEAD.
- `listLocalAndRemoteBranches`: sorted; `origin/HEAD` handling.
- `remoteBranchStatus` **[data]**: UpToDate, Ahead n, Behind n, Diverged (n, m), NoUpstream, and an upstream whose remote branch was deleted.

### 3.9 Tags
- `createTagIsAnnotated`: tagger, message, target.
- `createTagDuplicateFails`.
- `createTagInvalidNames` **[data]**.
- `createTagOnNonHeadCommit`.
- `renameTagKeepsKind` **[data]**: annotated, lightweight (CLI-created).
- `deleteTag`, `deleteMissingTagFails`.
- `listTagsSorted`.
- `checkoutTag`.
- `tagsOnRemote`: `pushTag` (lightweight and annotated), `pushAllTags`, `deleteRemoteTag` keeps the others, pushing a tag that already exists with a different target is rejected. *(Moved to §3.15: it needs a file remote.)*

### 3.10 Merge, squash, rebase, cherry-pick, revert
- `mergeFastForward`: no merge commit.
- `mergeCreatesMergeCommit`: message text, 2 parents.
- `mergeAlreadyUpToDateIsNoop`.
- `mergeUnrelatedHistories`: document whether it is allowed.
- `mergeConflictLeavesState`: `hasConflicts`, `getIsMergeInProgress`, `listConflictsForAFile` markers. Then `abortMerge` restores the files and HEAD.
- `mergeConflictResolveAndCommit`: stage the resolution, commit, 2 parents, state clean.
- `mergeConflictKinds` **[data]**: both modified, modify/delete, delete/modify, add/add, rename/rename, binary conflict.
- `mergeWithDirtyWorkdirFails`: the dirty file is untouched.
- `mergeIntoNonCurrentDestinationChecksItOut`.
- `squashBranches`: one commit, the body lists the squashed commits, the source is kept.
- `squashInvalidArguments` **[data]**: an empty source, an empty destination, source == destination give `SquashBranchesInvalidArguments`.
- `squashBusyRepository`: a merge is in progress, giving `SquashBranchesRepositoryBusy`.
- `squashConflictsKeptInWorkdir`: `SquashBranchesFailedWithConflicts`.
- `squashNothingToSquash`: the source is already merged.
- `rebaseLinear`: moves feature onto main; commit count; authors kept; committer updated.
- `rebaseAlreadyUpToDate`.
- `rebaseConflictAbortRestores`: the original branch tip and files.
- `rebaseConflictContinueMultipleSteps`: 3 commits with conflicts at steps 1 and 3; `commitRebase` before resolving fails; after resolving it continues and finishes.
- `rebaseEmptyCommitAfterResolve`: the resolution equals the base; document skip vs empty commit.
- `rebaseWithMergeCommitsInRange`: document whether they are flattened.
- `rebaseSigned`: with `signCommit=true` and signing configured.
- `rebaseInProgressBlocksOtherOps`: merge, squash and checkout fail during a rebase.
- `cherryPick`: a different author keeps the original author (document); the message is kept.
- `cherryPickConflict`: state, resolve, commit.
- `cherryPickMergeCommitFails` (needs a mainline).
- `cherryPickAlreadyApplied` gives an empty result (document).
- `revertCommit`: custom message, inverse diff.
- `revertConflict`.
- `revertMergeCommit` (document the mainline).
- `revertRootCommit`.

### 3.11 Stash
- `stashAndList`: order (newest = 0), note text, default name.
- `stashIncludesUntracked` (the third parent in `getStashStatus`).
- `stashCleanWorkdirFails`.
- `applyStashKeepsEntry`.
- `applyStashConflict` / `listConflictsForStashFile`.
- `applyStashWithDirtyWorkdir`.
- `applyStashInvalidIndexFails` **[data]**: -1, count, a big number.
- `deleteStashByIndex` reindexes the rest.
- `deleteAllStashes`.
- `deleteAllWhenEmpty`.
- `stashStatusFields`.
- `stashWithStagedAndUnstagedSameFile`: both versions come back.
- `stashBinaryAndLfsFiles`.

### 3.12 Worktrees
- `addWorktreeNewAndExistingBranch`.
- `addWorktreeBranchRequired`: an empty branch gives `AddWorktreeBranchRequired`.
- `addWorktreeBranchAlreadyCheckedOutFails`.
- `addWorktreeDuplicateNameOrPathFails`.
- `addWorktreeFromBaseBranch`.
- `addWorktreePathWithSpacesUnicode`.
- `listWorktreesFields`: main first, sorted, isMain, isCurrent, head, valid, locked. It is identical when opened from the linked worktree.
- `listWorktreesPrunableMissingDir`: the directory was deleted manually, so valid is false.
- `lockUnlockIdempotent`, `lockWithReason`, `lockMainFails`.
- `removeLockedFails`, `removeDirtyFailsUnlessForce`, `removeMainFails`, `removeMissingNameFails`.
- `commitInWorktreeVisibleInMain`.

### 3.13 Bisect
- `bisectFindsFirstBadCommit`: a loop over 10 commits; steps and remaining count down; firstBad fields.
- `bisectWaitingStates` **[data]**: good only gives waitingForBad; bad only gives waitingForGood.
- `bisectSkipAmbiguous`: skipping around the boundary gives `ambiguous`.
- `bisectInvalidRange`: good is a descendant of bad.
- `bisectResetRestoresHead`; a second reset gives `NotInProgress`.
- `bisectBusyRepository`: merge in progress.
- `bisectInvalidArguments`: an empty or unknown commit.
- `bisectCliCompatibility`: start by our API, continue with `git bisect good/bad` and vice versa; `refs/bisect/*`, `BISECT_LOG`.
- `bisectMarksInHistory`.

### 3.14 Remotes, targets, upstream (local only, no network)
- `addRemoteWithPushUrl`.
- `addRemoteInvalidAndDuplicateNames` **[data]**: `"a/b"`?, `""`, `"a b"`, `"origin"` twice.
- `listRemotesSorted`.
- `defaultRemoteFallbacks`: config set, config pointing at a removed remote, no `origin`, no remotes at all.
- `setDefaultRemoteMissingFails`.
- `updateRemoteRenameMovesTrackingRefsAndUpstream`.
- `updateRemoteUrlsOnly`.
- `removeRemoteTwiceFails`.
- `removeRemoteClearsUpstreamOfBranches`.
- `targetsAddEditDeleteDefault`: a duplicate fails; deleting the default target; a missing remote.
- `upstreamSetGet`: a branch with slashes, a missing local branch, `getUpstream` with no upstream, `.git/config` with comments and odd quoting (`getUpstream` reads the file directly, so give it adversarial config).
- `configEditsPreserveComments`: the user's `.git/config` comments and unknown sections survive remote and target edits.

### 3.15 Network through local file remotes
Each network op sleeps 1.5 s, so keep these scenarios few and big.
- `cloneFromLocalPathAndFileUrl` **[data]**: a plain path, `file://` with forward slashes, a path with spaces, a unicode path.
- `cloneToExistingNonEmptyDirFails`: the existing content is not deleted.
- `cloneFailureRemovesCreatedDir`: a bad URL leaves no dir behind.
- `cloneEmptyRemote`.
- `cloneSendsProgressEvents`: subscribe to `GitRepoManagerCloneRepoProgress`; first started, last finished; percents never go down.
- `cloneGitSshUrlRequiresCommercialPlan` **[data]**: `git@host:x.git`, `git://host/x` give `CloneRepositoryGitSshUrlRequiresCommercialPlan` with no dir created. The same applies to pushTag, pushAllTags, deleteRemoteBranch, deleteRemoteTag and addSubmodule.
- `fetchRemoteAndAll`: new branches and tags appear. `fetchAll` with one broken remote still returns true (current contract; assert and document).
- `fetchRemovedRemoteFails`.
- `pullFastForward`, `pullUpToDate`.
- `pullNoUpstreamFails` (`PullNoUpstreamConfigured`).
- `pullDivergedLeavesMergeState`: no commit; MERGE_HEAD; commit gives 2 parents.
- `pullDivergedConflict`.
- `pullRebaseMovesLocalCommits`.
- `pullRebaseConflictAborts`: state is clean and the local commit is intact.
- `pullWithDirtyWorkdir`: a conflicting dirty file is not overwritten.
- `pushFastForward`, `pushNothingIsOk`.
- `pushNoUpstreamFails`: and assert that `getLastError` is set (it currently isn't; see §6).
- `pushNonFastForwardRejected` gives `PushFailedToPushRemote`.
- `pushForceOverwrites`.
- `pushToDifferentlyNamedUpstreamBranch`.
- `pushToNonBareCheckedOutBranchRejected`.
- `deleteRemoteBranchKeepsLocal`.
- `deleteRemoteMissingBranch`.
- `remoteStatusAfterFetch` **[data]**: Ahead, Behind, Diverged.
- `fetchPrunesDeletedRemoteBranch`: document whether it does.

### 3.16 Submodules
- `addSubmoduleFromLocalRepo`: `.gitmodules` and the gitlink are staged; branch tracking; `getSubmodulePath`.
- `addSubmoduleDuplicatePathFails`.
- `addSubmoduleBadUrlCleansUp`: no half-written `.gitmodules` or `.git/modules`.
- `submoduleStatus` **[data]**: UpToDate; Outdated after a commit inside; Error after the checkout dir is deleted; uninitialised after a clone without recursion.
- `updateSubmoduleReclonesAndChecksOutPin`.
- `updateSubmoduleBranchTracking`.
- `deleteSubmoduleRemovesIndexConfigAndModules`.
- `nestedSubmodules`.
- `submoduleWithSpacesInPath`.

### 3.17 LFS
- `lfsPointerParseMake` **[data]**: valid, extra keys, wrong version, missing size, CRLF line endings, trailing junk, oversized.
- `lfsObjectPathLayout` (`aa/bb/oid`).
- `lfsStoreReadExists`, `lfsStorageStatusCounts`.
- `lfsStageAndCheckoutSmudge`: stage writes a pointer; a checkout from another clone fills the content from the local store.
- `lfsSmudgeMissingObjectDownloads` uses LfsStubServer: `lfs.url` points at the stub; batch download; the object is stored; the content is checked.
- `lfsDownloadErrors` **[data]**: 404, 401, malformed JSON, a hash mismatch, a size mismatch. No corrupt object is stored.
- `lfsUploadOnPush`: push uploads only the missing objects (stub records calls); a second push uploads nothing.
- `lfsUploadNoStoreIsNoop`.
- `lfsEndpointDiscovery` **[data]**: `lfs.url` > `remote.<r>.lfsurl` > derived from https, from a URL with `.git` suffix, from one without the suffix.

### 3.18 Signing
Skip if gpg or ssh-keygen is missing. ConfigOverride for the keys.
- `signCommitWithSshKey`: history reports signatureType and signature; `git verify-commit` with `gpg.ssh.allowedSignersFile` passes.
- `signCommitWithGpg`: a throwaway GNUPGHOME in the temp dir, key generated with `--quick-gen-key --batch --passphrase ''`.
- `signAmendAndRebase`.
- `signWithPassphraseSshKeyRejected`.
- `signMissingKeyFails`, `signBadProgramPathFails`: the commit is not created and the index is intact.
- `signingDisabledIgnoresSignFlag` (document).

### 3.19 Workflows (repository I/O; the pure helpers are already covered)
- `gitFlowFeatureReleaseHotfix` **[data]**: start and finish for each type; branch names from ConfigManager; `master` fallback; tags on release and hotfix; develop gets the merge back.
- `gitFlowFinishConflict`: state is reported; no branch is deleted.
- `gitFlowStartExistingBranchFails`.
- `gitFlowNextInitStartFinishUpdate`: start before init gives a not-initialized error; `gitflow.*` config is readable by the CLI; finish with tag; finish again with an existing tag fails before any change.
- `applyCustomGitFlowNextWorkflow`: base branches are created; the reference id and revision are stored; switching type clears them.
- `gitHubFlow`: start, update (merges main), finish (merges and deletes); updating main itself fails.
- `gitLabFlow`: feature start and finish; promote along the chain; promoting past the last environment fails; promotion creates a missing env branch; release branch name; `cherryPickToGitLabFlowRelease` with `conflicted`.
- `workflowTypeRoundTrip` via `sourcerer.workflow`.

## 4. tst_gitrepomanager.cpp: event-driven component

Uses the same `request()` / `REQUEST(name, cmd)` helper as the hg suite: `startWait`, `send`, `waitForAnswer({Result, Error}, 120 s)`.
- `requestsBeforeOpenFail`: the error contains "has not been opened".
- `initOpenCommitAndClose`: Init, Open, Status, Stage, Commit, History, ListLocalBranches; a bad checkout carries GitRepo's error text; Close; a request after close fails; reopen in the same uuid.
- `openTwiceFails` / `initOrCloneOnUsedUuidFails` ("already in use").
- `openNonRepositoryFailsAndCleansContext`: an open afterwards succeeds.
- `twoContainersAreIndependent`: two uuids and two repos, interleaved requests, no cross-talk. Closing one leaves the other working.
- `everyCommandTypeRoundTrips` **[data]**: one row per `GitRepoCommandType`, with a minimal valid command, asserting that a Result or Error event is received (never a timeout). This guards against unmapped events. **It should catch `GetLfsStorageStatus`, which has no mapping in `GitRepoManager::eventCallback`** (verified: only ProcessingState handles it).
- `errorEventsCarryGitErrorCode`: `GitRepoError.gitError` matches; `translateGitError` gives non-empty text.
- `cloneReportsProgress`.
- `fetchPullPushReportProgress`.
- `closeRefusesQueuedRequests`: 5 Status requests plus Close without waiting; every one is answered; nothing remains open.
- `closeDuringLongClone`: document the behaviour (no in-flight cancel); no deadlock; the thread joins.
- `managerStopWithOpenContexts`: `stop()` joins all worker threads.
- `credentialsOverlay` (needs the §2.4 seam + GitHttpServer):
  - wrong then right gives prompt, invalid, finished; `repoPath` is the clone destination
  - stored credentials are used without a prompt
  - cancel fails with "cancelled" after a single prompt
  - expired stored credentials prompt again
  - several URLs on one thread produce one Finished each (`finishPendingCredentialPrompts`)
- `sshKeyPassphrase` (Windows, fake `ssh.cmd` + SourcererAskPass, as in hg): wrong passphrase, remembered passphrase, cancel, a host-key question answered "no".
- `bridgeSmoke`: GitRepoManagerBridgeImpl maps 3–4 calls (open, status, commit, history) correctly, without the GUI.

## 5. tst_gitcredentials.cpp

Runs with `QStandardPaths::setTestModeEnabled(true)`.
- `storeGetRemoveRoundTrip`, `hasCredentials`, `getAllRepositoryPaths`, `getCredentialsInfo`.
- `persistenceAcrossInstances`: reload from the file.
- `encryptionIsNotPlaintext`: the password bytes do not appear in the file.
- `wrongMasterKeyFailsGracefully`: the file is corrupted or truncated, so the result is empty with no crash.
- `expiry` **[data]**: not expired, expired, expiry 0 (never).
- `clearMemoryVsClearAll`.
- `unicodeAndSpecialCharsInPassword` **[data]**: `"s3cret=!"`, `":"`, `"ü"`, an empty password, a 4 KB password.
- `basicAuthHeaderEncoding`: a `user:pa:ss` colon, unicode.
- `keyStorage`: set, get, remove a key pair per host; `generateKeyPair` (RSA-4096, slow, maybe one row); host matching with ports and case.
- `sshAuthInfoFallbackToDefaultKey`: `~/.ssh/id_ed25519` under the test HOME.
- `sshAuthInfoEncryptedKeyPrompts`: driven by the fake overlay.

## 6. Suspected defects for tests to pin down
Each was written as a test first. Status:
1. **Fixed.** `GetLfsStorageStatus` was not mapped in `GitRepoManager.cpp` eventCallback, so the request timed out (`everyRequestIsAnswered(GetLfsStorageStatus)`, §6.1 "§4–5").
2. **Fixed.** `push` with no upstream, `cloneRepository` mkpath failure and `initRepository` initial-commit failure returned false without `setError` (new `PushNoUpstreamConfigured` and `CloneRepositoryFailedToCreateDirectory`, §6.1 "§3.1–3.7" and "§3.15–3.19"). A null repository is a precondition (`Q_ASSERT`), and GitRepoManager answers requests that need a repository before calling GitRepo without one.
3. **Fixed.** `fetchAll` always returned true (`fetchErrors`, §6.1 "§3.15–3.19").
4. **Fixed.** `ssh://` URLs bypassed the commercial gate and never got SSH credentials (`commercialPlanGatesSshUrls`, §6.1 "§3.15–3.19").
5. **Fixed.** `certificateCheckCallback` accepted any TLS certificate and SSH host key. The policy is now (`GitRepoCertificates.cpp`):
   - A certificate libgit2 trusts (a trusted chain whose name matches the host; an SSH host key in `~/.ssh/known_hosts`) is accepted.
   - A self-signed TLS certificate (issuer equals subject, signed with its own key, whatever its dates or key usage) or an SSH host key that is not in `known_hosts` is trusted on first use. Its SHA-256 fingerprint is pinned per host (case insensitive, TLS and SSH separately) in `git_pinned_certificates.txt` in the application's config directory. The same certificate or key is accepted again; a different one is rejected with a message naming the host, the new fingerprint and where to trust it, and is remembered next to the pin (each line: kind, host, fingerprint, trusted since, and the refused fingerprint with its time; files with only the first three fields are still read).
   - UI: the Credentials manager lists the pinned HTTPS certificates and the SSH keys manager the pinned SSH host keys (`PinnedCertificatesWidget`): host, fingerprint, trusted since, and whether a different one was refused. "Trust new certificate"/"Trust new key" (`GitRepo::trustRejectedCertificate`) replaces the pin with the refused one after a warning that shows both fingerprints; "Forget" (`GitRepo::forgetPinnedCertificate`) drops the pin, so the next connection trusts what the server presents. `GitRepo::getPinnedCertificates` lists them. Tests: `oldPinsFileIsRead`, `pinnedCertificatesAreListed`, `rejectedCertificateCanBeTrusted`, `forgetOnePinnedCertificate`, and `httpsCloneChangedCertificateFails` trusts the new certificate and clones again. The widget itself has no automated test (it needs the GUI); it was checked by rendering it offscreen.
   - Everything else is rejected: an untrusted issuer, a certificate for another host, an unreadable certificate, other certificate types. The message says why (`is not trusted: <reason>`, `issued for another host`).
   - libgit2 is given the system's trusted roots (`QSslConfiguration::systemCaCertificates()`, `GIT_OPT_ADD_SSL_X509_CERT`) in `initForCurrentThread`. Its OpenSSL backend only reads OpenSSL's default paths, which are empty on Windows, so before this every HTTPS server was "invalid" there and worked only because the callback accepted everything.
   - Found by `httpsCloneTrustedChain`: libgit2 1.9 (`lib/libgit2/src/libgit2/streams/openssl.c`, `verify_server_cert`) compared IP addresses in `subjectAltName` inverted (`!!memcmp`), so a certificate for the right address failed and one for any other address of the same length matched. Patched in the vendored copy (marked `Sourcerer:`), covered by `httpsCloneTrustedChainOtherAddressFails`. Re-apply it when libgit2 is updated, unless upstream fixed it.
   - SSH caveat: libgit2 reports a key that is missing from `known_hosts` and one that differs from it the same way (`valid == 0`), so a host whose key in `known_hosts` changed is also trusted on first use here. For SSH, libgit2 replaces the rejection message with "invalid or unknown remote ssh hostkey"; `setError` puts the callback's reason back (`takeCertificateRejection`), so the user sees which host and where to trust the new key.
6. **Fixed.** The 5-argument `rebaseBranches` and `push(git_repository*)` declarations without definitions are gone from `GitRepo.h`.
7. **Fixed.** `getUpstream` parsed `.git/config` by hand. It now reads the branch configuration through `git_repository_config` (`upstreamAdversarialConfig`, `upstreamFromLinkedWorktree`, §6.1 "§3.8–3.14").

### 6.1 Confirmed by tests

#### §3.1–3.7
Each item below was covered by a `QEXPECT_FAIL` in `tst_gitrepo.cpp`. All are now fixed and their markers removed, except two libgit2 limits that are still marked: `initIntoUnusualPaths(long)` and the first half of `statusLongPaths` (without `core.longpaths`, files past MAX_PATH are missing from the status).

| Test | Defect |
|---|---|
| `commitBranchesTagsAndHeads` | **`getCommitTags` had an inverted comparison.** `git_oid_streq()` returns 0 on a match, so the function returns the tags of every *other* commit. |
| `discardStagedNewFileKeepsIt` | **Data loss.** Discarding a staged new file checks out HEAD over it, which deletes the file from disk. The Mercurial backend keeps it as untracked. |
| `blameCommittedAndUncommittedLines` | `blame` attributes lines from the committed file but takes the content from the working copy. Uncommitted lines are never reported as `kNullCommit`. |
| `blameCommittedAndUncommittedLines`, `blameEdgeCases` | libgit2 1.9 `git_blame_linecount()` includes an end sentinel, so every file gets one extra line with no commit (an empty file gets one line). |
| `patchAddsAndDeletesFiles` | `applyPatch` fails for any patch that creates a file. It writes the file first, then `git_apply` reports "1 conflict prevents checkout". |
| `diffCommittedEmptyFile` | `diffFiles` takes a committed empty file for a new file, so it adds a second delta and a `new file mode` header. |
| `initHonoursGlobalDefaultBranch` | `initRepository` fails when the user has `init.defaultBranch` set: `createInitialCommit` only renames a branch called `master`. |
| `initWithoutAuthorFails` | When the initial commit fails, `initRepository` leaves `.git` behind, so a retry fails because of `NO_REINIT`. |
| `commercialPlanSwitch` | `cloneRepository` creates the destination directory before the plan check and leaves it behind when the check refuses the clone. |
| `statusLongPaths` | Without `core.longpaths`, files past MAX_PATH are silently missing from the status. With it set, `stageFiles` still fails: its submodule probe at `<file>/.git` is too long. |
| `resetToUnknownRevisionFails` | `resetToCommit(..., ResetType::Unknown)` returns true. |
| `discardUnusualPaths` | `discardFiles` doesn't convert backslash separators (`stageFiles` and `blame` do). |
| `commitSignoffIsNotDuplicated` | A `Signed-off-by` line that is already in the message is appended again. |
| `stageNothing` | `unstageFiles({})` fails without an error message, while `stageFiles({})` succeeds. |
| `stageDirectory` | Staging a directory fails (the Mercurial backend stages it recursively). |
| `initIntoUnusualPaths(long)` | libgit2 refuses a `.git` path longer than MAX_PATH (a libgit2 limit, documented only). |

Behaviour that differs from the Mercurial backend but looks intentional, pinned as-is:
- `commitWithoutStagedFiles`: an empty commit is created instead of a `NothingToCommit` error.
- `statusLineEndings`: a line-ending-only change is listed as modified, matching `git status`, but `diffFiles` shows no lines.

#### §3.8–3.14
Each item below was covered by a `QEXPECT_FAIL` in `tst_gitrepo.cpp`, after it was checked against the source in `lib/GitRepo/src` (and libgit2 where it matters). All are now fixed (see "Fixes" below) and the markers are removed.

| Test | Defect |
|---|---|
| `mergeFastForwardKeepsLocalChanges` | **Data loss.** The fast-forward path of `merge` moves the branch and then calls `git_reset(GIT_RESET_HARD)`. libgit2 forces the checkout for a hard reset, so uncommitted changes to tracked files are thrown away. `git merge` keeps them. |
| `removeDirtyWorktreeNeedsForce` | **Data loss.** `removeWorktree` always prunes with `GIT_WORKTREE_PRUNE_WORKING_TREE` and never checks for uncommitted changes. `force` only overrides a lock. `git worktree remove` refuses a dirty worktree without `--force`. |
| `applyStashWithDirtyWorkdir` | `applyStash` adds `GIT_CHECKOUT_ALLOW_CONFLICTS`, so libgit2 skips a file that has local changes instead of failing. The apply returns true, but the stashed change to that file is not applied. |
| `mergeCreatesMergeCommit` | `merge` creates the merge commit but never calls `git_repository_state_cleanup`. `MERGE_HEAD` and `MERGE_MSG` stay behind, so `getIsMergeInProgress` stays true and `squashBranches` and `bisect` report the repository as busy. |
| `cherryPick` | `cherryPickCommit` uses the author passed in, not the author of the picked commit. |
| `rebaseConflictsAtSeveralSteps` | `commitRebase` passes the given signature as the author to `git_rebase_commit`, both for the resolved step and for every step after it. The original authors are lost (`git rebase --continue` keeps them). `rebaseBranches` itself keeps them. |
| `checkoutNestedRemoteBranch` | `checkoutBranch(Remote)` names the local branch after the last path component, so `origin/feature/x` is checked out as `x`. |
| `remoteBranchStatus` | `listRemoteBranchesWithStatus` pairs a remote branch with the local branch that has the same last path component, so `origin/feature/x` is compared with `x` and reported as `NoUpstream`. It also pairs by name, not by the configured upstream. |
| `checkoutRemoteBranchWhenLocalExists` | When the local branch exists but is at a different commit, `checkoutBranch(Remote)` checks out the remote commit's tree and points HEAD at the local branch. The working copy no longer matches HEAD. The local branch's upstream is also overwritten. |
| `checkoutTagDetaches` | `getActiveBranch(Tag)` compares HEAD with the id of the tag object, so a checked-out annotated tag is never reported. |
| `activeBranchQueries` | `getActiveBranch(Tag)` checks for a detached HEAD with the reference from `git_repository_head`, which is already resolved and therefore always direct. A tag on the tip of the current branch is reported as active even though HEAD is attached. |
| `renameBranchInvalidNames(HEAD)`, `(leading dash)` | `renameBranch` uses `git_branch_move`, which doesn't apply the branch-name rules that `git_branch_create` applies. A branch can be renamed to `HEAD` or `-x`. |
| `createBranchUpstream` | `createBranchFromCommit` always sets an upstream to `<default remote>/<branch>`. With no remotes at all, `getDefaultRemote` returns `"origin"`, so the branch gets an upstream on a remote that doesn't exist. |
| `defaultRemoteFallbacks` | `getDefaultRemote` falls back to `"origin"` without checking that it exists. When the only remote is `upstream`, it returns `origin`. |
| `renameRemoteMovesRefsAndUpstream` | `updateRemote` renames the remote but doesn't update `sourcerer.defaultremote`, so a chosen default is lost after a rename. |
| `updateRemoteUrlsOnly` | `updateRemote` with an empty push URL always fails: `git_remote_set_pushurl` refuses an empty URL ("cannot set empty URL"). The rename and the fetch URL are applied before that point, so the call fails half done. |
| `targetsAddEditDeleteDefault` | `addTarget` doesn't check that the remote exists. It writes `remote.<missing>.target.<name>.*` anyway. |
| `targetsAddEditDeleteDefault` | `setDefaultTarget` saves the previous default URLs as a target named `Default`, and `listRemotes` skips that name. The previous URLs disappear from the list. |
| `listRemotesSorted` | Targets are stored as `remote.<remote>.target.<name>.fetchUrl` and `pushUrl`. git reads these as a remote named `<remote>.target.<name>`, so it appears in `git remote`. |
| `upstreamAdversarialConfig` | The hand-written fallback in `setUpstream` (used when the remote branch doesn't exist) only recognises `remote =` and `merge =` with spaces. With `remote=origin` it adds a second entry, so the key ends up with two values. |
| `upstreamAdversarialConfig` | The fallback in `getUpstream` opens `<workdir>.git\config` (built with `QDir::separator`). libgit2 then takes the working directory as the file's directory and resolves relative `[include]` paths against it, so an upstream set in an included file is not found. |
| `upstreamFromLinkedWorktree` | In a linked worktree, `setUpstream`'s fallback writes `<git_repository_path>/config`, which is `.git/worktrees/<name>/config`. git doesn't read that file. |
| `upstreamFromLinkedWorktree` | In a linked worktree, `getUpstream`'s fallback opens `<workdir>/.git/config`, but `.git` is a file there, so the upstream is not found. |
| `bisectCliCompatibility` | A session started by `bisect` can't be continued with `git bisect good`/`bad`. git 2.55 stops with "could not open '.git/BISECT_NAMES'", which `bisect` never writes. (Starting with git and finishing with `bisect` works.) |
| `bisectCliCompatibility` | `Reset` removes `refs/bisect/*`, `BISECT_START` and `BISECT_LOG`, but leaves behind the state files git adds (`BISECT_TERMS`, `BISECT_NAMES`, `BISECT_EXPECTED_REV`). |

Found in the source (also fixed):
- `setDefaultTarget`: if the target had a `fetchUrl` but no `pushUrl` key (for example after a hand edit), `newPushUrl = newFetchUrl` and both were then `free`d. That was a double free. The function was rewritten without raw buffers.
- `merge` returned false on conflicts without calling `setError`, so `getLastError` was stale. It now reports the new `MergeFailedWithConflicts` (`mergeConflictAndAbort`).
- `applyStash` passed absolute native paths to `markConflictedFiles`. It now passes paths relative to the working directory.
- `applyStash` used the index libgit2 had read earlier, so after git changed the index the apply failed with "uncommitted changes exist in the index". It now forces a reload of the index first (`applyStashConflict` commits with git right before applying).
- `bisect(repo, action, commit, status)` reset `status` before it read `commit`, so passing `status.currentCommit` passed an empty commit. The commit is now copied first.
- `revertCommit` and `cherryPickCommit` cast the result of `git_revparse_single` to `git_commit*` without peeling it. They now peel it to a commit.

Fixes:
- `merge`: the fast-forward path checks out the new tree with `GIT_CHECKOUT_SAFE` and only then moves the branch. Unrelated local changes are kept, and conflicting ones stop the merge with `MergeFailedToCheckoutDestinationTree`. A merge commit calls `git_repository_state_cleanup`.
- `removeWorktree` without `force` refuses a worktree with changes, untracked files included (new `RemoveWorktreeHasChanges`).
- `applyStash` no longer uses `GIT_CHECKOUT_ALLOW_CONFLICTS`. A file with local changes fails the apply with `ApplyStashFailedToApplyStash`; conflicts between the stash and HEAD are still written.
- `cherryPickCommit` keeps the picked commit's author. `commitRebase` keeps the original authors of every step. In both, the signature passed in is the committer.
- `checkoutBranch(Remote)` and `listRemoteBranchesWithStatus` strip the remote name (`git_branch_remote_name`) instead of keeping only the last component. `listRemoteBranchesWithStatus` pairs a remote branch with the local branch that tracks it first, then with the local branch of the same name. When the local branch exists, `checkoutBranch(Remote)` checks it out where it is and only sets an upstream if it has none.
- `getActiveBranch(Tag)` checks `git_repository_head_detached` and peels tags to their commit.
- `renameBranch` checks the new name with `git_branch_name_is_valid` (new `RenameBranchInvalidName`).
- `getDefaultRemote` returns only remotes that exist: the chosen default, the current branch's upstream remote, `origin`, or else the first remote by name. Without remotes it returns an empty string. Branches created without remotes therefore get no upstream.
- `updateRemote` removes the push URL when it is given an empty one. A rename also moves the targets and `sourcerer.defaultremote`.
- Targets are stored as `sourcerer-target.<remote>:<target>.fetchUrl`/`pushUrl`, which git doesn't read as remotes (remote names can't contain `:`). The old `remote.<remote>.target.<target>.*` keys are migrated by `listRemotes` and by every target function (`legacyTargetsAreMigrated`). `addTarget` and `editTarget` need an existing remote (new `AddTargetRemoteNotFound`). `setDefaultTarget` swaps the URLs, so the previous default stays listed under the target's name. `removeRemote` removes the remote's targets.
- `setUpstream` and `getUpstream` read and write the branch configuration through `git_repository_config` instead of editing `.git/config` by hand. That handles git's syntax, includes and linked worktrees.
- `bisect` writes `BISECT_TERMS` and `BISECT_NAMES` when it starts a session, like `git bisect start`. `Reset` removes every bisect state file git uses.

Behaviour that differed from git, now aligned with it:
- `deleteBranch(repo, name, force = false)`: like `git branch -d`, a branch whose tip is in neither its upstream nor HEAD is refused (new `DeleteBranchNotFullyMerged`) unless `force` is set. `GitRepoDeleteBranchCommand` has a `force` field for it (no UI sets it yet), and the workflows pass `force` after they integrated the branch themselves (`deleteBranch`).
- `cherryPickCommit` and `revertCommit` use `git_cherrypick` and `git_revert`. Conflicts leave `CHERRY_PICK_HEAD` or `REVERT_HEAD` and fail with the new `CherryPickCommitFailedWithConflicts` or `RevertCommitFailedWithConflicts`. `getIsMergeInProgress` already reports these states, and `abortMerge` now aborts them too. Local changes in the way stop them before anything is touched. A result that changes nothing makes no empty commit (new `CherryPickCommitIsEmpty`, `RevertCommitIsEmpty`). Both reload the index first, like `applyStash`. `cherryPickToGitLabFlowRelease` reports conflicts through `conflicted` as before (`cherryPickConflict`, `revertConflict`, `cherryPickAlreadyApplied`).
- `merge(..., allowUnrelatedHistories = false)` refuses branches without a common commit (new `MergeUnrelatedHistories`) unless allowed (`mergeUnrelatedHistories`).
- `listConflictsForStashFile` returns the file only when it has conflict markers, like `listConflictsForAFile`. The stash index is not needed, the conflicts are in the working copy.
- `listTags`, `listLocalBranches` and `listRemoteBranches` are sorted, loose and packed references together. Symbolic references such as `origin/HEAD` are not listed (`git branch -r` shows them as pointers), so they get no status either.
- `getActiveBranch(Remote)` returns the upstream of the current branch, empty on a detached HEAD.
- `addTarget` refuses an existing target (new `AddTargetAlreadyExists`). `editTarget`, `deleteTarget` and `setDefaultTarget` need an existing target (new `EditTargetNotFound`, `DeleteTargetNotFound`).
- `removeWorktree`, `lockWorktree` and `unlockWorktree` report a name that is not a worktree as `...FailedToLookupWorktree`. `...MainNotAllowed` is only for the main worktree's name.
- `setUpstream` refuses a remote that doesn't exist (new `SetUpstreamRemoteNotFound`).

#### §3.15–3.19
Each item below was covered by a `QEXPECT_FAIL` check in `tst_gitsync.cpp` or `tst_gitflows.cpp`, after checking the source. All are now fixed (see "Fixes" below) and the markers are removed; the tests also check the new error codes.

| Test | Defect |
|---|---|
| `addSubmoduleKeepsExistingSubmodule` | **Data loss.** `addSubmodule` starts with `deleteSubmodule` for the name, so adding a submodule under a name that is already in use deletes the existing submodule's working copy (untracked files included) and its `.gitmodules` entry. Then the add fails. |
| `deleteSubmoduleKeepsOtherSubmodules` | **Data loss.** `deleteSubmodule` filters `.gitmodules` and `.git/config` with `strstr(line, name)`, so deleting `sub` also drops the sections of `sub2` (or of any submodule whose path or URL contains the name). |
| `lfsEndpointDiscovery` | **LFS configuration is never read.** `getConfigString` calls `git_config_get_string` on a live config, which libgit2 refuses ("get_string called on a live config object"; snapshots only). `lfs.url`, `remote.<name>.lfsurl` and `remote.<name>.lfspushurl` are ignored, and the endpoint is always derived from the remote URL. |
| `lfsStageAndSmudgeFromLocalStore` | `discardFiles` writes the blob from HEAD itself, without the LFS filter. Discarding a change to an LFS file replaces it with the pointer text. |
| `addSubmoduleWithBranchAndSpaces` | `checkoutSubmoduleBranch` moves HEAD to the tracked branch and only then checks out HEAD with `GIT_CHECKOUT_SAFE`. The files of the remote's default branch stay, so a submodule added for `dev` shows `main`'s files as local changes. |
| `tagsOnRemote` | `pushTag` overwrites a tag that exists on the remote with another target. `git push` refuses without `--force`. |
| `tagsOnRemote` | `deleteRemoteTag` keeps only the part after the last `/`. Deleting `release/1.0` deletes `1.0` instead. |
| `deleteRemoteBranch` | `deleteRemoteBranch` only strips `origin/`. `upstream/topic` is deleted as `refs/heads/upstream/topic` on origin, and the branch on upstream stays. |
| `pullNamedBranchNotCheckedOut` | `pull(repo, "other")` fast-forwards the reference HEAD points to, so the current branch is moved to the other branch's upstream commit and `other` is not updated. |
| `signRebasedCommits` | `commitRebaseSigningCallback` signs the commit message instead of the commit buffer. Rebased commits carry a `gpgsig` header that doesn't verify. |
| `commercialPlanGatesSshUrls` | `addSubmodule` runs `git_submodule_add_setup` before the plan check. A refused `git@`/`git://` submodule leaves its `.gitmodules` entry and config behind. |
| `commercialPlanGatesSshUrls(ssh)` | `ssh://` URLs are not checked against the plan (only `git@` and `git://` are), §6 item 4. |
| `fetchErrors` | `fetchAll` returns true when a remote could not be fetched, §6 item 3. |
| `pushWithoutUpstreamSetsError`, `pullRebaseConflictRestoresBranch` | `push` without an upstream, and `pull` with rebase stopped by conflicts, return false without `setError`, §6 item 2. |
| `submoduleStatuses` | `listSubmodules` leaves out a submodule whose directory is missing, so it is never reported as `Error`. (One that is registered but not initialized, with an empty directory, is reported as `Error`.) |
| `cloneFailureRemovesCreatedDirectory` | `cloneRepository` creates missing parent directories with `mkpath` and removes only the last one when the clone fails. |
| `lfsPointerParse(pointer inside a bigger file)` | `parsePointer` ignores unknown lines and has no size limit, so any file that contains the three pointer lines is taken for a pointer. git-lfs only accepts files of at most 1024 bytes that have nothing but pointer lines. |
| `lfsEndpointDiscovery(derived without .git)` | For a remote URL without `.git`, git-lfs uses `<url>.git/info/lfs`. Sourcerer uses `<url>/info/lfs`. |
| `lfsSmudgeDownloadsMissingObject` | LFS requests ask the user for credentials (`getBasicAuthHeader` → `acquireCredentials`) before trying without them. A public LFS server still prompts. |
| `lfsPushToLocalRemote` | No LFS endpoint can be derived from a local (path or `file://`) remote, so pushing a repository with LFS content to one fails as a whole. git-lfs copies the objects into the remote's `lfs/objects`. |
| `gitFlowStartAndFinish(release)`, `(hotfix)` | `finishGitFlowBranch` creates no tag for a release or hotfix (git flow tags them on main). |
| `gitFlowStartFailures` | `startGitFlowBranch(Unknown, "x")` creates and checks out a plain branch `x` instead of refusing the type. |
| `gitFlowNextInit` | `initGitFlowNext` writes the classic preset over an existing configuration, which loses a branch model the user changed. |

Also found while writing these tests (also fixed):
- `GitRepo.h` declared `push(git_repository*)` and the 5-argument `rebaseBranches` without defining them (§6 item 6). The `push` declaration made `GitRepo::push(repo)` ambiguous with `push(repo, branch = {}, force = false)`. Both declarations are removed.
- `rebaseBranches` (and so `updateGitFlowNextBranch` and the rebase strategy of `finishGitFlowNextBranch`) failed with "Uncommitted changes exist in index" right after git changed the index while the repository was open, the stale-index problem `applyStash` had. It now reloads the index first, like `applyStash`.
- libgit2's local transport pushes to a remote's fetch URL and ignores `pushurl` (`transports/local.c`). This is a libgit2 limit. The LFS upload is therefore tested through `Lfs::uploadLocalObjects` directly (`lfsUploadLocalObjects`).

Fixes:
- `addSubmodule` checks the plan before anything is written, and refuses a name that is in use (new `AddSubmoduleAlreadyExists`) instead of deleting the existing submodule first.
- `deleteSubmodule` removes the `submodule.<name>.*` variables of `.gitmodules` and `.git/config` through `git_config` (exact name), then the section headers left empty. The index entry is removed by the submodule's path. A new `DeleteSubmoduleFailedToUpdateConfig` reports a config that can't be written.
- `listSubmodules` lists a submodule that is in `.gitmodules`, the index or the working copy, so one whose directory is missing is reported as `Error`. One that is only in HEAD (deleted, not committed) is still left out.
- `checkoutSubmoduleBranch` checks out the branch's tree (`GIT_CHECKOUT_SAFE`, compared with the current HEAD) before it moves the branch and HEAD.
- `isSshOrGitUrl` (`git@`, `git://`, `ssh://`) is the one plan gate for clone, fetch, pull, push, the tag and remote-branch functions and submodules. The credentials callback gives `ssh://` URLs SSH keys, and `extractSshHost` reads the host of an `ssh://` URL (it took `ssh` for the host).
- `pushTag` refuses to move a tag that exists on the remote with another target (push negotiation callback, new `PushTagExistsOnRemote`). `deleteRemoteTag` keeps the whole tag name. `deleteRemoteBranch` finds the remote from the remote-tracking branch (`<remote>/<branch>`), the local branch's upstream, or a `<remote>/` prefix, in that order.
- `pull(repo, branch)` for a branch that isn't checked out only fast-forwards that branch, and refuses one that diverged (new `PullBranchNotCheckedOutCannotFastForward`). A pull with rebase stopped by conflicts reports the new `PullRebaseFailedWithConflicts`; one that fails in `git_rebase_next` is aborted too.
- `push` without an upstream reports the new `PushNoUpstreamConfigured`. `fetchAll` fetches every remote and returns false (with the error of the failing remote) when one fails.
- `cloneRepository` removes every directory it created when the clone fails, and a failed `mkpath` reports the new `CloneRepositoryFailedToCreateDirectory`.
- `commitRebaseSigningCallback` signs the commit buffer, like `commitSigningCallback`.
- LFS: `getConfigString` reads a config snapshot. Endpoints follow git-lfs: `lfs.pushurl` (upload), `lfs.url`, `remote.<name>.lfspushurl` (upload), `remote.<name>.lfsurl`, then the remote URL, with `.git` added before `/info/lfs` when the URL lacks it. Requests go out without credentials first and ask for them only on a 401. A path or `file://` remote without a configured endpoint gets the objects copied into (and read from) its `lfs/objects`, like git-lfs. `parsePointer` takes only files of at most 1024 bytes that start with the version and have nothing but pointer lines (`ext-*` included).
- `discardFiles` writes the HEAD blob through `git_blob_filter`, so LFS and line-ending filters apply.
- Git Flow: `finishGitFlowBranch` tags a release or hotfix on the primary branch with its name, and refuses one whose tag exists before merging (new `GitFlowTagExists`). `startGitFlowBranch` and `finishGitFlowBranch` refuse `GitFlowBranchType::Unknown` (new `GitFlowUnknownBranchType`). `initGitFlowNext` only writes the preset keys that are missing.

Left as it is: `pushAllTags` still moves tags that exist on the remote; `git push --tags` refuses those one by one.

Behaviour checked and already the same as git:
- `fetchRemote` doesn't overwrite a local tag with a remote tag of the same name (`fetchDoesNotClobberLocalTags`). It prunes deleted remote branches only with `fetch.prune` (`fetchPrunesOnlyWhenConfigured`).
- `push` refuses to update the checked-out branch of a non-bare remote (`pushToCheckedOutBranchOfNonBareRemote`).
- A diverged `pull` leaves the merge for the user to commit. This is on purpose and unlike `git pull` (`pullDivergedLeavesMergeToCommit`).
- `listSubmodulesWithStatus` compares a submodule with the commit pinned in HEAD, not in the index. After `updateSubmodule` it is `Outdated` until the new pin is committed (`updateSubmoduleRecordsCurrentCommit`).

#### §4–5
These were covered by `QEXPECT_FAIL` checks in `tst_gitrepomanager.cpp`, `tst_gitcredentials.cpp` and `tst_gitsync.cpp`, after checking the source. All are now fixed (see "Fixes" below) and the markers are removed; the tests check the fixed behaviour.

| Test | Defect |
|---|---|
| `deleteSubmoduleRefusesOtherDirectories` (gitsync) | **Data loss.** `deleteSubmodule` ends with `QDir(submodulePath).removeRecursively()` and never checks that the path is the submodule's. An empty path is the process's current directory, which is deleted. `GitRepoManagerBridgeImpl::deleteSubmodule` passes `<repository>/<name>`, so an empty name deletes the whole working copy. |
| `rejectedRequestsKeepTheOpenRepository` | **The open repository is closed by a refused request.** A second `OpenRepo`, `CloneRepo` or `InitRepo` in a container that is in use is refused with `<Open|Clone|Init>RepoError`. The processing state answers every such error with `destroyContext()` for the container, so the repository that was open is closed and its requests fail with "has not been opened". |
| `reopenAfterClose` | `CloseRepo` closes the repository but never destroys the context. Its worker thread keeps running until the manager stops, and the next `OpenRepo` in the same container fails with "Git repository has already been opened." |
| `closeAnswersQueuedRequests` | `CloseRepo` sets the context's `quit` flag as soon as it is scheduled. Readers already queued (Status, history, ...) see it and drop their answer, so callers wait for their timeout. |
| `everyRequestIsAnswered(GetLfsStorageStatus)` | `GitRepoManager` doesn't subscribe to `GitRepoManagerGetLfsStorageStatus`, so the request is never handled (§6 item 1). |
| `everyRequestIsAnswered(GetCommitTags)` | `getCommitTagsWorker` answers a commit without tags with `GetCommitTagsError` instead of an empty list. |
| `everyRequestIsAnswered(AbortMerge, AbortRebase, EditTarget, GetUpstream, UpdateRemote)` | The error events have an empty (or stale) message: GitRepo returns false without `setError` (nothing to abort, no upstream, missing remote or target). |
| `httpCredentials` | A wrong HTTP password stays in the per-URL credentials cache, and libgit2 is given it again for every 401 until it gives up. A clone is not retried, so the user is never asked again. (Fetch and push retry, so there the second answer works.) |
| `tamperedEntryIsRejected` | The credentials file is AES-CBC without a MAC. A changed byte loads changed credentials instead of being rejected. |
| `pathsAreExactKeys` | Stored credentials are keyed by the path string as given. A trailing slash or native separators find nothing. |
| `keyPairsPerHost` | SSH key pairs are looked up by the exact host string, so `EXAMPLE.invalid` doesn't find `example.invalid`. |
| `sshPassphrasePrompt(OpenSSH ed25519)` | `getSshAuthInfo` only asks for a passphrase when the key file contains "ENCRYPTED", which only PEM keys have. A passphrase-protected key in the default OpenSSH format is never asked for, so libssh2 fails. |
| `sshPassphraseIsCheckedAndRemembered` | The passphrase is neither checked (a wrong one is passed on to libssh2) nor remembered (every connection asks again). `validateSshKey`, which could check it, is never called. |

Found in the source (also fixed):
- In a debug build, a `Status` request after `CloseRepo` in the same container hit `Q_ASSERT(repo != nullptr)` in `GitRepo::getStatus`: `statusWorker` passed the closed (null) repository on.
- `GitRepoManagerProcessingState::context()` used `_contexts[uuid]`, which inserted an empty entry for every unknown container it was asked about.
- `GitCredentialStorage::loadPersistedCredentials` trusted the entry count in the file: a damaged count made it run PBKDF2 (100 000 iterations) for every missing entry.
- `GetLfsStorageStatus` was not dispatched by the worker loop either, only scheduled.

Not a defect of the code, but a side effect of testing it: `GitCredentialStorage` keeps its master key in the OS keychain under the application's name (`qAppName()`). Test programs that used it before `useTestCredentialsKey()` existed (`gitsync_test`, and `hgrepomanager_test` on the hg branch) have added keychain entries under their own names. They can be removed in Windows Credential Manager.

Fixes:
- `deleteSubmodule` refuses an empty or path-like name (new `DeleteSubmoduleInvalidName`). It removes only a directory inside the working copy: the submodule's path from its metadata, or, without metadata, the given directory if it is a git checkout. Never the working copy itself or anything outside it.
- GitRepoManager contexts remember the request that made them (`ownerSyncUuid`) and the close that ends them (`closeSyncUuid`). Only those answers destroy a context; a refused Open/Clone/Init leaves the open repository alone. `CloseRepoResult` destroys the context, so the worker thread ends and the container can open a repository again.
- Once `CloseRepo` is scheduled, new requests for the container are refused ("has not been opened"), and readers still queued answer with "The repository was closed." instead of dropping their answer. Requests queued behind an open that failed are answered with their error event without running (a table from command type to error event, `errorEventFor`).
- `GitRepoManager` subscribes to `GitRepoManagerGetLfsStorageStatus`, and the worker loop dispatches it.
- `getCommitTagsWorker` answers a commit without tags with an empty list; only a failed lookup is an error.
- `setError` uses GitRepo's own message for the error when libgit2 reported none, and the new `setOwnError` does so for errors GitRepo detects itself. `abortMerge`, `abortRebase` (nothing to abort), `getUpstream` (none configured), `updateRemote` (missing remote or empty arguments) and `editTarget` (empty arguments, the Default target) set errors (new `AbortMergeNothingToAbort`, `AbortRebaseNothingToAbort`, `GetUpstreamNotConfigured`, `UpdateRemoteNotFound`, `UpdateRemoteInvalidArguments`, `EditTargetInvalidArguments`).
- `credentialsAcquireCallback` counts the requests per operation (`GitCredentialPayload::credentialAttempts`). A second request means the credentials were rejected: they are dropped from the cache, `GetCredentialsInvalid` is sent and the user is asked again (stored credentials are skipped), up to five times.
- `getSshAuthInfo` recognises passphrase-protected OpenSSH keys (cipher in the `openssh-key-v1` header) as well as PEM keys, checks the passphrase (OpenSSL for PEM keys, `ssh-keygen -y` for OpenSSH keys when it is available) and asks again after a wrong one, and remembers a working passphrase per key until `clearCachedCredentials`.
- GitCredentialStorage normalises repository paths (`QDir::cleanPath` of `fromNativeSeparators`). New entries carry an HMAC-SHA256 of the IV and ciphertext (prefixed `SGC2`), and a changed entry is rejected; entries written before have no MAC and are still read. Loading stops at the end of the data whatever the count says.
- GitKeyStorage looks up hosts case-insensitively.

#### §4–5, second pass
| Test | Defect |
|---|---|
| `generateKeyPairRefusesBadNames` | `GitKeyStorage::generateKeyPair` used the key name as a path below `.ssh` unchecked: `../name` wrote the key pair outside `.ssh`, and an empty name wrote `.ssh/` and `.ssh/.pub`. |
| `generateKeyPair`, `generateKeyPairWithPassphrase` | On Windows the private key inherited the folder's permissions (the code set owner-only permissions on other systems only). OpenSSH refuses it ("UNPROTECTED PRIVATE KEY FILE"), so a key made in Sourcerer didn't work with `ssh` or `git` on the command line. |
| `defaultKeyFallback` | Without a key stored for the host, only `~/.ssh/id_ed25519` was tried. A user with only `id_rsa` or `id_ecdsa` (which `ssh` finds) got no key. |
| `credentialPromptsFinishOncePerUrl` | Every credential prompt was closed twice: `finishCredentialsAcquiring(url)` sent `GetCredentialsFinished` without forgetting the prompt, and `finishPendingCredentialPrompts` sent it again at the end of the command. It was also sent after every fetch, push and clone for URLs that never prompted. |

Found in the source (also fixed): when the public key could not be written, `generateKeyPair` left the private key behind, and the next attempt failed with "Key pair already exists".

Fixes:
- `generateKeyPair` refuses empty, `.`/`..` and names with path separators, `:`, wildcards or control characters ("Invalid key name").
- The private key file is restricted before the key is written to it: on Windows a protected ACL with a single entry for the current user (as `ssh-keygen` writes it), elsewhere mode 600. Qt's owner-only permissions are not enough on Windows: OpenSSH objects to the entry Qt adds for the token's group.
- The default key is the first of `id_ed25519`, `id_ecdsa`, `id_rsa` that has both files (`id_ed25519` first, as before).
- `finishCredentialsAcquiring` closes only a prompt raised on the thread, and forgets it, so each prompt gets exactly one `GetCredentialsFinished`.
- A failed public key write removes the private key.

Already the same as git (listed here before by mistake):
- `rebaseBranches` drops merge commits and replays the commits they bring in, checks out the rebased branch, and an abort returns to that branch. `git rebase <upstream> <branch>` without `--rebase-merges` does the same.
- `stash` takes the stasher from `user.name` and `user.email`, like `git stash`, and fails without them (`StashFailedToGetDefaultSignature`).

## 7. Order of work
1. Infrastructure (§2), the commercial-plan seam, and move the 3 existing tests onto the helpers.
2. §3.1–3.7 (local, fast, highest value).
3. §3.8–3.14.
4. §3.15–3.16 (file remotes).
5. §4 manager suite, including `everyCommandTypeRoundTrips`.
6. GitHttpServer + credentials (§4 overlay, §5).
7. LFS stub (§3.17), signing (§3.18), workflows (§3.19).
8. Fix or `QEXPECT_FAIL` §6 items, and wire everything into `ctest`. Done.

Estimated size: about 230 test slots (about 70 data-driven), giving roughly 3–4× the hg suite's assertion count. Keep the runtime of `gitrepo_test` under 3 minutes by batching network scenarios (each costs ≥1.5 s).
