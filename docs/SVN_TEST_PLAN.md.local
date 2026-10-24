# Subversion Test Plan

This plan gives Subversion (`lib/SvnRepo` and `component/SvnRepoManager`) the same coverage that `docs/GIT_TEST_PLAN.md` gave Git, and goes further on edge cases. SvnRepo is a fresh port with no real tests, and it maps Git concepts (index, stash, submodules, remotes, amend, reset) onto Subversion features that behave differently. Expect most defects at those seams.

## 1. Current state

| Area | Covered today |
|---|---|
| `tests/svnrepo/tst_svnrepo.cpp` | 16 scenario slots on **one shared working copy** created in `initTestCase`. The slots depend on each other's state. No `init`/`cleanup`, no `_data` tests, no CLI oracle, no config isolation, almost no error-code asserts. No `svnrepo_test` binary exists in the build trees, so it has probably never run. |
| SvnRepoManager | **Nothing** |
| Credentials / certificates over svn | **Nothing** |

**The existing tests and the code disagree on the staging model.**
- The tests assume tracked modifications are staged automatically and that `unstageFiles` fails with "no staging area" (`tst_svnrepo.cpp:172-176`).
- The code stages through the `sourcerer-staged` changelist (`SvnRepoPrivate.h:144`, `SvnRepoWorkingCopy.cpp:290`). A plain edit is reported as `modifiedFiles`, and `unstageFiles` succeeds.
- `commitAll` stages only new files. `patches()` and `conflictedUpdateAndAbort()` call `commitStaged` without staging, so they hit "Nothing is staged to commit." (`SvnRepoWorkingCopy.cpp:553`).

**Decided (2026-10-19):** "staging" is the user's selection of the files to commit, implemented with the changelist. Unstaged changes are never committed together with staged ones. This matches `SvnRepo.h:177-179`, so the existing tests are replaced (§8, step 1). The other behaviour decisions are in §7.4.

### Progress
- **Done:** `RepoFeatures::CommitAmend` (`RepoManagerBridge.h`). Only `GitRepoManagerBridgeImpl::features()` returns it. `RepoContainer` hides "Amend previous commit" in the commit pane's drop-down menu without it, and never amends while it is hidden.
- **Done (§8 step 1, 2026-10-22):**
  - `SvnRepo::Testing` (`SOURCERER_SVNREPO_TESTING`, set on the SvnRepo target by `tests/svnrepo/CMakeLists.txt`): `setConfigDirectory` (used for `svn_config_get_config` and as `SVN_AUTH_PARAM_CONFIG_DIR`), `sslServerTrustCheck`, `installMalfunctionHandler` with `takeMalfunctions` (every test's `cleanup` fails if a malfunction was recorded), and `resetLastError`.
  - `tests/svnrepo/SvnTestHelpers.h`: file helpers, `EXPECT_SVN_ERROR(kind)` (kind and a non-empty message), `resetLastError`, `svn`/`svnIn`/`svnadmin`/`svnlook` wrappers (`--non-interactive --config-dir <tmp>/svnconfig` for svn; svnadmin and svnlook take neither), the XML status/info/log oracles, `svnPropget`, `svnlookChanged/Author/Log/Date`, `svnlookRevprop` (the stored bytes, CRLF included), `head`, `makeRepo(dir, Layout)`, `loadDump` with `DumpBuilder` and the §2.4 dumps, `secondWc`, `svnCommitAll`, `commitAll` (stages everything, no update), `addPreRevpropChangeHook`, `ConfigOverride`, `useTestCredentialsKey` and `unusualNames()`. The tools are looked up in `PATH`, then in `C:\Program Files\TortoiseSVN\bin`; the suite is skipped without them.
  - `tst_svnrepo.cpp` uses per-test fixtures (a fresh `QTemporaryDir`, isolated config, `initRepository` + `openRepository`). The 16 slots are rewritten for the changelist staging model and §7.4, with oracle checks. `unstagingAModificationFails` became `unstageModificationKeepsIt`, and `conflictedUpdateAndAbort` became `conflictedUpdateIsNotAbortable`. `fixtureRepositories` checks the dumps, `makeRepo` layouts, the pre-revprop-change hook (none, refusing, allowing) and `secondWc`. 19 passed (with initTestCase/cleanupTestCase), 18 `QEXPECT_FAIL`s. `ctest -R svnrepo` passes, about 20 s.
  - `amend` and `resetToCommit` are in `SvnRepoUnsupported.cpp` and return `UnsupportedOperation` (B6, B29 closed; checked in `gitOnlyOperationsAreUnsupported`, which also checks that the working copy is untouched). The SvnRepoManager Amend events (`SvnRepoManagerAmend*`, `SvnRepoAmendCommand`, workers, FSM name) are removed, and the Svn bridge's `amend` only reports the operation as unsupported.
  - `discardUntrackedFileKeepsIt` in `tst_gitrepo.cpp` passes.
- **Done (§8 step 2, 2026-10-22):** the operation-state design for merge, cherry-pick and revert is §3.11.1: a state file in the admin directory, the start sequence with its data-loss guard, what happens while an operation is in progress, abort, and messages. `SvnRepo.h` has the nine new `SvnError` values (`MergeFailedWithConflicts` … `AbortMergeNothingToAbort`, appended before `Last`) and the contract as header comments on `merge`, `cherryPickCommit`, `revertCommit`, `getIsMergeInProgress`, `abortMerge` and `commitStaged`. Nothing implements it yet. `conflictedUpdateIsNotAbortable` now expects `AbortMergeNothingToAbort` (B9). Decided: §3.11.1 step 4 updates only the operation's paths to HEAD before applying it (§7.4 item 9).
- **Done (§8 step 3, 2026-10-22):** §3.1–3.7 in `tst_svnrepo.cpp`: 120 test functions, 352 passing rows, 155 `QEXPECT_FAIL`s and 3 skips; `ctest -R svnrepo` passes in the build container. Section 7.5 lists which suspected defects the run confirmed and which it didn't reproduce, plus the 20 new ones (B30–B49).
  - Names that differ from the plan: `initIgnoresDefaultBranch`, `openWorkingCopiesOfBothVersions`, `successfulCallDoesNotResetTheError`, `abortMergeAfterConflictedOperation` (one `_data` test for merge, cherry-pick and revert), `createPatchFromCommitLayouts`, `blameCases`, `patchAlreadyAppliedEmptyGarbage`. `commitFromLockedWorkingCopy` is part of `openLockedWorkingCopy`; `resetToCommitIsUnsupported` is part of `gitOnlyOperationsAreUnsupported`; `abortMergeKeepsUnversionedFiles` is part of `abortMergeAfterConflictedOperation`.
  - New hook: `SvnRepo::Testing::leaveStaleWorkingCopyLock` writes the `WC_LOCK` row an interrupted operation leaves in `wc.db`. `cloneRepository` now uses the test configuration directory too: it replaces the auth baton, so it used to skip it.
  - The changed-paths oracle is `changedPaths` (`svn log -v --xml`), not `svnlook changed`, because svnlook writes paths in the console's code page. A replacement shows as `R`.
  - Not covered yet: `ignoreOutsideWorkingCopyTerminates` is skipped, because B10 would hang the run and a thread left running crashes it on exit. Also missing: a 1.6-format working copy fixture, `cloneProgressIdsAreDistinct` (needs the credential events; moved to §6), and `resolveTreeConflictRestoresOnFailure` (no reliable way to make the resolution fail halfway). `patchRoundTrips` skips non-ASCII names for `svn diff` patches, because the CLI writes paths in the console's code page.
  - Runtime: about 3.7 min in the container and 6.4 min on the desktop, above the 3-minute budget in §8. Most of it is a new repository per test and the CLI oracle calls.
- **Done (§8 step 4, 2026-10-22):** §3.8–3.12 in `tst_svnrepo.cpp`: 50 more test functions. The whole suite is 170 functions, 472 passing rows, 255 `QEXPECT_FAIL`s and 3 skips; `ctest -R svnrepo` passes in the build container. Section 7.5 has the results and the new defects B50–B58.
  - Names that differ from the plan: `historyAfterBranchDeletedOrRenamed` (one `_data` test), `createBranchFromCommitRevisions`, `branchesFromSubdirectoryWc` (covers `checkoutBranchFromSubdirectoryWc` and `tagFromSubdirectoryWc`), `cherryPickCases`, `cherryPickWithLocalChangeInTheWayFails`, `revertCommitCases`, `applyStashCases`, `stashTwiceInARow`. Already covered by step 3 tests: `mergeKeepsUnrelatedLocalChanges` and `cherryPickKeepsUnrelatedLocalChanges` (`unstagedChangesAreNeverCommitted`), `mergeConflictAndAbort`, `cherryPickConflict` and `revertConflict` (`abortMergeAfterConflictedOperation`), and `mergeConflictResolveAndCommit` (`mergeInProgressState`).
  - Not covered yet: a cherry-pick of a revision that touches two branches (B10h; the source is hash-ordered, so the test would be flaky until it is fixed), a cherry-pick of a merge revision, `mergeNoTrunkLayout`, the failure path of `renameActiveBranch` (B18h), `deleteAllStashesPartialFailure` (no way to make one deletion fail), and two stashes in the same millisecond (B19h; the test can't control the clock).
  - The helpers set `SVN_EXPERIMENTAL_COMMANDS=shelf3`, so the 1.14 CLI's `x-shelve` works for `stashCliInterop`.
  - `svn:author` for `file://` is the user name cached in the configuration area once the CLI has committed with `--username`, otherwise the logged-in user. `authorOfPickedRevision` compares it with another commit of the same working copy instead of a fixed name.
  - Runtime: about 5 min in the container and 9.6 min on the desktop. Splitting §3 into several executables, so ctest can run them in parallel, is the obvious way back under the budget.
- **Done (§8 step 5, 2026-10-22):** §3.13–3.15 in `tst_svnrepo.cpp` (18 more test functions, 188 in all) and §4 in the new `svnsync_test` (`tst_svnsync.cpp`, 7 test functions, own `add_test`). `ctest -j 2 -R "^svn(repo|sync)$"` passes in the build container: svnsync in 20 s, svnrepo in 5.7 min. Section 7.5 has the results and the new defects B59–B64.
  - Names that differ from the plan: `addSubmoduleCases` (`addSubmodule`), `gitOnlyOperationsTable` (the Git-only functions `gitOnlyOperationsAreUnsupported` doesn't call). `listRemotesSingleOrigin` is part of `remotes`, and `remoteBranchesWithStatusBehindAhead` is `remoteBranchesWithStatus` in tst_svnsync.
  - `workflowTypeIsConsistent` documents the behaviour: `WorkflowType` has no "none", so `getWorkflowType` reports its default, GitFlow, and every flow action is unsupported.
  - Not covered yet: `externalsCrlfPropertyValue` (the CLI refuses CR in an `svn:` property, so there is no way to make one), `incompleteUpdate` (needs an update interrupted halfway; `pull` takes no quit flag), and the "externals follow" case of `updateRemoteRelocates`, which is part of `relocateAfterRepositoryMove` instead.
- **Done (§8 step 6, 2026-10-22):** §5 in the new `svnrepomanager_test` (`tst_svnrepomanager.cpp`, 22 test functions; it also links SvnRepoManager, ComponentManager and GitRepoManager, whose `GitRepoResult` carries the clone progress). M1, M2, M3, M5 and M6 were confirmed, so the Git manager's lifecycle fixes are ported to `SvnRepoManagerProcessingState.cpp`:
  - **M1:** `SvnRepoContext` has `ownerSyncUuid` and `closeSyncUuid`. Only the answer to the request that made a context, or to its close, ends it; a refused open, clone or init leaves the live context alone.
  - **M2:** the component subscribes to `CloseRepoResult`, so a closed container can be opened again.
  - **M3:** every scheduler refuses a request once the container is closing. `workersThreadFunc` answers a command that needs a working copy without running it when there is none (`errorEventFor`). Before the fix, a DiscardFiles sent after a Close reverted a file in the process's current directory.
  - **M4:** `context()` uses `value()` (not reproduced as a crash, but it inserted null entries).
  - **M5:** Status sends an error when it meets the quit flag.
  - **M6:** requests ended by the close say "The repository was closed.".
  - `svnrepomanager` passes under ctest in the container (73 s; svnrepo, svnsync and svnrepomanager run with `-j 3`). The application builds.
  - Not covered yet: `closeDuringClone` (a `file://` checkout finishes before a close can overtake it; needs svnserve, §6), `getSubmodulePathErrorIsAnswered` (M7: `SvnRepo::getSubmodulePath` never returns empty, so that path can't be reached), `listSubmodulesWithStatusIsCancellable` (M13). `noAmendRequest` holds by construction, because the Amend events are gone (§8 step 1).
- **Done (§8 step 7, 2026-10-22):** §6 in the new `svnauth_test` (`tst_svnauth.cpp`, 13 test functions, with `SvnServe.h`, `TlsHandshakeServer.h` and the Git suite's `FakeCredentialsOverlay.h`).
  - **B20 is fixed:** `sslServerTrustPromptCallback` now applies the Git policy. On Windows a chain the system trusts is accepted by svn's Windows certificate-store provider, now registered before the prompt. Any failure other than an unknown issuer (expired, not yet valid, another host, other) is rejected with a message naming the host and the reasons. A certificate whose only failure is its unknown issuer goes to the new `GitRepo::checkUnverifiedServerCertificate`: it is trusted on first use and pinned per host when it is self-signed, with the same pins as Git, so the Credentials manager's "trust the new one" works for both. Everything else is rejected. Nothing is stored in Subversion's own auth area.
  - Confirmed: B21, B27, and the new B65–B67 (§7.5). `credentialsKeyedByPathSpelling` passes for every spelling: GitCredentialStorage normalises the path.
  - svnserve is TortoiseSVN's (the CLI tools' folder); the suite skips the svn:// tests without it. `clientCertificateProviders` is documentation only: SvnRepo registers no client-certificate provider, so servers that require one can't be used.
- **Done (§8 step 8, 2026-10-22):** everything builds and runs in Debug and in Release in the build container. `ctest -j 3` runs 12 tests (svnrepo, svnsync, svnrepomanager, svnauth, the six Git suites, workflows, commit_graph_builder), and all pass in both configurations. svnrepo has 534 passing rows in Debug, svnsync 9, svnrepomanager 101, svnauth 25.
  - **Debug needed three fixes.** The test targets copy the zlib DLL next to them (`zd.dll` stays in `GitRepo/zlib`, so no Qt test started). libsvn's plain C asserts, which Release compiles out, aborted the run twice:
    - A lowercase drive letter reached libsvn as a non-canonical path (`stageUnstageRoundTrip(lowercase drive)`). `normalizedPath` and `absoluteFilePath` now upper-case it.
    - **B2h is fixed:** branch and tag names are percent-encoded in URLs, in libsvn's canonical form (`encodedUrlPath`), and decoded when read back from a URL. Names with spaces, `é`, `日本`, `%`, `#` and `%2` now switch, rename and delete in both configurations.
  - **Fixed, with a test:**
    - **B10:** `ignoreFiles` refuses a path outside the working copy with `InvalidArgument`, and the walk up the parents stops at a root. `ignoreOutsideWorkingCopyTerminates` runs now; it was skipped.
    - **B10h:** when a revision changed several branches, a cherry-pick takes the first one by name other than the working copy's own branch, and a revert takes the working copy's own branch (`twoBranchRevision`).
    - **B25h:** only a missing path (`SVN_ERR_FS_NOT_FOUND`, `SVN_ERR_FS_NOT_DIRECTORY`, `SVN_ERR_RA_ILLEGAL_URL`) counts as "no branches" or "no trunk". A server that can't be reached is an error (`serverGoneIsAnError` in svnauth).
  - **Fixed by inspection** (no test reaches them):
    - **M7:** a missing context is reported on `GetSubmodulePathError`.
    - **M11:** the InitRepo subscription is removed.
    - **M13:** ListSubmodulesWithStatus uses the quit flag and answers "The repository was closed.".
    - **M14:** `eventName` has Blame and ListRemoteTags.
    - **M15:** the Svn bridge's Close carries a sync uuid, so the manager frees the context. The Git bridge has the same gap.
  - **Not reproduced:** B18h (`renameActiveBranch` passes, and the switch can't be made to fail).
  - **Left open:** M17. No code sends `SvnRepoManagerGetCredentials*`, because SvnRepo prompts through the Git credential events, so MainWindow's handler and the overlay's `setIsSvn` path are dead. Removing them is a GUI cleanup. Every other confirmed defect is still carried as a `QEXPECT_FAIL` (§7.5). That includes the merge, cherry-pick and revert operation state (§3.11.1), which nothing implements yet.
  - **ctest output:** no QtTest binary in this build prints its report when its output is redirected; only `-o <file>` works. Each svn test therefore also writes `<build>/tests/svnrepo/<name>.txt`. `runTool` logs every failed CLI call with the tool's message, also when the caller passes `ok`.
  - **Intermittent:** in parallel runs, a CLI call failed once each in Debug (a checkout in `stageConflictedFile(tree)`, a commit in `closeDuringLongHistory`), and gitrepomanager failed once in Release (no report, see above). None of them came back in three repeats under load or in the final Debug and Release runs. `secondWc` now checks out a second time after a failure. The next failure's log will show the CLI's message.
- **Done (defect fixes after §8, 2026-10-22):** every defect a test had carried as a `QEXPECT_FAIL` is fixed, except three limitations of Subversion and Windows (below). svnrepo has 533 passing rows, svnsync 9, svnrepomanager 101, svnauth 25. Debug and Release both pass, and the suites run under ctest.
  - **Status, staging, commit.**
    - Folders with a property change are changes of their own. The root is listed as `.` (B3).
    - Folders are staged in a file in the administrative area (`sourcerer-staged-directories`), because changelists only hold files. That covers empty and copied folders (B7) and property changes.
    - Staging a folder stages the new files in it (B37).
    - Staging refuses:
      - text conflicts that still have markers, and property conflicts (B22);
      - ignored files (B33);
      - missing paths and paths outside the working copy (B34).
    - Unstaging skips unversioned paths (B38).
    - Status reports ignored files (B18), sorts every list (B30), and doesn't enter junctions (B32) or externals (B36).
    - Log messages get LF line endings (B5), and a summary starting with `#` survives cleanup (B35).
    - A status error is no longer "Nothing is staged" (B14).
    - A tag can't be committed to (B52).
    - A staged folder's property change is brought to HEAD (that folder only) before the commit, because Subversion only commits it there.
  - **Discard and ignore.**
    - Discard keeps added and unversioned files on disk and un-adds them (B28).
    - It refuses the root (B39).
    - It restores a file of a deleted folder and keeps the rest deleted (B41).
    - A folder listed for its property change loses only that change.
    - Ignore patterns are escaped (B17). A recursive ignore puts the file's own name into `svn:global-ignores` of the nearest versioned folder (B16, §7.4 item 7).
  - **Merge, cherry-pick, revert (§3.11.1 implemented).**
    - There is a dry run with the data-loss guard (`LocalChangesInTheWay`), and only the operation's paths come to HEAD.
    - Exactly the operation's paths, plus the root's `svn:mergeinfo`, are committed in one revision (B4h).
    - Conflicts stop the operation with `…FailedWithConflicts` and the state file `sourcerer/operation.json` in the admin area. Nothing to do gives `…IsEmpty` (B17h).
    - `commitStaged` ends the operation once its conflicts are resolved, and `abortMerge` only aborts an operation (B9).
    - `pull`, `checkoutCommit` and `checkoutBranch` report conflicts (B24) and refuse while an operation is in progress, as `stash` does.
    - The merge message quotes the destination (B56).
    - A merge into another branch carries local changes there, like `git checkout` (§7.4 item 11).
  - **Branches, tags, history.**
    - Names are validated (B13). Nested names are refused (B7h, §7.4 item 10).
    - Trunk and the empty name can't be deleted or renamed (B5h, B54, B57). The folder the working copy is in can't be deleted.
    - Outside trunk, branches and tags there is no active branch (B11h).
    - Working copies of a subfolder switch, branch and merge that subfolder (B6h).
    - `listRemoteBranchesWithStatus` lists trunk (B55). The user's own commits don't count as Behind (B12h), and a fresh commit is HEAD (B13h).
    - Tags point at their source's last change, follow renames, and work in project layouts (B14h, B15h).
    - History falls back to the working copy's revision when its branch is gone at HEAD (B16h).
    - Commit diffs and file sizes follow copies (B50).
    - Commit file lists are sorted and leave out folders (B8h, B51).
    - Unknown revisions are `InvalidRevision`, and "+2" and numbers past 32 bits are refused (B24).
  - **Diff, patch, blame, conflicts.**
    - `diffFiles` text has `---`/`+++` headers and `/dev/null` for new and deleted files, and keeps "\ No newline" markers (B22h).
    - A missing file diffs as deleted (B49), and BASE has expanded keywords (B44).
    - r0 is refused (B47), and a folder's size is an error (B48).
    - A patch that changes nothing fails (B8).
    - Patches keep Latin-1 bytes for files that aren't UTF-8, take `svn diff --git` paths, and parse " b/" in names (B25). Paths in patches are UTF-8: `svn_utf_initialize2(TRUE)` (B42).
    - `.svnpatch.rej` files are listed as new (B40).
    - `listConflictsForAFile` is empty without a text conflict (B43), and `blame` refuses an unknown commit (B45).
    - Taking the incoming side of a tree conflict brings it in and leaves no `.local` copy (B46).
    - Opening a subfolder opens the working copy root (B25).
    - Cancelled status and history say Cancelled (B26). Stash indexes out of range are `InvalidArgument` (B26h).
    - A stash doesn't overwrite an unversioned file (B58) and keeps line endings out of its diffs (B20h). Its added folders go with it and come back (B19h).
  - **Lifecycle, remotes, externals.**
    - Clone and relocate URLs are canonical (B11).
    - `initRepository` refuses names that leave the parent folder and Git repositories, records the author, and leaves nothing behind on failure (B23).
    - Adding an external checks out only that external (B24h), quotes names with spaces, and defines it in the nearest versioned folder (B63). Deleting one refuses any folder but its own (B24h).
    - An external with local changes or a newer upstream is Outdated (B59). `getUpstream` only answers for existing branches (B60). `readGitFlowNextConfig` sets an error (B61).
    - Relocating moves the externals of the same repository along (B62). It also releases the lock the relocation leaves behind (B64).
  - **Credentials and certificates.**
    - Stored credentials are tried once (B21) and expire (B66).
    - An RA-wrapped cancel is Cancelled (B27).
    - A refused answer is reported invalid before the next prompt. The prompt is closed, and the answer remembered, only once the server accepts it, through a small auth provider whose save hook marks success (B65).
    - A certificate for a server addressed by IP is matched by its IP SANs (B67, with the new `GitRepo::certificateMatchesHost`).
  - **SvnRepoManager.**
    - Empty query results are results (M8, with `SvnErrorContext::serial` to tell them from errors).
    - Error codes are Subversion's, with Subversion's text (M9, `SvnRepo::errorCodeText`).
    - A failed open forwards SvnRepo's error (M10).
    - The messages are about the working copy, not Git (M12).
    - A failed merge is an error (M18).
  - **Still open, as `QEXPECT_FAIL`s:**
    - B19h for copies: shelves can't hold them.
    - B21h: the 1.14 CLI's shelf format isn't read.
    - B31: a commit through a junction fails on a Windows 11 desktop.
    - M17: dead GUI code.
  - **Test fixes:**
    - `relocateAfterRepositoryMove` and `statusExternals` used an external of trunk inside trunk, which includes itself without end: the CLI and the library overflow their stacks. They now use a separate folder.
    - Nested names moved to the refused rows.
    - `ignoreVersionedFile` and `discardCategories(file in added dir)` expect the root's change and Subversion's listing of an unversioned folder.
    - `trustPolicy(another host)` uses a certificate for another host.
    - The CLI helper retries a tool that failed to load (0xC000007B under load in the container).
- **Confirmed by running:** B4h (merge, revert and cherry-pick commit an unrelated local edit), B9 (an update conflict counts as a merge in progress, and `abortMerge` reverts the user's edit), B13h, B24 (`pull` returns true on a conflict), B28 (discard deletes added and unversioned files), B3 (the root's `svn:externals` change is not in the status), and the new B26h.
- **Running the suites:** the Windows build happens in the `sourcerer-windows-builder` container (`docker --context desktop-windows`), so `CTestTestfile` paths point at `C:\workspace`. Run ctest in that container with `C:\Program Files\TortoiseSVN\bin` mounted at the same path. For Debug, configure `build/Debug-Windows` with `-DCMAKE_BUILD_TYPE=Debug`. The QtTest reports are in `tests/svnrepo/<name>.txt` under the build folder. On the host, `svnrepo_test.exe` needs `Qt6Test.dll` 6.10.3 (not deployed to the build directory) on `PATH`.
- **Oracle caveat:** the Windows CLI tools write text with CRLF, so `runTool` turns CRLF into LF by default, and `svnlookLog` can't tell a stored CRLF from LF. For B5 and the revision property edge cases, use `svnlookRevprop`, which returns the stored bytes (`Output::Raw`).

**Library and CLI versions differ.** The vendored library is Subversion **1.16** (trunk, `lib/subversion/subversion/include/svn_version.h`). The CLI on this machine is TortoiseSVN's **1.14.5** (`svn`, `svnadmin`, `svnlook`, `svnserve` in `C:\Program Files\TortoiseSVN\bin`). Working-copy format compatibility between the two is a test subject in its own right (§3.1), and the CLI oracle must be able to read working copies the library creates.

## 2. Infrastructure (do first)

### 2.1 Files and targets
```
tests/svnrepo/
  CMakeLists.txt           svnrepo_test, svnsync_test, svnrepomanager_test, svnauth_test
  SvnTestHelpers.h         shared helpers (below)
  SvnServe.h               starts svnserve on 127.0.0.1:<free port> with a passwd file
  TlsHandshakeServer.h     QSslServer that only completes a TLS handshake (certificate prompts)
  (dump fixtures)          repositories the library can't create, built in SvnTestHelpers.h (see 2.4)
  tst_svnrepo.cpp          synchronous SvnRepo:: API on file:// repositories
  tst_svnsync.cpp          two working copies on one repository: update, out-of-date, externals, relocate (svnsync_test)
  tst_svnrepomanager.cpp   event-driven SvnRepoManager
  tst_svnauth.cpp          credential prompts (svnserve) and server-certificate policy
```
- Same CMake pattern as `tests/gitrepo`: an `addSvnRepoTest()` function, `RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}`, one `add_test` per executable, all gated by `SOURCERER_BUILD_SVNREPO_TESTS` (`src/CMakeLists.txt:119-123`).
- `svnrepomanager_test` also needs `SvnRepoManager` and `ComponentManager`, like `gitrepomanager_test`.
- SvnRepo stores credentials in `GitRepo::GitCredentialStorage`, so suites that touch credentials link GitRepo with `SOURCERER_GITREPO_TESTING` and call `useTestCredentialsKey()`. Otherwise they write a master key into the OS keychain.

### 2.2 Test hooks in SvnRepo (`SvnRepo::Testing`, `SOURCERER_SVNREPO_TESTING`)
SvnRepo has no test seams today. Add them first:
- `setConfigDirectory(path)`: today `svn_config_get_config(&ctx->config, nullptr, pool)` (`SvnRepo.cpp:445`) reads the real `%APPDATA%\Subversion`. The user's `global-ignores` (the default list includes `*.rej *~ .DS_Store`), `auto-props`, `store-passwords` and `enable-auto-props` leak into results. Tests point it at an empty directory in the temp dir; specific tests write a `config` file there.
- `sslServerTrustCheck(failures, certInfo)`: an entry point to the trust policy without a TLS server, mirroring `GitRepo::Testing::checkCertificate`.
- `installMalfunctionHandler()`: `svn_error_set_malfunction_handler` with a handler that records the assertion and returns an error instead of calling `abort()`. Without it, a non-canonical path or URL ends the whole test run with no report. **Run the suite in a Debug build as well**: libsvn also uses plain C `assert(svn_uri_is_canonical(...))` (`dirent_uri.c`), which Release builds compile out, so those failures only show in Debug.

### 2.3 Helpers (SvnTestHelpers.h)
- Reuse `writeFile`, `readFile`, `changedLines`, `makeWritable` and `ConfigOverride` from `GitTestHelpers.h` (move the generic parts to a shared header, or copy them). `makeWritable` matters here too: `.svn/pristine` files are read-only and `QTemporaryDir` can't delete them on Windows.
- `lastError()`, `lastErrorMessage()`, and `expectError(SvnError)`: asserts the call failed, the error kind matches, **and the message is not empty**.
- `resetLastError()`: called before every failure assert. `gLastError` is never reset by successful calls, so a stale error would otherwise satisfy an assert.
- `svn(args, bool *ok)` / `svnIn(dir, args, ok)` / `svnadmin(args)` / `svnlook(args)`: QProcess, 60 s timeout, always `--non-interactive --config-dir <tmp>/svnconfig`, `LC_ALL=C`, `--username Tester` where relevant. `QSKIP` in `initTestCase` when the tools are missing (the bundled CLI is optional, `SOURCERER_BUILD_SVN_CLI`, `lib/SvnRepo/CMakeLists.txt:337`).
- Oracles: `svnStatusXml(wc)` (parsed `svn status --xml -v`, including changelists), `svnInfo(target, field)`, `svnLogXml(url, rev)`, `svnPropget(name, target)`, `svnlookChanged(repo, rev)`, `svnlookAuthor/Log/Date`. Results are compared with what SvnRepo reports.
- `commitAll(summary)` through the API under test (stages everything first, **no automatic update afterwards**), and `svnCommitAll(wc, msg)` through the CLI for setup only. The existing `commitAll` runs `pull` after every commit, which hides every mixed-revision defect (§7, B12h, B13h).
- `makeRepo(QString *url, Layout)`: `svnadmin create` plus a layout (`Standard`, `ProjectRoot` = `/proj/{trunk,branches,tags}`, `Flat` = no trunk). Returns both the path and a canonical `file:///` URL.
- `secondWc(url)`: another checkout of the same repository, the svn counterpart of a second clone.
- `addPreRevpropChangeHook(repo, allow)`: writes `hooks/pre-revprop-change.bat` (Windows) or an executable `.sh`.
- `head(wc)`: `svnlook youngest`, not `getHistory`, so the oracle doesn't depend on the code under test.
- `unusualNames()`: one shared `_data` table used by every path-taking area (§3.0).

### 2.4 Fixtures
- `initTestCase`: `QSKIP` without `svn`/`svnadmin`, `QStandardPaths::setTestModeEnabled(true)`, `SvnRepo::init()`, `initForCurrentThread()`, the malfunction handler, `useTestCredentialsKey()`.
- `init`: a fresh `QTemporaryDir` (`_base = cleanPath(fromNativeSeparators(...))`), `setConfigDirectory(_base + "/svnconfig")`, then `SvnRepo::initRepository("repo", _base + "/wc", "main", kAuthor, kEmail)` and `openRepository`. This exercises the library's init the way the Git suite does.
- `cleanup`: `closeRepository`, `makeWritable(_base)`, reset `_quit`, clear the cached credentials.
- Repositories the library can't make are loaded with `svnadmin load --bypass-prop-validation` (`loadDump`). The dumps are written by `DumpBuilder` in `SvnTestHelpers.h` (`SvnTest::Dumps`), not kept as `dumps/*.dump` files: a dump records byte lengths and needs LF line endings, while the sources are kept with CRLF. `fixtureRepositories` checks every one of them:
  - a revision with no `svn:author`, one with no `svn:date`, one with an empty log (`Dumps::revpropEdgeCases`);
  - a log with CRLF line endings and one with non-UTF-8 bytes (same dump);
  - a project-root layout and a layout with no `trunk` (`projectRootLayout`, `noTrunkLayout`);
  - a revision that touches two branches at once, and one that touches only a tag (`branchesAndTags`);
  - a replaced node (`R`) and a moved directory (`replacedAndMoved`; `svnlook changed` lists a replacement as `D` and `A`, `svn log -v` as `R`);
  - revisions above 2^31 aren't loadable, so the 32-bit truncation (B24) is tested through `parseRevision` inputs only.

## 3. tst_svnrepo.cpp: synchronous API

Each bullet is a test slot: a scenario with many asserts, including its own error path. **[data]** marks `_data` tests. Every failure assert checks `getLastError().error` against the exact `SvnError`, checks the message is not empty, and checks that nothing changed (the oracle's status, `svnlook youngest`). §7 lists the suspected defect each test targets (B1, B2, …).

### 3.0 Shared edge-case matrix
`unusualNames()` rows, used by status, stage, discard, ignore, commit, diff, blame, history, branches, tags, stash and externals:
- spaces: `a b.txt`, `dir with space/x.txt`
- Latin and CJK unicode, emoji: `ünï.txt`, `日本.txt`, `😀.txt`; NFC vs NFD spellings of the same name
- URL-sensitive characters: `#hash.txt`, `100%.txt`, `a?b` (not on Windows), `a+b.txt`, `x,y.txt`, `a;b.txt`, `a=b.txt`, `it's.txt`, `(1).txt`, `a!.txt`, `$x.txt`, `a&b.txt`
- peg-revision trap: `a@b.txt`, `file@` (trailing `@`), `@leading.txt`
- glob characters: `file[1].txt`, `a*b.txt` (not on Windows)
- leading dash: `-x.txt`; leading dot: `.hidden`
- names that collide with layout words: `trunk`, `branches`, `tags` as ordinary directories inside trunk
- a deep path near or beyond MAX_PATH (`\\?\` writes; `QEXPECT_FAIL` only for a confirmed libsvn limit)
- a case-only pair `Foo.txt` / `foo.txt` (Windows: case-only rename)

Path spellings, where a function takes paths **[data]**: relative, `./x/../x`, backslashes, absolute with forward slashes, absolute native, lowercase drive letter `c:/...`, trailing slash, a path through a junction or symlink to the WC.

### 3.1 Repository lifecycle
- `initCreatesStandardLayout`: the repository has `trunk`, `branches`, `tags` (oracle: `svn ls`); the WC is at trunk and clean; check where the repository directory was created (`<parent>/<repoName>-svnrepo`, `SvnRepo.cpp:1465`) and record it as the contract.
- `initIgnoresOrHonoursDefaultBranch` **[data]** `main`, `trunk`, `feature/x`, `ветка`: document that `defaultBranch` is ignored, or fix it (B23).
- `initAuthorIsRecorded`: the layout revision's `svn:author` (oracle `svnlook author`); today it is the OS user (B23).
- `initRefusesExistingWorkingCopy` and `initIntoGitRepository`: the second init fails with an error and **no repository or r1 stays behind**; a retry after fixing the problem works (B23).
- `initUnusualRepoNames` **[data]** spaces, unicode, `../escape`, `a/b`, `a\b`, empty, a drive root as `workingDir`: unsafe names are refused and nothing is created outside the parent directory (B23).
- `initIntoUnusualPaths` **[data]** §3.0 names for `workingDir`.
- `openRejectsNonWorkingCopy` **[data]** empty dir, missing path, a file, empty path, an unversioned directory inside a WC: `ErrorWhileOpening` with a message.
- `openFromSubdirectory`: open `wc/dir`. Document whether it walks up to the WC root; if not, every later relative path is shifted. Status, stage and commit from a subdirectory-opened repository must still report root-relative paths or refuse to open (B25).
- `openPathSpellings` **[data]** §3.0 spellings open the same WC, and later calls with a different spelling of the same path still match (credentials keyed by path, current-repository bookkeeping).
- `openOldAndNewFormats`: a 1.6-format WC (from a zipped fixture) needs an upgrade; a WC made by the 1.14 CLI opens; a WC made by the library is readable by the 1.14 CLI (`svn status` succeeds). Errors carry the svn code.
- `openLockedWorkingCopy`: `wc.db` with a stale lock (made by the CLI with an interrupted operation, or `svn lock`-free WC lock via `sqlite`): status reports a clear error; `cleanup` is not run silently.
- `closeRepositoryOfOtherPath`: `closeRepository(path)` ignores its argument and clears the thread's current repository (`SvnRepo.cpp:1366`). Open A, open B, close A, then B still works.
- `cloneUrlSpellings` **[data]** trailing `/`, `FILE:///`, `file://localhost/`, unescaped space, escaped `%20`, a plain local path, a backslash path, a URL with `@` in the path: canonicalised or refused with `CheckoutFailed`, never a malfunction (B11).
- `cloneIntoExistingDestinations` **[data]** non-empty dir, existing WC of another URL, missing nested parent.
- `cloneFailureCleansUp`: a bad URL leaves no directory behind, including parents it created.
- `cloneRepositoryRoot`: cloning the root checks out every branch and tag. Document what `getActiveBranch` says there (B11h).
- `cloneProgressIdsAreDistinct`: two clones with different uuids get different progress ids; the default empty uuid currently hashes to the same id for all (`SvnRepo.cpp:1426`).
- `lastErrorIsPerThread`: an error on a worker thread is not seen on the test thread.
- `concurrentCallsOnTwoThreads`: status and history on two WCs from two threads, no crash, correct results.
- `successfulCallDoesNotLeaveStaleError`: a failing call, then a succeeding one; `getLastError()` should be reset or documented as unspecified. Functions returning values (`getStatus`, `hasConflicts`) can't otherwise be told apart from failures.

### 3.2 Status
- `statusCategories`: one test that creates every state and compares with `svn status --xml`: unversioned, added, modified, deleted, missing (`!`), replaced (`R`), obstructed (`~`, file replaced by a directory), conflicted (text, property, tree), copied with history, moved (`moved_from`/`moved_to`), ignored, external (`X`), incomplete (interrupted update), locked, switched subtree; each staged and unstaged where that makes sense.
  - Pins: moves show as add + delete (B15); `replaced` maps to typeChanged; obstructed counts as typeChanged even when staged; missing is never staged; `incomplete`/`external` are dropped.
- `statusIgnoredFilesAreReported`: a file matching `svn:ignore` appears in `ignoredFiles`. Today the list is never filled (`no_ignore=FALSE`, `SvnRepo.cpp:1148`) (B18).
- `statusDirectoryPropertyChanges`: `svn propset` on a directory and on the WC root. The change must be visible and committable (B3).
- `statusUnversionedDirectories`: nested files reported as files; an empty unversioned directory; a directory containing only ignored files; a nested WC; a symlink or junction to a directory.
- `statusExternals`: changes inside an external, and an external pointing at a missing URL.
- `statusUserConfigGlobalIgnores`: with a `config` file setting `global-ignores = *.tmp`, a `.tmp` file is ignored; with the empty config dir it is not. Proves isolation works.
- `statusUnusualFileNames` **[data]** §3.0.
- `statusMixedLineEndings` **[data]** `svn:eol-style` native/LF/CRLF/none with LF and CRLF content: what counts as modified matches the oracle.
- `statusKeywords`: `svn:keywords Id Rev` — an expanded file is not reported as modified.
- `statusNeedsLock`: a read-only `svn:needs-lock` file; a file locked by another WC.
- `statusOtherChangelist`: a file in a user changelist `mine` shows as unstaged and keeps its changelist.
- `statusSorted`: output order is deterministic and sorted.
- `statusCancelled`: `_quit` set before the call returns empty with `Cancelled`; set from another thread during a 3,000-file status, it returns promptly.
- `statusManyFiles`: 3,000 files under a time budget.
- `statusFailureIsNotClean`: status on a locked or deleted WC fails with an error. Today it returns an empty `SvnStatus` that callers read as clean (`SvnRepo.cpp:1168`).

### 3.3 Staging (changelist `sourcerer-staged`)
The rule every test in §3.3, §3.4 and §3.11 checks: **only staged files reach a revision**. After each commit the oracle (`svnlook changed`) lists exactly the staged paths (plus the parent directories they need), and every unstaged change is still in the working copy, unchanged.
- `unstagedChangesAreNeverCommitted` **[data]** one row per operation that creates a revision: `commitStaged`, `merge`, `cherryPickCommit`, `revertCommit`, a commit after a resolved conflict. Each starts with an unstaged edit, an unstaged new file, an unstaged deletion and an unstaged property change, all on files the operation doesn't touch.
- `stageUnstageRoundTrip` **[data]** §3.0 spellings: new → staged → unstaged → staged; the oracle shows the changelist.
- `stageModifiedDeletedMissing`: a modified file joins the changelist; a missing file is scheduled for delete and staged; a deleted file stays staged.
- `stageDirectory`: staging a directory stages every changed file below it and nothing else. Document that unmodified files in it join the changelist, so their future edits appear staged.
- `stageEmptyAddedDirectory` and `stageCopiedDirectory`: today they can never become staged, so `commitStaged` says "Nothing is staged" (B7).
- `stageInsideUnversionedParents`: `dir/sub/b.txt` adds the parents but only the file is staged; unstaging leaves the parents added.
- `stageIgnoredFile`: refused with an error, or added with `--no-ignore`. Today it returns true and does nothing.
- `stageConflictedFile` **[data]** text conflict with markers still in the file, property conflict, tree conflict: staging must not silently resolve a file that still has `<<<<<<<` (B22).
- `stageMovesFileFromUserChangelist`: a file in `mine` is moved into the staging changelist. Document or refuse (B22).
- `stageNothing`: an empty list is a no-op returning true, or an `InvalidArgument` error; same for `unstageFiles({})`.
- `stageNonexistentAndOutsidePaths` **[data]** missing file, `../outside`, an absolute path in another WC: `AddFailed` or `InvalidArgument`, nothing changed.
- `stageFileInsideIgnoredDirectory`.
- `unstageKeepsAddAndDelete`: unstaging an added file leaves it added (status: new, unstaged); a deleted file stays deleted.
- `unstageUnversionedOrUnstagedFile`: no error or a clear one; never a raw `WC_PATH_NOT_FOUND`.

### 3.4 Commit, amend, sign-off
- `commitStagedOnly`: only changelist members are committed; other modifications stay modified; the changelist is cleared on committed files (oracle: `svnlook changed`).
- `commitStagedDeletedDirectory`: delete `dir` with children, stage, commit. Expected to fail today with "non-recursive commit because it has children" (depth empty, `SvnRepoWorkingCopy.cpp:574-615`) (**B1, critical**).
- `commitStagedReplacedDirectory` and `commitStagedAddedTree` (added directories two levels deep with files).
- `commitStagedMoveBothSides` and `commitStagedMoveOneSide`: `svn mv` by the CLI; staging only one side must give a clear error or stage both sides (B15).
- `commitWithoutStagedFiles`: error, nothing committed.
- `commitStagedFileRevertedExternally`: staged, then reverted by the CLI; the commit must not report success with no revision.
- `commitMessageComposition` **[data]** summary only; summary + body; multi-line body; trailing whitespace; unicode; `#` lines with cleanup on and off; `#42 fixes` as the summary; very long summary; empty or whitespace-only summary; CRLF in the summary; CRLF in the body; a lone `\r` mid-line. Compare with `svnlook log` byte for byte. CRLF is expected to be rejected by the repository today (B5).
- `commitSignoff`: exactly one `Signed-off-by`, no duplicate when the body already has it; with an empty author, document the behaviour.
- `commitAuthor`: what `svn:author` ends up being for `file://` (OS user) and for `svn://` (auth user). The `author`/`email` arguments only reach the sign-off; document or set `svn:author`.
- `commitOutOfDate`: another WC commits the same file first; the commit fails with `CommitFailed` and the svn out-of-date code, and the changelist survives for a retry.
- `commitNeedsLockAndForeignLock`.
- `commitFromLockedWorkingCopy`: the real error is reported, not "Nothing is staged" (B14).
- `commitDoesNotIncludeUnstagedPropertyChanges`.
- `commitSignCommitIsReported`: `signCommit=true` is refused with `UnsupportedOperation` or ignored with a warning; document which.
- Amend does not exist for Subversion (§7.4). `amend` joins the Git-only functions in §3.15 (`UnsupportedOperation`, no revision property is changed, `svnlook log` is unchanged).

### 3.5 Discard, ignore, reset, checkout, patch
Discard behaves like `GitRepo::discardFiles` (§7.4): a file in BASE goes back to BASE (content, properties, a deleted or missing file comes back); an added file, staged or not, is only un-added and **stays on disk as unversioned**, like `discardStagedNewFileKeepsIt` in the Git suite. GitRepo leaves untracked files on disk too (removing the index entry of a file that has none does nothing), but no Git test pins that; add `discardUntrackedFileKeepsIt` to tst_gitrepo.cpp so both backends are held to the same rule.
- `discardCategories` **[data]** modified, added (stays on disk, becomes unversioned; today deleted, `added_keep_local=FALSE`), added and staged (same), unversioned file and unversioned directory with ignored content (left on disk, like Git; today deleted), deleted, missing, property-only, replaced (back to the BASE node), a file in an added directory (un-added, stays on disk, the added parents become unversioned when nothing in them is added any more): compare with the oracle and with what is left on disk, and check the file leaves the staging changelist.
- `discardStagedChanges`: a staged edit with more unstaged edits on top goes back to BASE, as in Git.
- `discardDeletedChildOfDeletedDirectory`.
- `discardOneSideOfMove`.
- `discardTreeConflict`: today skipped and returns true (B19).
- `discardRootAndOutsidePaths` **[data]** `.`, empty string, `..`, an absolute path outside the WC: refused, nothing deleted (the Git suite found an empty path deleting the CWD).
- `discardPartialFailureLeavesConsistentState`: one bad path in the list; unversioned files must not already be deleted when the revert fails.
- `ignoreFileNonRecursive`: appends to the parent's `svn:ignore`; status shows the file as ignored; the property change is visible and committable (B3).
- `ignoreRecursive` **[data]** `dir/x.o`, `x.o` at the root (today a no-op that returns true, B16), §3.0 names: the escaped name is added to the parent's `svn:global-ignores` (§7.4). Files with that name at any depth below are ignored, and other new files below stay visible. Today the code writes `*`, which ignores everything below.
- `ignoreGlobCharacters` **[data]** `file[1].txt`, `a*b`, `a?b`: the written pattern matches only that file (B17).
- `ignoreExistingPropertyValues`: an existing value with CRLF, trailing spaces, duplicates; no duplicate is added and existing entries survive.
- `ignoreInsideUnversionedTree`: the topmost unversioned directory is ignored instead; document.
- `ignoreOutsideWorkingCopyTerminates`: an absolute path outside the WC and `C:/`. Must return, not loop (`SvnRepoWorkingCopy.cpp:447-454`) (B10). Run it with a watchdog timer.
- `ignoreVersionedFile`: an error or a documented no-op.
- `resetToCommitIsUnsupported` **[data]** Soft, Mixed, Hard, Unknown, with local edits and staged files: each returns `UnsupportedOperation` with a message (§7.4). The WC is untouched: content, changelist membership and the BASE revision are the same as before.
- `checkoutCommit` **[data]** a past revision, `r`-prefixed, above HEAD, r0, a revision before the branch existed, a non-number, `HEAD`: valid ones update the WC to that revision (oracle `svn info`), invalid ones fail with `InvalidRevision`.
- `checkoutCommitWithConflictingLocalEdit`: must report the conflict, not return plain true (B24).
- `patchRoundTrips` **[data]** a patch from `createPatchFromCommit`, from `diffFiles().diff`, from `git diff`, from `svn diff`, from `svn diff --git`: modify, add file, delete file, add/delete empty file, no newline at end, CRLF file, unicode path, path with ` b/` in it, git octal-quoted path, rename, binary git patch.
- `patchSkippedTargetFails`: a patch against a missing or unversioned file must fail. Today the skip notification is ignored and the call returns true (B8).
- `patchRejectedHunks`: fails, reports which file, and the `.svnpatch.rej` files are visible (not hidden by `*.rej` in `global-ignores`).
- `patchAlreadyApplied`, `patchEmpty`, `patchGarbage` **[data]**.
- `patchCrlfEmptyContextLine`: a CRLF patch whose context includes an empty line (`SvnRepoWorkingCopy.cpp:91`).
- `patchNonUtf8Content`: Latin-1 bytes survive.

### 3.6 Conflicts, merge state, tree conflicts
- `conflictKinds` **[data]** text, property, tree (edit vs delete, delete vs edit, add vs add, move vs edit, directory victim): `hasConflicts`, `conflictedFiles`, `isTreeConflict`, `listConflictsForAFile` match the oracle.
- `listConflictsForNonConflictedFile`: empty or an error, not the raw file contents; binary conflict; property conflict (`.prej`).
`abortMerge` behaves like `GitRepo::abortMerge` (§7.4): it fails with an error when no merge, cherry-pick or revert is in progress; otherwise it ends that operation and resets the working copy to BASE, like `git reset --hard`. Files added after the operation started are removed from disk, as `git reset --hard` does with staged new files; unversioned files stay. As in Git, the data-loss guard is on the way in: merge, cherry-pick and revert refuse to start when a local change is in a file they would change (§3.11).
- `abortMergeAfterConflictedMerge` / `AfterConflictedCherryPick` / `AfterConflictedRevert`: files back to BASE, nothing in progress, no conflicts, oracle status shows no changes, **no file left in the staging changelist** (B4), the BASE revision of paths outside the operation unchanged (§3.11.1).
- `abortMergeKeepsUnversionedFiles`.
- `abortMergeWithoutOperationFails` **[data]** a clean WC, a WC with local edits only, a conflicted **update** (`pull`): `abortMerge` fails with an error and touches nothing. An update conflict is not an operation that can be aborted; today it is treated as one and the user's own edits are reverted (B9).
- `abortMergeTwiceFails`.
- `mergeInProgressState`: `getIsMergeInProgress` is true from the conflicted merge, cherry-pick or revert until it is committed or aborted, survives closing and reopening the repository, and is false after an update conflict.
- `mergeInProgressFromCli`: after `svn merge` by the CLI (uncommitted mergeinfo, no state file) document what `getIsMergeInProgress` says.
- `resolveTreeConflict` **[data]** keepLocal true/false × file/directory victim × victim missing locally: the WC ends clean, no `.local` backup stays unversioned, and the path is not updated to HEAD with unrelated changes.
- `resolveTreeConflictRestoresOnFailure`: a failure halfway restores a renamed directory (today `QFile::remove` can't remove a directory).

### 3.7 Diff, blame, file size
- `diffCategories` **[data]** modified, added, unversioned, deleted, missing, property-only, binary (`svn:mime-type`), empty file, no newline at end (the `\ No newline` information is kept), CRLF and `svn:eol-style`, `svn:keywords` (no spurious diff between `repoContent` and `filesContent`), non-UTF-8 content, unicode content, a 10 MB file with the max-diff-size config.
- `diffUnusualFileNames` **[data]** §3.0.
- `diffOutsideWorkingCopy`: one bad file must not fail the whole call, or must fail with an error naming the file.
- `diffTextIsAValidPatch`: `diffFiles().diff` has `---`/`+++`, `/dev/null` for adds and deletes, and applies with `applyPatch` and with `git apply` (B22h).
- `diffFromCommit` **[data]** a file added, modified, deleted (today an empty delta instead of all lines removed, B9h), renamed, copied, property-only, binary in that revision; a revision before the branch existed; after the branch was renamed or deleted in the repository; r0; r1; a non-revision that isn't a shelf; `HEAD`.
- `diffFromCommitAbsolutePaths`: `getCommitStatus` file lists fed back into `diffFilesFromCommit` work for every §3.0 name.
- `createPatchFromCommit` **[data]** trunk WC, branch WC, WC at the repository root, WC at a project-root layout, a branch named `my branch`, the branch-creation revision, a revision with a rename, a binary change: the headers are `a/<path>` relative to the branch and apply with `git apply`.
- `blame` **[data]** committed, locally modified (empty commit on changed lines), locally added (per-file error), binary (`errorMessage`, valid), empty file (zero lines, not one), a commit argument, a shelf name (must not blame the WC silently), a file with CRLF, `svn:keywords`.
- `blameLineCountMatchesFile`: no extra trailing line (the libgit2 bug class).
- `fileSizeFromCommit` **[data]** a file, a directory, a missing path, a shelf file that was deleted, an invalid revision: values and error kinds.

### 3.8 History
- `historyLinearParents`: newest first; `parents` and `children` link consecutive revisions.
- `historyFollowsCopies`: a branch's history includes trunk revisions before the copy; each of them works with `diffFilesFromCommit` and `getFileSizeFromCommit`.
- `historyPaging` **[data]** page size 1, 2, exact boundary `(page+1)*size`, a page past the end, page -1, page 0 after new commits: `hasMore` correct, no duplicates or gaps.
- `historyRevpropEdgeCases` (dump fixtures): no `svn:author` → empty, no `svn:date` → -1, empty log, CRLF log (summary trimmed, message keeps or strips `\r` consistently), non-UTF-8 log.
- `historyTimestamps`: milliseconds UTC, equal to `svnlook date`.
- `historyAfterBranchDeletedOrRenamed`: the WC's branch is deleted or renamed by the CLI. History still works or fails with a clear error (HEAD peg, B16h).
- `historyOtherBranchesOnly`: a revision that only touches another branch is not in this branch's history.
- `historyCancelled`.
- `historyProjectRootLayout` and `historyRepoWithSpaceInPath`: `commitInfo.branches` filled correctly (encoded/decoded mismatch).
- `commitStatusFileLists` **[data]** add, modify, delete, replace, move, copy, a property change on the WC root, a branch creation (directories are not "files"), paths outside the WC; lists are relative and sorted (B8h).
- `commitStatusOfInvalidRevision` **[data]** above HEAD, `abc`, `-1`, `r`, empty, `+5`, `1234567` (a git-looking id), `4294967297`: `InvalidRevision` (B24).
- `commitBranchesTagsHeads`: `getCommitBranches` for a revision touching two branches (deterministic, sorted); `getCommitTags` for a tag made from HEAD after another branch changed (B14h), a renamed tag, a project-root layout (B15h); `getCommitHeads` with nested branches.
- `commitIsHeadAfterOwnCommit`: commit, then `getCommitIsHead(newRev)` **without** an update. Today false (mixed-revision root, B13h).

### 3.9 Branches
- `listBranchesLayouts` **[data]** standard, project root, no trunk, WC at the repository root, WC at `branches/` itself: `listLocalBranches`, `listRemoteBranches`, `getActiveBranch` for each. Outside trunk/branches/tags, `getActiveBranch` returns an empty name with no error (§7.4, today `trunk`, B11h). `deleteBranch`, `renameBranch`, `merge` and `createBranchFromCommit` with an empty `commit` then fail with `InvalidArgument`.
- `branchNames` **[data]** `feature/x` (nested), `my branch`, `é`, `日本`, `a@b`, `100%`, `#x`, `%2` (`.arg()` substitution, B23h), `trunk`, empty, `..`, `a\b`: create, list, checkout, active name, rename, delete. Names must round-trip exactly (today `branchNameFromUrl` returns percent-encoded names, B1h/B2h); invalid names fail with `InvalidArgument`.
- `createBranchFromCommit` **[data]** HEAD (empty commit), `r`-prefixed, r0, a future revision, a revision older than the branch, an existing name: valid ones create `branches/<name>` copied from the **branch root**, not the WC subdirectory (B6h).
- `checkoutBranchFromSubdirectoryWc`: a WC of `trunk/src` switches to `branches/X/src` (B6h).
- `checkoutBranchCases`: the current branch, a missing branch, a branch with unrelated ancestry (`svn mkdir`), conflicting local edits, a switched subtree, a tag (later commits to it are refused or warned), types All/Remote.
- `deleteBranchGuards` **[data]** `""`, `trunk`, the active branch, the active nested branch, the active branch with an encoded name: none of them may delete trunk, `/branches`, or the checked-out branch (B5h).
- `renameActiveBranch`: the WC follows; if the switch fails, the result is reported and the WC is not left on a dead URL (B18h).
- `renameOntoExistingBranch`, `renameTrunk`.
- `remoteBranchesWithStatus`: trunk is listed; after the user's own commit (no update) the active branch is not Behind (B12h); after another WC commits, Behind 1.

### 3.10 Tags
- `tagLifecycle` **[data]** §3.9 names: create from HEAD and from a revision, list, `getCommitTags`, checkout, rename, delete.
- `deleteTagGuards`: `deleteTag("")` must not delete `/tags` (B5h).
- `tagFromSubdirectoryWc`, `tagFromTagWc`.
- `tagMessage`: `#42 release` survives; author and email documented.
- `pushTagAndPushAllTags`: documented no-ops that return true, or `UnsupportedOperation`.

### 3.11 Merge, cherry-pick, revert
These behave like the Git backend (§7.4), and the tests mirror the Git suite's (`mergeCreatesMergeCommit`, `mergeWithDirtyWorkdirFails`, `mergeConflictAndAbort`, `mergeConflictResolveAndCommit`, `cherryPick`, `cherryPickConflict`, `cherryPickAlreadyApplied`, `revertCommit`, `revertConflict`):
- Clean result: exactly one new revision with the operation's changes only (and `svn:mergeinfo` for merge and cherry-pick), with Git's messages: `Merge branch '<source>' into '<destination>'`, the picked revision's own message, the caller's revert message (default `Revert "<summary>"`).
- A local change in a file the operation changes: refused before anything is touched, with its own error, local content intact, BASE revision unchanged.
- Local changes elsewhere: kept, and not committed (§3.3).
- Conflicts: returns **false** with a `…WithConflicts` error, writes the conflicts, commits nothing, leaves the operation in progress (`getIsMergeInProgress`). The user resolves, stages and calls `commitStaged`, which commits the operation's changes plus the resolution and ends it; or calls `abortMerge`.
- Nothing to do (already merged, already picked, already reverted): returns false with an "is empty" error and makes no revision.

This needs new SvnRepo pieces (designed in §3.11.1), so the tests are written against that contract and marked `QEXPECT_FAIL` until it is implemented.

#### 3.11.1 Operation state (design, §8 step 2)
The contract the §3.6 and §3.11 tests check. The error values are in `SvnRepo.h` already, and the header comments of `merge`, `cherryPickCommit`, `revertCommit`, `getIsMergeInProgress`, `abortMerge` and `commitStaged` describe this contract. Implemented after §8 (see §1); a merge into another branch carries the local changes there instead of refusing them (§7.4 item 11).

**State file.** `<wc root>/<adm dir>/sourcerer/operation.json`. The admin directory name comes from `svn_wc_get_adm_dir` (it is `_svn` under `SVN_ASP_DOT_NET_HACK`). Subversion ignores unknown files there, so the 1.14 CLI, `svn status` and `svn cleanup` leave it alone, and it survives closing and reopening the repository. It is written with `QSaveFile`, so it is never half-written.
```
{ "version": 1,
  "kind": "merge" | "cherry-pick" | "revert",
  "source": "<branch name>",        // merge: the merged branch; cherry-pick, revert: the revision's branch
  "sourceUrl": "<URL>",
  "revisions": [<start>, <end>],    // the merged range; cherry-pick r: [r-1, r]; revert r: [r, r-1]
  "destination": "<branch name>",   // the checked out branch the operation applies to
  "message": "<proposed log message>",
  "paths": ["a.txt", "dir/b.txt"],  // working-copy paths the operation changed, "." for svn:mergeinfo on the root
  "added": ["dir/new.txt"],         // paths the operation added; abort removes them from disk
  "conflicted": ["a.txt"] }
```

**Starting an operation** (`merge`, `cherryPickCommit`, `revertCommit`). Every refusal leaves the working copy exactly as it was: content, changelists, BASE revisions, no state file.
1. Validate the arguments: a revision that parses and isn't r0 (`InvalidRevision`), a known source branch (`InvalidArgument`), and no merge of a branch into itself (`InvalidArgument`, B17h). A state file that is already there means `OperationInProgress`.
2. Merge only: when `destinationBranch` isn't the checked out branch, switch to it. The switch needs a working copy without local changes (`LocalChangesInTheWay`), because Subversion would carry them to the other branch.
3. List the paths the operation will change with a dry run of the same `svn_client_merge_peg5` call (`dry_run=TRUE` changes nothing on disk). Its notifications also cover the "every unmerged revision" range of a merge, which a plain diff summary of the range doesn't. If any of them, or the root's properties for a merge or cherry-pick (`svn:mergeinfo`), has a local change, is conflicted, or is obstructed by an unversioned file, the result is `LocalChangesInTheWay` and the message names the paths. This is the data-loss guard Git has (`mergeWithDirtyWorkdirFails`).
4. Bring only those paths (and the root at depth empty) to HEAD. They have no local changes, so this update can't conflict, and everything else keeps its BASE revision. Today the code updates the whole working copy, which is a hidden `pull` (B4h).
5. Run `svn_client_merge_peg5` with `allow_mixed_rev`, and record the paths from its notifications (`paths`, `added`, `conflicted`). A cherry-pick or revert of a revision of the checked out branch ignores mergeinfo, as today; merges and cherry-picks from other branches record `svn:mergeinfo` on the root.
6. Nothing changed apart from `svn:mergeinfo`: revert `paths` and return false with `MergeIsEmpty`, `CherryPickIsEmpty` or `RevertIsEmpty`. There is no state file and no revision, like `cherryPickAlreadyApplied`.
7. Add every path in `paths` that isn't conflicted to the staging changelist. The user's own changes elsewhere stay unstaged (§7.4, item 1).
8. With conflicts: write the state file and return false with `MergeFailedWithConflicts`, `CherryPickFailedWithConflicts` or `RevertFailedWithConflicts`. The conflicted files are left unstaged, with their markers.
9. Without conflicts: commit exactly what step 7 staged, plus the root when it carries `svn:mergeinfo`, with the operation's message. That is one revision. If that commit fails (out of date, a hook refusal), keep the state file, return false with `CommitFailed`, and the operation stays in progress, so it can be committed or aborted.

**Messages.** Merge: `Merge branch '<source>' into '<destination>'`. Cherry-pick: the picked revision's message with a `(cherry picked from r<N>)` line, like `git cherry-pick -x`. Revert: the caller's message, or `Revert "<summary>"` with `This reverts revision r<N>.` Sign-off and cleanup follow `makeLogMessage`, as today.

**While an operation is in progress.**
- `getIsMergeInProgress` is true exactly while the state file is there and valid. Conflicts from an update (`pull`, `checkoutCommit`, `applyStash`) don't count (B9).
- `commitStaged` refuses while any path in `conflicted` still has a conflict, with `CommitFailed` and a message naming the files. Otherwise it commits the staged paths and the root when `paths` has `.` (the mergeinfo; this needs B3), using the caller's message, and then deletes the state file. Files the user staged that aren't the operation's are committed too, as in Git.
- `stageFiles` on a conflicted file resolves it only when the conflict markers are gone (B22).
- `merge`, `cherryPickCommit`, `revertCommit`, `checkoutBranch`, `checkoutCommit`, `pull` and `stash` fail with `OperationInProgress`. `discardFiles` works on any file, like `git checkout -- <file>`.
- A stale state file, one whose `paths` have neither local changes nor conflicts (the user committed with the CLI, or reverted everything), is deleted when it is read, and nothing is in progress.

**`abortMerge`.** Without a state file: false with `AbortMergeNothingToAbort`, and nothing is touched (B9). With one: revert `paths` and the root's properties with `clear_changelists=TRUE` (B4), remove `added` from disk (as `git reset --hard` removes staged new files), and delete the state file. Unversioned files and the user's changes outside `paths` are kept. Step 4 may have moved `paths` to a newer BASE revision, and abort doesn't move them back.

**New `SvnError` values** (appended before `Last`, so existing values keep their numbers): `MergeFailedWithConflicts`, `CherryPickFailedWithConflicts`, `RevertFailedWithConflicts`, `MergeIsEmpty`, `CherryPickIsEmpty`, `RevertIsEmpty`, `LocalChangesInTheWay`, `OperationInProgress`, `AbortMergeNothingToAbort`. They follow the Git names (`MergeFailedWithConflicts`, `CherryPickCommitIsEmpty`, `AbortMergeNothingToAbort`, …), so SvnRepoManager can translate them one to one when M9 and M18 are fixed (§8 step 6).

**Updating before applying (decided, §7.4 item 9).** Step 4 updates the operation's paths to HEAD, so merging works without a `pull` first. An abort leaves those paths at the newer revision; §3.6 checks that the BASE revision of paths **outside** the operation is unchanged.

Tests:
- `mergeCreatesMergeRevision`: one revision, message, mergeinfo (oracle `svn propget svn:mergeinfo`), status clean, nothing in progress.
- `mergeWithLocalChangeInTheWayFails` and `mergeKeepsUnrelatedLocalChanges` (B4h).
- `mergeConflictAndAbort`, `mergeConflictResolveAndCommit`: the resolution commit carries the mergeinfo and ends the merge (B17h).
- `mergeAlreadyUpToDate` (second merge of the same branch), `mergeIntoItself` (refused).
- `mergeIntoOtherBranchChecksItOut` (destination ≠ current branch), `mergeFromSubdirectoryWc`, `mergeNoTrunkLayout`.
- `cherryPick` **[data]** a normal revision, r1 (layout creation), a branch-creation revision, a merge revision (refused, like `cherryPickMergeCommitFails`), a revision touching two branches (deterministic source, B10h), a tag-only revision, a revision from the WC's own branch.
- `cherryPickConflict` (abort, then resolve and commit, then a local change in the way), `cherryPickAlreadyApplied`, `cherryPickKeepsUnrelatedLocalChanges` (B4h).
- `revertCommit` **[data]** the same rows; `revertConflict`; reverting a revision from another, never-merged branch ("is empty").
- `operationStateSurvivesReopen`: a conflicted cherry-pick, close and reopen the repository, then commit or abort.
- `authorOfPickedRevision`: `svn:author` is the committer, never the original author; document (Subversion has no separate author).

### 3.12 Stash (shelves)
- `stashCategories` **[data]** modified, added file, added directory, deleted, property-only, binary, unversioned (skipped or included — must not be silent), copied/moved, tree-conflicted: what is shelved, what stays, and that **nothing is lost**. Unshelvable changes must make the call fail or be reported (B19h).
- `stashKeepsStagingOrDocumentsLoss`: changelist membership after stash + apply.
- `stashTwiceInOneMillisecond`: two shelves, not one shelf with two versions (B19h).
- `stashOrdering`: index 0 is newest; stable when mtimes are equal.
- `applyStash` **[data]** clean WC, conflicting edit (markers, `hasConflicts`), an added file that already exists, another branch.
- `stashIndexes` **[data]** -1, out of range, after deletes: `InvalidArgument` and nothing deleted.
- `deleteAllStashesPartialFailure`.
- `stashStatus`: file lists; timestamps in ms.
- `stashDiffEolNative`: `diffFilesFromCommit(shelf)` with `svn:eol-style native` on Windows does not mark every line changed (B20h).
- `stashCliInterop`: a shelf made by the 1.14 CLI (`svn x-shelve`, v3 storage) is listed, or the incompatibility is documented (shelf2 stores v2, B21h).
- `stashNote` **[data]** empty (fails today), unicode, `#` first.

### 3.13 Remotes and synchronisation (file:// repositories)
- `listRemotesSingleOrigin`: origin is the repository root.
- `updateRemoteRelocates` **[data]** a moved repository directory; `file:///` vs `file://localhost/`; drive-letter case; trailing slash; a URL with spaces; a different repository UUID (fails); trunk instead of root (fails); externals follow.
- `remoteUnsupportedOperations` **[data]** add/remove remote, targets, `setUpstream`: `UnsupportedOperation` with a message.
- `getUpstreamUnknownBranch` **[data]** unknown, empty, a tag name.
- `pullUpdatesAndReportsConflicts`: pull with an incoming change; with a conflicting change it must report the conflict (today returns true).
- `pullMixedRevisionWc`, `pullRebaseFlag` (ignored; documented).
- `pushWithStagedWork`: push is a no-op; with unstaged or staged uncommitted work, document what the UI should hear.
- `fetchAllAndFetchRemote`: true, and do not touch the WC.
- `remoteBranchesWithStatusBehindAhead` lives in tst_svnsync (second WC).

### 3.14 Externals (submodules)
- `addSubmodule` **[data]** a plain target, a target with spaces, a URL with spaces, a peg revision `-r5`, a relative URL `^/trunk`: added, listed, checked out.
- `addSubmoduleDoesNotUpdateWholeWc`: local edits and the WC revision elsewhere are untouched.
- `addSubmoduleCommitted`: the `svn:externals` change is visible in status and committable.
- `deleteSubmoduleKeepsOthers`: `ext` and `ext2` — deleting `ext` keeps `ext2` (the Git suite's `strstr` bug class).
- `deleteSubmodulePathContainment` **[data]** `..`, an absolute path elsewhere, the WC root, empty: refused, nothing deleted (B25h).
- `externalsCrlfPropertyValue`.
- `submoduleStatus`: an external with local edits or a newer upstream revision is not UpToDate.
- `fileExternals`.

### 3.15 Git-only operations
- `gitOnlyOperationsAreUnsupported` **[data]** every function in `SvnRepoUnsupported.cpp`, plus `amend` once it moves there: returns false or empty, `UnsupportedOperation`, a message, null out-parameters are safe.
- `workflowTypeIsConsistent`: `getWorkflowType` returns `GitFlow` while every flow action is unsupported; decide.
- `readGitFlowNextConfigSetsError`: today returns false without setting an error.

## 4. tst_svnsync.cpp: two working copies

- `updateBringsOtherCommits` and `outOfDateCommitFails`.
- `remoteBranchesWithStatus`: Behind counts after another WC commits, after a branch is created there, for trunk.
- `branchDeletedElsewhere`: the WC's branch is deleted by the other WC; status, history, checkout of another branch all behave.
- `externalsFromAnotherRepository`.
- `relocateAfterRepositoryMove`.
- `incompleteUpdate`: an update cancelled through `quitFlag` (if supported) or a killed CLI update leaves `incomplete`; status and the next update report and recover.
- `cliInterop`: start with the library, finish with the CLI (`svn status`, `svn commit`, `svn update`) and the reverse, using library 1.16 and CLI 1.14 on the same WC.

## 5. tst_svnrepomanager.cpp: event-driven component

Same skeleton as `tst_gitrepomanager.cpp`: `Reply`, `newUuid`, `request()` with `startWait` + `waitForAnswer(token, syncUuid, {Result, Error})`, `REQUEST(Name, cmd)` over `EventType::SvnRepoManager##Name`. The fixture creates repositories with the SvnRepo API.

SvnRepoManager was forked from an **older** GitRepoManager and lacks its lifecycle fixes, so straight ports of the Git tests are expected to fail (§7, M1–M6).
- `requestsBeforeOpenFail`: error text says Subversion, not "Git repository" (M12).
- `initOpenCommitAndClose`: Init (value is `workingDir`, not `workingDir/repoName`), Open, Status, Stage, Commit, History, Close; a request after close fails; reopen works (M2).
- `rejectedRequestsKeepTheOpenRepository` **[data]** Open, Clone, Init on a used uuid: the open repository keeps working (M1).
- `requestsAfterCloseAreRejected`: Close, then Status and DiscardFiles with the process CWD set to a sentinel WC: an Error, and the sentinel is untouched (M3).
- `unknownUuidDoesNotPoisonTheMap`: Status with a random uuid, then Open on it succeeds, no crash (M4).
- `everyRequestIsAnswered` **[data]** one row per `SvnRepoManagerEvent` request type: a Result or Error, never a timeout, and no Error without a message (M5, M6, M7).
- `queriesWithEmptyResults` **[data]** GetCommitTags / Branches / Heads for a revision with none: an empty Result, not an Error with a stale message (M8).
- `errorsCarryTheSvnError`: CheckoutCommit with a bad revision: `gitError == InvalidRevision`, `errorCode` the real svn code (M9, M10 for Open).
- `mergeErrorIsReported`: a failed or conflicted merge sends an Error carrying the `…WithConflicts` kind (today `Result(false)` and the error is dropped, M18).
- `noAmendRequest`: once the SvnRepoManager Amend events are removed, `kRequests` has no Amend row; until then, Amend is answered with `UnsupportedOperation`.
- `gitOnlyRequestsAreUnsupported`.
- `twoContainersAreIndependent`, `requestsOnOneUuidAreFifo`.
- `closeAnswersQueuedRequests`: five Status plus Close without waiting; every one answered (M5).
- `closeDuringClone`, `closeDuringLongHistory`, `stopWithOpenContexts` (stop does not set `quit`, so a running operation is waited for; no deadlock).
- `initThenOpenImmediately`: no "already opened" race (M16).
- `cloneReportsProgress`: decodes `GitRepoResult` from `SvnRepoManagerCloneRepoProgress`.
- `applyStashResultMap` (`success`, `hasConflicts`, `conflictedFiles`) and `listConflictsReportsTreeConflict`.
- `getSubmodulePathErrorIsAnswered` (M7).
- `listSubmodulesWithStatusIsCancellable` (M13).

## 6. tst_svnauth.cpp: credentials and certificates

- `SvnServe.h` starts `svnserve -d --listen-host 127.0.0.1 --listen-port <free> -r <repo>` with `anon-access = none`, `auth-access = write` and a `passwd` file; QSKIP without `svnserve`.
- `FakeCredentialsOverlay.h` is reused as is: SvnRepo raises the same `GitRepoManagerGetCredentials*` events (`SvnRepo.cpp:195-290`).
- Tests (svn:// simple auth):
  - `rightCredentials`: one prompt, then Finished; the commit's `svn:author` is the user.
  - `wrongThenRight`: prompt, invalid, prompt, finished.
  - `wrongStoredCredentialsPromptAgain`: stored wrong credentials must lead to a prompt, not two silent failures (`SvnRepo.cpp:210-218`; `clearCachedCredentials` does nothing, B21).
  - `cancel`: fails with `Cancelled` (not the operation's own error kind) after one prompt; a cancelled checkout leaves no directory.
  - `storedCredentialsSkipPrompt`, `expiredStoredCredentials` (`ConfigOverride`).
  - `credentialsKeyedByPathSpelling` **[data]** §3.0 spellings.
  - `onePromptFinishedPerUrl`.
  - `userConfigStorePasswordsIsIgnored`: nothing written to the isolated config dir's `auth` area unless intended.
- Certificates (no DAV server is needed to reach the trust callback):
  - `trustPolicy` **[data]** through `SvnRepo::Testing::sslServerTrustCheck`: a trusted chain, self-signed (trust on first use and pin, like Git), another host, expired, not yet valid, unknown CA, a changed pinned certificate. Every failure was accepted (`SvnRepo.cpp:350-370`, B20), the same defect Git had (GIT_TEST_PLAN §6 item 5). Fixed in §8 step 7.
  - `httpsCheckoutHandshake`: `TlsHandshakeServer.h` presents a certificate on 127.0.0.1; `cloneRepository("https://127.0.0.1:<port>/svn")` must reach the callback and refuse an untrusted certificate. The request then fails because the server is not DAV; the assert is on the trust decision.
  - `clientCertificateProviders`: no client-certificate provider exists; document.

## 7. Suspected defects for tests to pin down

Found by reading the code; none confirmed by running yet. Each must first be written as a failing test (`QEXPECT_FAIL` with the defect id in the comment), then fixed and the marker removed, as was done for Git. Lines are from the audit and may drift.

### 7.1 lib/SvnRepo: working copy and core (B)
| Id | Suspected defect | Where |
|---|---|---|
| B1 | **Critical.** A staged directory deletion is committed at depth empty, which svn refuses when the directory has children. Deleted directories may never be committable. | `SvnRepoWorkingCopy.cpp:574-615`, `SvnRepo.cpp:1125` |
| B2 | The existing tests contradict the changelist staging model (§1). | `tst_svnrepo.cpp:172-176, 221, 369, 386` |
| B3 | Directory property changes (including `svn:ignore` from `ignoreFiles`, `svn:externals` from `addSubmodule`) are filtered out of status and can't be staged or committed. | `SvnRepo.cpp:549-556` |
| B4 | Hard reset and `abortMerge` revert with `clear_changelists=FALSE`, so later edits to those files show as staged. | `SvnRepoWorkingCopy.cpp:783, 824` |
| B5 | Commit messages aren't normalised to LF; svn rejects `svn:log` with CR. | `SvnRepo.cpp:1068` |
| B6 | **Superseded by §7.4:** amend is removed for Subversion. The current `amend` rewrites `svn:log` of the newest branch revision whoever made it, and can't work without a `pre-revprop-change` hook. Move it to `SvnRepoUnsupported.cpp` and remove the SvnRepoManager Amend events. | `SvnRepoWorkingCopy.cpp:623-678` |
| B7 | Copied directories and empty added directories can't be staged. | `SvnRepoWorkingCopy.cpp:287-292`, `SvnRepo.cpp:496` |
| B8 | `applyPatch` ignores skip notifications and returns true having applied nothing. | `SvnRepoWorkingCopy.cpp:35-47` |
| B9 | `abortMerge` treats any conflict, including an update conflict, as an abortable operation and reverts the user's own edits; there is no operation state to check. (Resetting to BASE during a real abort is the decided Git behaviour.) | `SvnRepoWorkingCopy.cpp:809-828` |
| B28 | Discard deletes added and unversioned files from disk (`added_keep_local=FALSE`, `removeRecursively`); Git keeps them. | `SvnRepoWorkingCopy.cpp:479-520` |
| B10 | `ignoreFiles` may loop forever for absolute paths outside the WC (`QFileInfo("C:/").path()` is `C:/`). **Fixed in §8 step 8.** | `SvnRepoWorkingCopy.cpp:447-454` |
| B11 | `cloneRepository` passes the URL uncanonicalised. | `SvnRepo.cpp:1434` |
| B12 | `urlOfRelativePath` escapes `@ + = , ; ! $ & ' ( ) * :`, which svn's canonical form keeps literal. | `SvnRepo.cpp:990` |
| B13 | `branchUrl` doesn't encode or validate names. | `SvnRepo.cpp:997-1009` |
| B14 | A status failure is masked as "Nothing is staged" in `commitStaged` and lets `amend` proceed. | `SvnRepoWorkingCopy.cpp:546, 633` |
| B15 | Moves are reported as add + delete; staging one side fails at commit. | `SvnRepo.cpp:496-507` |
| B16 | Recursive ignore of a root-level file does nothing and returns true. Recursive ignore writes `*` instead of the file's name (§7.4). | `SvnRepoWorkingCopy.cpp:418-423` |
| B29 | `resetToCommit` updates the WC (Soft/Mixed/Unknown) or reverts it (Hard) instead of returning `UnsupportedOperation` (§7.4). | `SvnRepoWorkingCopy.cpp:772-800` |
| B17 | Ignore patterns aren't escaped (`[`, `*`, `?`). | `SvnRepoWorkingCopy.cpp:425` |
| B18 | `ignoredFiles` is never filled. | `SvnRepo.cpp:1148, 643` |
| B19 | `discardFiles` skips tree conflicts whose node status is normal. | `SvnRepoWorkingCopy.cpp:502` |
| B20 | The SSL trust callback accepts every certificate failure. **Fixed in §8 step 7.** | `SvnRepo.cpp:350-370` |
| B21 | Stored wrong credentials are reused forever; `clearCachedCredentials` does nothing. | `SvnRepo.cpp:210-218, 1349` |
| B22 | `stageFiles` resolves text conflicts with markers still present, can't handle tree conflicts, and moves files out of user changelists. | `SvnRepoWorkingCopy.cpp:250-271, 290` |
| B23 | `initRepository`: no rollback, `repoName` can escape the parent, `defaultBranch`/`author`/`email` ignored. | `SvnRepo.cpp:1482-1535` |
| B24 | `parseRevision` truncates above 2^31 on Windows, accepts `+5`; `checkoutCommit` and `pull` return true on conflicts. | `SvnRepo.cpp:917-933`, `SvnRepoWorkingCopy.cpp:745-770, 927` |
| B25 | `applyPatch` CRLF counter desync, `" b/"` split, no octal unquoting, global strip count, UTF-8 re-encoding; opening a WC subdirectory shifts every relative path. | `SvnRepoWorkingCopy.cpp:91, 115, 692, 702`; `SvnRepo.cpp:1366` |
| B26 | Only status installs the quit flag; clone, update, commit can't be cancelled; `*gQuitFlag` is a data race. | `SvnRepo.cpp:429, 843` |
| B27 | A cancel wrapped by RA (credential cancel) is reported as the operation's error, not `Cancelled`. | `SvnRepo.cpp:796` |

### 7.2 lib/SvnRepo: branches, history, diff, remotes, shelves (Bh)
| Id | Suspected defect | Where |
|---|---|---|
| B1h | `branchNameFromUrl` returns percent-encoded names; comparisons with decoded list names fail, so active-branch guards miss. | `SvnRepo.cpp:1038`, `SvnRepoBranches.cpp:473, 509, 663` |
| B2h | Unencoded branch URLs (see B13) affect create, delete, rename, checkout, merge, history. **Fixed in §8 step 8.** | `SvnRepo.cpp:997-1009` |
| B3h | Commit diffs and file sizes break for names with `@ + =` (see B12). | `SvnRepoDiff.cpp:134` |
| B4h | Merge, cherry-pick and revert commit the whole WC, including unrelated local changes. | `SvnRepoBranches.cpp:140-151`, `SvnRepo.cpp:1112` |
| B5h | `deleteTag("")` deletes `/tags`; `deleteBranch("")` / `("trunk")` delete trunk. | `SvnRepoBranches.cpp:488, 609` |
| B6h | A WC of `trunk/src` is switched to the branch root; new branches and tags contain only `src`. | `SvnRepoBranches.cpp:420, 455, 589` |
| B7h | Nested branches (`feature/x`) appear as `feature`; deleting them can delete the checked-out branch. | `SvnRepo.cpp:1037`, `SvnRepoBranches.cpp:221` |
| B8h | `getCommitStatus` compares an encoded WC path with decoded changed paths, so every file becomes absolute. | `SvnRepoHistory.cpp:312-319` |
| B9h | A file deleted in a commit gives an empty delta. | `SvnRepoDiff.cpp:478-486` |
| B10h | The cherry-pick/revert source branch is the first of a hash-ordered list. **Fixed in §8 step 8.** | `SvnRepoBranches.cpp:157-159` |
| B11h | The project root or a repository without trunk reports active branch `trunk`. | `SvnRepo.cpp:1030` |
| B12h | After the user's own commit the active branch shows Behind (mixed-revision root). | `SvnRepoBranches.cpp:313-314` |
| B13h | `getCommitIsHead` is false for a fresh commit until an update. | `SvnRepoHistory.cpp:549` |
| B14h | A tag made at HEAD is attached to the repository HEAD revision, not the source's last change; renamed tags lose their link. | `SvnRepoHistory.cpp:427` |
| B15h | `getCommitTags` never matches in project-root layouts. | `SvnRepoHistory.cpp:432` |
| B16h | HEAD-pegged URLs: history and commit diffs fail after the branch is renamed or deleted. | `SvnRepoDiff.cpp:458`, `SvnRepoHistory.cpp:106` |
| B17h | A conflicted merge returns true; self-merge isn't refused; an already-merged cherry-pick returns true with no commit. | `SvnRepoBranches.cpp:141, 669-700` |
| B18h | Renaming the active branch can leave the WC on a dead URL. | `SvnRepoBranches.cpp:529-531` |
| B19h | Same-millisecond stashes share one shelf; unshelvable changes are skipped silently. | `SvnRepoShelves.cpp:162` |
| B20h | Stash diffs compare pristine `.base` with raw `.work` (EOL, keywords). | `SvnRepoDiff.cpp:154-157` |
| B21h | shelf2 (v2) can't see 1.14 CLI shelves (v3). | `SvnRepoShelves.cpp:36` |
| B22h | `diffText` has no `---`/`+++`, `/dev/null`, modes or no-newline markers. | `SvnRepoDiff.cpp:239-256` |
| B23h | Chained `.arg()` substitutes into a branch name containing `%2`. | `SvnRepoBranches.cpp:446-448` |
| B24h | `deleteSubmodule` deletes a caller-supplied path without checking it is inside the WC; `addSubmodule` can't handle spaces and updates the whole WC. | `SvnRepoRemotes.cpp:482-486` |
| B25h | `readChildren` and `getCommitHeads` treat auth and network errors as "missing directory". **Fixed in §8 step 8.** | `SvnRepoBranches.cpp:24`, `SvnRepoHistory.cpp:508` |
| B26h | **Confirmed** (`stashes`). A stash index out of range fails with `ShelveFailed` instead of `InvalidArgument` (§3.12 `stashIndexes`). | `SvnRepoShelves.cpp` |

### 7.3 component/SvnRepoManager (M)
Line numbers are in `SvnRepoManagerProcessingState.cpp` unless stated otherwise.
| Id | Suspected defect | Where |
|---|---|---|
| M1 | A refused Open/Clone/Init destroys the live context (no owner sync-uuid matching, which Git has). | 1849-1873 |
| M2 | Close never destroys the context (no `CloseRepoResult` subscription), so reopen fails. | 26-38 |
| M3 | Requests after Close run with an empty repo path, which resolves against the process CWD. | schedulers; `workersThreadFunc` 5373+ |
| M4 | `context()` uses `_contexts[uuid]`, inserting null entries; a later scheduler dereferences null. | 1375 |
| M5 | Status on quit sends no answer, so the bridge times out. | 4091-4094 |
| M6 | Quit errors have an empty message. | 1355, 2133, 2179, … |
| M7 | `getSubmodulePathWorker` reports on the request event instead of `…Error`. (Git has the same bug at its line 2921.) | 2467 |
| M8 | Empty GetCommitTags/Branches/Heads results are sent as errors with a stale message. | 2181, 2230, 4765 |
| M9 | `decodeSvnError` masks codes libgit2-style; `translateSvnError` has only a default. | `SvnRepoManagerStructs.cpp:10-33` |
| M10 | `openRepoWorker` drops SvnRepo's error (`ErrorWhileOpening`). | 3486-3491 |
| M11 | The InitRepo subscription is never removed in the destructor (Git too). | `SvnRepoManager.cpp:118-119, 188-356` |
| M12 | Messages say "Git repository …" in most places. | 2011, 2984, 4576 |
| M13 | ListSubmodulesWithStatus can't be cancelled. | 2847 |
| M14 | `eventName` has no case for Blame and ListRemoteTags. | `SvnRepoManagerFsm.cpp` |
| M15 | The bridge sends Close without a sync uuid (Git too). | `SvnRepoManagerBridgeImpl.cpp:925-937` |
| M16 | Init followed immediately by Open can hit "already opened". | InitRepoResult teardown |
| M17 | MainWindow handles `SvnRepoManagerGetCredentials` but never subscribes to it (dead code). | `MainWindow.cpp:1656, 268` |
| M18 | Merge sends `Result(bool)` even on failure and drops the error. | 2963-2970 |

Status after the defect fixes: **fixed** M1–M6 (§8 step 6), M7, M11, M13, M14, M15 (§8 step 8), M8, M9, M10, M12, M18 (after §8). **Open:** M17 (dead code, §1).

Status after §8 step 6: **fixed** M1, M2, M3, M4, M5, M6 (see §1). **Confirmed, still open** (`QEXPECT_FAIL` in tst_svnrepomanager): M8 (empty GetCommitTags, GetCommitBranches and GetCommitHeads results are errors, and the GetCommitTags one has no message), M9 (`decodeSvnError` masks the code to 16 bits; `translateSvnError` has only the default text), M10, M12, M18. **Not reproduced:** M16. **Not tested:** M7 (unreachable, see §1), M11, M13, M14, M15, M17.

### 7.4 Behaviour decisions
All settled on 2026-10-19:
1. **Staging**: the term doesn't exist in Subversion, but the feature does: the user selects the files to commit (the staging changelist). Unstaged changes are never committed together with staged ones, by any operation (§3.3).
2. **Discard**: the same as the Git backend. Files in BASE go back to BASE; added files are un-added and stay on disk as unversioned; unversioned files are left alone (§3.5).
3. **`abortMerge`**: the same as the Git backend. Fails when nothing is in progress; otherwise resets to BASE (§3.6).
4. **Merge, cherry-pick, revert**: work like the Git backend: one revision on a clean result, refuse when local changes are in the way, stop with conflicts and an operation in progress that `commitStaged` or `abortMerge` ends (§3.11). This needs an operation state in the working copy; until it exists the result must at least be staged and never committed with unrelated changes.
5. **Amend**: does not exist for Subversion. `RepoFeatures::CommitAmend` is returned only by the Git bridge, and the commit pane hides "Amend previous commit" without it (done, §1). SvnRepo's `amend` becomes `UnsupportedOperation` and the SvnRepoManager Amend events go away.

6. **`resetToCommit`**: `UnsupportedOperation` for every reset type. Subversion can't move a branch back, and the Svn bridge doesn't return `HistoryContextMenuReset`, so the GUI never offers it. `checkoutCommit` (update to a revision) stays as the way to look at an old revision.
7. **Recursive ignore**: writes the file's own name, escaped, to the parent's `svn:global-ignores`, so it matches that name at any depth below, like a `.gitignore` entry without a slash. It no longer writes `*`. For a file at the WC root, the parent is the root.
8. **`getActiveBranch` outside trunk/branches/tags** (the project root, the repository root, a repository without the standard layout): returns an empty name and sets no error. Branch operations that need an active branch fail with `InvalidArgument`.
9. **Updating before a merge, cherry-pick or revert** (decided 2026-10-22): only the operation's own paths, and the root at depth empty, are updated to HEAD before it is applied (§3.11.1 step 4). The user doesn't have to `pull` first. An abort doesn't move those paths back to their old BASE revision. The rest of the working copy keeps its revisions.
10. **Nested branch and tag names** (decided while fixing B7h, 2026-10-22): refused with `InvalidArgument`. Every folder below `branches/` or `tags/` is one branch or tag, so `feature/x` would be a folder inside the branch `feature`; listing nested names would mean walking whole branch trees.
11. **A merge into another branch** (decided while fixing B4h, 2026-10-22): switches with the local changes, like `git checkout`; a conflict with them stops the merge there. §3.11.1 step 2 had it refused (`LocalChangesInTheWay`), which `branchesSwitchAndMerge` contradicts.

### 7.5 Status after §8 step 3 (2026-10-22)
- **Confirmed**, with a `QEXPECT_FAIL` naming the id: B3, B5, B7, B8 (a patch that applies nothing reports success), B9, B11, B16, B17, B18, B22, B23, B24, B25, B26 (a status cancelled before it starts reports no error), B28; B4h, B11h, B13h, B17h, B22h, B26h; and the new B30–B49 below.
- **Not reproduced:** B1: a staged deleted or replaced folder commits fine. B8 for a missing target: the patch fails. B9h: a file deleted in a revision diffs as all lines removed. B8h: names with spaces, `ü` and `%` come back relative. B2h for `createPatchFromCommit` on a branch named `my branch`. B19 for an edit-vs-delete victim (another victim kind may still show it). B14 with a stale lock: status works and the commit reports the lock.
- **Confirmed by §8 step 4:** B13 (a name with a backslash is accepted), B26 (also for history); B2h (checking out a branch or tag whose name needs URL encoding hits a libsvn assertion), B5h (`deleteBranch("")` and `("trunk")` delete trunk), B6h, B7h (also for nested tags), B8h (the lists of a revision with several files aren't sorted), B12h, B14h, B15h, B16h, B17h, B19h (an added folder stays behind after a stash; a copy can't be stashed), B20h, B21h; and the new B50–B58.
- **Not reproduced by §8 step 4:** B1h (names with `é`, `日本`, `@`, `%2` come back decoded), B13 for names with spaces, `%` and `#` (creating, listing, renaming and deleting work; only the switch fails, B2h), B5h for `deleteTag("")` (nothing is deleted; B57 instead), B23h (a branch named `%2` round-trips). Renaming a branch onto an existing one is refused, so it isn't a defect.
- **Confirmed by §8 step 5:** B3 (an added external's `svn:externals` change), B11 (relocating to `file://localhost/…` stores that spelling; a lowercase drive letter hits a libsvn assertion), B24 (`pull`), B24h (adding an external updates the whole working copy; a target with a space fails; `deleteSubmodule` deletes any folder it is given, including the working copy itself), B16h, B55; and the new B59–B64.
- **Confirmed by §8 step 7:** B21 (stored credentials the server refuses are used again, no prompt), B27 (a cancelled prompt is reported as CheckoutFailed). **Fixed:** B20.
- **§8 step 8:** **fixed** B10, B10h, B25h, and B2h (Debug builds abort on it); B11's lowercase drive letter no longer reaches libsvn. **Not reproduced:** B18h.
- **After §8:** every other confirmed defect is fixed (§1). **Open:** B19h for copies, B21h, B31.

New defects found by the §3.1–3.7 tests:
| Id | Defect | Test |
|---|---|---|
| B30 | Status lists aren't sorted: the files of unversioned folders come after everything else. | `statusSorted` |
| B31 | A commit through a directory junction fails ("Unable to make name in …\.svn\tmp: The file exists") on a Windows 11 desktop; it works in the Server Core build container. | `openPathSpellings(junction)` |
| B32 | Status enters a directory junction inside the working copy and lists the files of its target. | `statusUnversionedDirectories` |
| B33 | Staging an ignored file returns true and does nothing. | `stageIgnoredFile` |
| B34 | `stageFiles` returns true for a missing path and stages a file of another working copy there; a path outside the working copy fails with `StatusFailed`. | `stageNonexistentAndOutsidePaths` |
| B35 | Commit message cleanup removes a summary that starts with `#` (`#42 fixes`). | `commitMessageComposition` |
| B36 | An external's folder is reported as a new file. | `statusExternals` |
| B37 | Staging a folder doesn't stage the new files in it. | `stageDirectory` |
| B38 | Unstaging an unversioned file fails with Subversion's raw "node not found" (E155010) and an sqlite trigger error. | `unstageUnversionedOrUnstagedFile` |
| B39 | `discardFiles(".")` or `discardFiles("")` reverts the whole working copy when the root has a property change. | `discardRootAndOutsidePaths` |
| B40 | The `.svnpatch.rej` files of a rejected patch are hidden by the default `global-ignores` (`*.rej`). | `patchRejectedHunks` |
| B41 | Discarding a file of a deleted folder fails ("Can't revert … without reverting parent"), and the folder's other files come back anyway. | `discardDeletedChildOfDeletedDirectory` |
| B42 | A patch from `createPatchFromCommit` doesn't apply to a non-ASCII file name. | `patchRoundTrips` |
| B43 | `listConflictsForAFile` returns the content of a file that has no conflict. | `listConflictsForNonConflictedFile` |
| B44 | `diffFiles` gives the BASE content with unexpanded keywords (`$Rev$`) and the working content expanded. | `diffCategories(keywords)` |
| B45 | `blame` with a commit that is neither a revision nor a shelf blames the working copy without a word. | `blameCases(shelf name)` |
| B46 | `resolveTreeConflict(keepLocal=false)` leaves a `.local` copy behind (edit vs delete) and leaves the file deleted (delete vs edit). | `resolveTreeConflict` |
| B47 | `diffFilesFromCommit` for r0 returns an empty valid diff and no error. | `diffFromCommit(r0)` |
| B48 | `getFileSizeFromCommit` of a folder returns -1 without an error. | `fileSizeFromCommit(folder)` |
| B49 | `diffFiles` of a missing file has no lines. | `diffCategories(missing)` |
| B50 | A branch's history includes the trunk revisions before the copy, but diffs and file sizes of those revisions are read at the branch's URL, where the file didn't exist yet. | `historyFollowsCopies` |
| B51 | `getCommitStatus` lists folders as files: the root for a property change, the new folder of a branch creation. | `commitStatusFileLists` |
| B52 | A checked out tag accepts commits. | `tagFromTagWc` |
| B54 | `renameBranch("trunk", …)` moves trunk into branches. | `renameTrunk` |
| B55 | `listRemoteBranchesWithStatus` doesn't list trunk. | `remoteBranchesWithStatus` |
| B56 | The merge message is `Merge branch 'feature' into trunk`; Git's quotes the destination too. | `mergeCreatesMergeRevision` |
| B57 | `deleteTag("")` builds a non-canonical URL and hits a libsvn assertion (nothing is deleted). | `deleteTagGuards` |
| B58 | Applying a stash over an existing unversioned file overwrites it, with no conflict and no error. | `applyStashCases(added file exists)` |
| B59 | `listSubmodulesWithStatus` reports every checked-out external as UpToDate, with local edits or a newer upstream revision. | `submoduleStatus` |
| B60 | `getUpstream` reports an upstream for any name: an unknown branch, an empty name, a tag. | `getUpstreamUnknownBranch` |
| B61 | `readGitFlowNextConfig` fails without setting an error. | `readGitFlowNextConfigSetsError` |
| B62 | After a relocation, an external that names the repository by its old absolute URL keeps that URL. | tst_svnsync `relocateAfterRepositoryMove` |
| B63 | Adding an external in a folder that isn't versioned yet fails ("not under version control"). | `addSubmoduleCases(nested target)` |
| B64 | After relocating a working copy that has externals, the next update fails because the working copy is locked, and the externals aren't updated. | tst_svnsync `relocateAfterRepositoryMove` |
| B65 | A wrong password isn't reported as invalid: the prompt is closed as "finished" after every answer, so the overlay can't tell the user the first answer was wrong. | tst_svnauth `wrongThenRight` |
| B66 | Stored credentials never expire (`GitRepoGitCredentialsExpirationTime` is ignored; the Git backend checks it). | tst_svnauth `expiredStoredCredentials` |
| B67 | serf doesn't match IP addresses in a certificate's subjectAltName, so a self-signed certificate for a server addressed by IP is rejected as issued for another host. | tst_svnauth `httpsCheckoutHandshake` |

## 8. Order of work
1. Infrastructure (§2): the `SvnRepo::Testing` hooks (config dir, trust check, malfunction handler), `SvnTestHelpers.h`, per-test fixtures, dump fixtures. Rewrite the 16 existing slots onto the new fixture and the §7.4 decisions. Move `amend` and `resetToCommit` to `SvnRepoUnsupported.cpp`, and remove the SvnRepoManager Amend events and the Svn bridge's amend request. Add `discardUntrackedFileKeepsIt` to tst_gitrepo.cpp.
2. Design the operation state for merge, cherry-pick and revert (§3.11) and the new `SvnError` values, so the §3.6 and §3.11 tests have a contract to fail against.
3. §3.1–3.7 (lifecycle, status, staging, commit, discard/ignore/patch, conflicts, diff). Highest value: data loss and commit correctness.
4. §3.8–3.12 (history, branches, tags, merge, stash).
5. §3.13–3.15 and §4 (remotes, externals, unsupported, two working copies).
6. §5 manager suite. Port the Git manager's lifecycle fixes (owner/close sync uuids, CloseRepoResult teardown, quit checks, the repository guard in `workersThreadFunc`, `value()` in `context()`, Status-on-quit answer) when M1–M6 are confirmed.
7. §6 credentials and certificate policy (port the Git pinning policy).
8. Run everything in Debug and Release; fix or `QEXPECT_FAIL` each §7 item; wire into `ctest`.

Estimated size: about 200 test slots (about 70 data-driven), roughly 600 test rows with data rows counted. Keep `svnrepo_test` under 3 minutes: `file://` operations are fast, but every QProcess oracle call costs about 50–100 ms on Windows, so batch oracle checks per test.
