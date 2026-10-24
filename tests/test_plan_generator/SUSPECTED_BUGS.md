# Suspected bugs found during test case analysis

Four agents read the source code to write the test cases in `data_combined.csv`. While they read it, they found the problems below. Nobody ran the application to confirm them, so treat each one as a lead, not as a confirmed bug. A tester confirms or rejects an item with the test case for that area.

Each item gives the location as `file:line` or as a function name, relative to the repository root. The line numbers are from the working copy on 2026-10-23.

Severity is an estimate:

- **High**: data loss, a security problem, or a stuck application.
- **Medium**: wrong behavior that the user can see.
- **Low**: wrong text, a typo, or a problem that is hard to reach.

## Summary

| Category | High | Medium | Low | Total |
|---|---|---|---|---|
| Common | 5 | 12 | 3 | 20 |
| Git | 4 | 10 | 4 | 18 |
| Subversion | 3 | 8 | 3 | 14 |
| Mercurial | 3 | 7 | 1 | 11 |
| Total | 15 | 37 | 11 | 63 |

The section "Features in the plan but not in the code" at the end lists gaps between the design documents and the code. These are not counted as bugs.

## Common

| # | Severity | Problem | Location | Details |
|---|---|---|---|---|
| C-01 | High | Updates are not verified on Windows and Linux | `component/UpdateManager/src/UpdatingState.cpp` `downloadUpdate`, `stageDownloadedUpdate` | Only a check that the file is not empty. Any downloaded file is staged and run by the launcher. Only macOS checks the publisher. |
| C-02 | High | A failed AppImage update can leave no application | `src/launcher.cpp` `runUpdate`, Linux branch | The running AppImage is deleted before the rename or copy. If both fail, the AppImage is gone. |
| C-03 | High | Sign-in tokens are accepted without a key | `lib/Util/src/JWTDecoder.cpp` `isValidJwtFormat` | When `keys/cloud_jwt_public_key.pem` is missing, only the token format is checked, so any token is accepted. |
| C-04 | High | Encryption key stored in plain text | `lib/IssueTrackers/src/TokensStorage.cpp` `saveMasterKeyToWindowsStore`, `readMasterKeyFromWindowsStore` | With no keychain, the master key for issue-tracker tokens goes in plain text into the setting `git/credentialsMasterKey`. |
| C-05 | High | Custom workflows can be lost | `component/GuiManager/src/CustomWorkflowStore.cpp` | A corrupt `gitflow-next-workflows.json` loads as an empty list. Saving a new workflow then overwrites the file and loses the old workflows. |
| C-06 | Medium | Expired issue-tracker tokens are still used | `lib/IssueTrackers/src/TokensStorage.cpp` `getToken` | `expiresAt` is never checked. |
| C-07 | Medium | Bug report form gets stuck | `component/GuiManager/src/containers/BugReportContainer.cpp` `onZipCreationFinished` | An empty log archive returns without `restoreSubmissionUi()`, so the form stays disabled, Cancel included. `docs/BUG_REPORT_IMPLEMENTATION.md` says the report is sent without logs. On an upload failure the temporary zip is not deleted. |
| C-08 | Medium | Second launch ignores its arguments | `component/GuiManager/src/MainWindow.cpp:356` `activateFromAnotherInstance` | The arguments are only logged and the window is raised. A repository path passed to the second launch is not opened. |
| C-09 | Medium | Open and Recent fail with no message | `component/GuiManager/src/MainWindow.cpp:559`, `:688` | A folder that is not a repository root, or a Recent entry whose folder was deleted, only writes a log warning. |
| C-10 | Medium | Silent exit when the data folder is read-only | `src/main.cpp:200-206` | The single-instance lock fails, and the application exits with code 1 after 10 seconds with no message. |
| C-11 | Medium | Relaunch right after quit can show no window | `component/GuiManager/src/GuiManager.cpp:183` | The activation handler is removed while the exiting process still holds the lock and listens. A launch in that window is acknowledged, but no window appears. |
| C-12 | Medium | Window jumps back when shown | `component/GuiManager/src/MainWindow.cpp:450-460` `showEvent` | The geometry saved at the last close is restored on every show, not only the first. |
| C-13 | Medium | Numeric settings have no limits | `component/GuiManager/src/widgets/SettingsPaneLongWidget.cpp`, `component/ConfigManager/src/ConfigManager.cpp` `getLong` | The font size accepts INT_MIN to INT_MAX. A value such as `abc` becomes 0. A news poll interval of 0 can make the application poll the server in a loop. |
| C-14 | Medium | Clone proposes the wrong folder name | `component/GuiManager/src/containers/CloneRepoContainer.cpp` `onTextChanged` | `QFileInfo::baseName()` turns `my.project.git` into `my`. A `git@host:user/repo.git` URL gives an empty name. |
| C-15 | Medium | Workspace and recent-list errors are silent | `component/GuiManager/src/MainWindow.cpp:844` | The workspace name is used as the file name with no checks. An invalid name fails silently, and an existing workspace is overwritten without a question. A workspace file that is not valid JSON fails silently. The recent list uses `;` and `\|` as separators, so a path with either character breaks it. |
| C-16 | Medium | Console stays dead after `exit` | `component/GuiManager/src/widgets/linux/BashShellWidget.cpp:271` `init()` with `_initialized`, and the same pattern in `win32/PowerShellWidget.cpp` | The shell starts only once. |
| C-17 | Medium | Console memory grows without limit | `lib/QVTerminal/src/qvtlayout.cpp` | The terminal has no scrollback limit. |
| C-18 | Low | Hard-coded shell paths | `component/GuiManager/src/widgets/win32/PowerShellWidget.cpp:439-446` | The `pwsh` and `C:\Windows` paths are hard-coded. |
| C-19 | Low | Dangling AppImage symlinks | `installer/build-appimage.sh` (AppRun script) | `~/keys` and `$PWD/keys` link to the temporary mount. They dangle after exit, and the script check does not replace them. |
| C-20 | Low | Windows installer logs are never moved; date-time widget editor missing | `src/launcher.cpp` (`QDir sourceDir(updatePath)`), `component/GuiManager/src/widgets/SettingsPaneDateTimeWidget.cpp` | `QDir` is built from a file path, so the `.log` files next to the installer are not copied to the Logs folder. The date-time widget adds its editor only when the item has a description. No current setting uses that widget. |

Also noted: the OAuth client secrets for Azure and Bitbucket are hard-coded as defaults in `component/ConfigManager/src/ConfigManager.cpp`.

## Git

| # | Severity | Problem | Location | Details |
|---|---|---|---|---|
| G-01 | High | Network errors look like bad credentials | `lib/GitRepo/src/GitRepoRemotes.cpp:389-396` (pull), `:780-788` (fetch), `:1076-1082` (push) | Any error, such as a network drop or a non-fast-forward push, clears the cached credentials and shows "Invalid credentials". The push is also retried. |
| G-02 | High | Fetch overwrites local tags | `lib/GitRepo/src/GitRepoRemotes.cpp:371`, `:776` | The refspec `+refs/tags/*:refs/tags/*` silently replaces a local tag of the same name. |
| G-03 | High | GPG signing can hang | `lib/GitRepo/src/GitRepoSigning.cpp:272` | `waitForFinished(-1)` has no time limit. A passphrase prompt that never returns blocks the worker thread. |
| G-04 | High | SSH host with a changed key is trusted | SSH known-hosts handling, `lib/GitRepo/src/SshKnownHosts.cpp` | A host whose key in `known_hosts` changed is trusted on first use. Already listed in `docs/GIT_TEST_PLAN.md` §6 item 5, still not fixed. |
| G-05 | Medium | Rebase stops on a commit that is already applied | `lib/GitRepo/src/GitRepoMerge.cpp:794-808` | `rebaseBranches` treats `GIT_EAPPLIED` as a failure and aborts. `commitRebase` at line 1197 skips such a commit instead. |
| G-06 | Medium | Bisect reset can get stuck | `lib/GitRepo/src/GitRepoBisect.cpp:376-405` | If the original branch was deleted, Reset reads the branch name as a SHA and fails. It returns before the cleanup, so the session can never be reset. |
| G-07 | Medium | Worktree remove dialog does not match the behavior | `component/GuiManager/src/containers/RepoContainer.cpp:8014`, `lib/GitRepo/src/GitRepoWorktrees.cpp:357-378` | The dialog says uncommitted changes will be deleted. The library refuses an unlocked dirty worktree, because force is false. |
| G-08 | Medium | SSH URLs with a user other than `git` are not seen as SSH | `lib/GitRepo/src/GitRepoCredentials.cpp:726-728`, `lib/GitRepo/src/GitRepo.cpp:2309` | Only `git@`, `ssh://` and `git://` are recognized. `alice@host:repo.git` gets password credentials and skips the commercial-plan check. |
| G-09 | Medium | No attempt limit for SSH key authentication | `lib/GitRepo/src/GitRepoCredentials.cpp:724-757` `credentialsAcquireCallback` | The SSH branch never counts attempts, so a key the server rejects can loop. |
| G-10 | Medium | Stash and pull with rebase ignore the Sourcerer profile | `lib/GitRepo/src/GitRepoStashes.cpp:643`, `lib/GitRepo/src/GitRepoRemotes.cpp:515` | `git_signature_default` is used, so the operation fails when `user.name` is not set in the git config. |
| G-11 | Medium | Uninitialized submodules cannot be set up | `lib/GitRepo/src/GitRepoSubmodules.cpp:692-755` `updateSubmodule` | No code calls `git_submodule_init` or `git_submodule_update`. `updateSubmodule` only stages the HEAD of the submodule. Existing cases TC-079 and TC-080 (now in `data_git.csv`) will probably fail. |
| G-12 | Medium | Merge commits are never signed | `lib/GitRepo/src/GitRepoMerge.cpp:259`, `component/GuiManager/src/containers/RepoContainer.cpp:4550` | `merge()` has no sign flag, and the drag-and-drop merge ignores "Sign commit". |
| G-13 | Medium | LFS content that is not a pointer fails checkout | `lib/GitRepo/src/GitLfs.cpp:1210-1216` | Content that is not a pointer is refused. git-lfs passes it through. |
| G-14 | Medium | LFS pointer stored inside a pointer | `lib/GitRepo/src/GitLfs.cpp:1124-1178` | If the file on disk already holds pointer text, staging stores a pointer to that pointer. |
| G-15 | Low | Whole LFS file held in memory | `lib/GitRepo/src/GitLfs.cpp:1199` | Each object is buffered with an `int` length cast. Very large files are at risk. |
| G-16 | Low | LFS file name with a space and no extension | `component/GuiManager/src/containers/GitRepoManagerBridgeImpl.cpp:88-96` | The raw name goes into `.gitattributes`, so `big data` becomes the pattern `big`. |
| G-17 | Low | Unstage fails before the first commit | `lib/GitRepo/src/GitRepoWorkingCopy.cpp:491` | HEAD must resolve. Hard to reach, because opening a repository with no commits creates an empty first commit (`lib/GitRepo/src/GitRepo.cpp:2425`). |
| G-18 | Low | Typo in the drag-and-drop menu | `component/GuiManager/src/containers/RepoContainer.cpp:4498` | The text says "Rebase abd merge". |

Possible gaps, not clearly bugs: there is no option to merge unrelated histories, no hook support, no shallow clone or sparse checkout, and no way to cancel a clone. The free plan gets no credential prompt for HTTPS. A patch made from a commit does not include binary changes. A patch path that git quotes (non-ASCII) is not found by `lib/GitRepo/src/PatchFileUtils.cpp:12-31`.

## Subversion

| # | Severity | Problem | Location | Details |
|---|---|---|---|---|
| S-01 | High | Deleting an external deletes its uncommitted changes | `lib/SvnRepo/src/SvnRepoRemotes.cpp:700`, `component/GuiManager/src/containers/RepoContainer.cpp:7891` | The whole folder is removed. The confirmation still says "delete submodule" and is not translated. |
| S-02 | High | Pop deletes the shelf when applying it left conflicts | `component/GuiManager/src/containers/RepoContainer.cpp:10195` | The shelved changes are lost if the user then reverts the conflicts. |
| S-03 | High | A failed checkout shows no error | `component/GuiManager/src/MainWindow.cpp:283-293`, `:1765-1788` | MainWindow never subscribes to `SvnRepoManagerCloneRepoError`. Only Git and Mercurial checkout errors get a toast. The progress toast just closes. |
| S-04 | Medium | An external opens as a Git repository | `component/GuiManager/src/containers/SvnRepoManagerBridgeImpl.cpp:1884`, `component/GuiManager/src/MainWindow.cpp:671-674` | The bridge emits `openRepoRequested`, and `MainWindow::onOpenRepo` always uses `ContainerType::GitRepo`. Reported by both the Subversion and the Common agent. |
| S-05 | Medium | The tree-conflict dialog promises a `.local` copy that the code deletes | `component/GuiManager/src/containers/SvnRepoManagerBridgeImpl.cpp:2384`, `lib/SvnRepo/src/SvnRepoWorkingCopy.cpp:1552` | The dialog says "Take incoming" keeps the file as `<name>.local`. After success the code removes that file. The dialog also always says "an unversioned local file blocks the incoming change", also for edit-versus-delete conflicts. |
| S-06 | Medium | A checked-out tag can be deleted or renamed | `lib/SvnRepo/src/SvnRepoBranches.cpp:1023`, `:1050` | `deleteTag` and `renameTag` have no guard and do not move the working copy. Branches have both (`:873`, `:939`). |
| S-07 | Medium | A failed merge can leave the working copy on another branch | `lib/SvnRepo/src/SvnRepoBranches.cpp:1113` | When the target is not checked out, the merge switches first. A later refusal or conflict leaves the working copy there. `docs/SVN_TEST_PLAN.md` §3.11.1 says a refusal leaves the working copy unchanged. |
| S-08 | Medium | Property conflicts cannot be resolved in the application | `lib/SvnRepo/src/SvnRepoWorkingCopy.cpp:517` | `listConflictsForAFile` handles only text conflicts. The user sees "No difference compared to the HEAD commit.", and staging is refused. |
| S-09 | Medium | Relative external URLs are probably handled wrong | `lib/SvnRepo/src/SvnRepoRemotes.cpp:601` | `../` URLs are added to the URL of the defining folder without resolving `..`. Server-relative `/...` URLs probably go through `canonicalUrl` as a local path and become `file:///` URLs. |
| S-10 | Medium | "Amend previous commit" is visible for Subversion | `component/GuiManager/include/containers/RepoManagerBridge.h:19-77`, `component/GuiManager/src/containers/RepoContainer.cpp:10852` | `RepoFeatures::CommitAmend` does not exist, so the item shows whenever no merge or rebase runs. This contradicts `docs/SVN_TEST_PLAN.md:21`. Choosing it only shows an "unsupported" toast. |
| S-11 | Medium | Opening a subfolder of a working copy does nothing | `component/GuiManager/src/MainWindow.cpp:66`, `:559-566` | `isRepository` looks for `.svn` only in the chosen folder, although the library can find the working copy root. No message is shown. |
| S-12 | Low | "Sign commit" is ignored with no message | `lib/SvnRepo/src/SvnRepoWorkingCopy.cpp:1006` | The only trace is a log warning. |
| S-13 | Low | Revert message and pull error text | `component/GuiManager/src/containers/SvnRepoManagerBridgeImpl.cpp:5267`, `:4148` | The revert message is always `[Revert] <summary>`, so the library default `Revert "<summary>"` is never used. The pull failure path reads `SvnRepoError` from a Result event. It is unreachable today, but the message would be empty. |
| S-14 | Low | Text problems | Subversion bridge messages, `component/GuiManager/src/containers/InitRepoContainer.cpp:237` | "Subversion repository is up to-date." has a typo. The init error toast gives no reason. |

## Mercurial

| # | Severity | Problem | Location | Details |
|---|---|---|---|---|
| H-01 | High | Password visible on the command line | `lib/HgRepo/src/HgRepo.cpp:822-841` | A clone retried with credentials puts them in the hg process arguments, so the password shows in the process list. The command server sends them over the pipe instead. |
| H-02 | High | Abort can discard work | `lib/HgRepo/src/HgRepoBranches.cpp:871-876` | Abort after a backout conflict runs `hg update --clean -r .`, which throws away every uncommitted change. |
| H-03 | High | Same credentials for every remote | `component/HgRepoManager/src/HgRepoManagerCredentials.cpp:39-40`, `:119-120` | Stored credentials are keyed by repository path, not by URL, so the credentials for one host are sent to another. |
| H-04 | Medium | Success toast on conflicts | `lib/HgRepo/src/HgRepoBranches.cpp:1099`, `:1116-1117`; toasts at `component/GuiManager/src/containers/HgRepoManagerBridgeImpl.cpp:6370-6376` | Cherry-pick (graft) and revert (backout) report success when they stopped on conflicts. |
| H-05 | Medium | Blame ignores the commit | `component/GuiManager/src/containers/HgRepoManagerBridgeImpl.cpp:253-255`, `lib/HgRepo/src/HgRepoDiff.cpp:366-371` | The commit argument is not used, so blame always shows the working-directory version. |
| H-06 | Medium | A subrepo that is not checked out opens as Git | `component/GuiManager/include/RepositoryType.h` `repositoryTypeOf`, `component/GuiManager/src/MainWindow.cpp:679-686` | `openSubmodule` picks the type with `repositoryTypeOf`, which defaults to Git. `MainWindow::onOpenRepoWithType` does not check the folder first. |
| H-07 | Medium | Push ignores the upstream branch name | `lib/HgRepo/src/HgRepoRemotes.cpp:590-591` | The name from `[sourcerer-upstream]` is not used on push (`Q_UNUSED(remoteBranch)`). |
| H-08 | Medium | "Pushed" when nothing was sent | `lib/HgRepo/src/HgRepoRemotes.cpp:606-607` | The hg exit code 1 ("nothing to push") counts as success, so a push of only secret commits also says it pushed. |
| H-09 | Medium | Wrong text after a pull | `component/GuiManager/src/containers/HgRepoManagerBridgeImpl.cpp:5201` | A successful pull shows "Git repository is up to-date.". The same toast shows when a diverged pull left an uncommitted merge. |
| H-10 | Medium | Error text points to a setting that does not exist | `component/GuiManager/src/MainWindow.cpp:543` | The toast says to set the path of hg in Settings. `setExecutablePath` is never called, and there is no `MercurialExecutable` config key. Reported by both the Mercurial and the Common agent. |
| H-11 | Low | Leftover bookmark and missing version check | `lib/HgRepo/src/HgRepoShares.cpp:213-257`, `component/HgRepoManager/src/HgRepoManagerWorkers.cpp` (CloneRepo) | A failed worktree add does not remove its new bookmark. Clone checks only that hg exists, not the minimum version. Open and init check both. |

## Features in the plan but not in the code

These are gaps between the design documents and the code. The test cases follow the code.

### Subversion (`docs/SVN_TEST_PLAN.md`)

- There is no lock, unlock, steal or break.
- There is no working copy upgrade and no Cleanup command.
- There is no property editor beyond `svn:ignore`, `svn:global-ignores` and `svn:externals`.
- The checkout dialog has no depth or revision option.
- Update to a revision exists in the library, but the UI never offers it.
- Relocate is implemented, but the Remotes panel is hidden for Subversion, so the user cannot reach it.

### Mercurial (`docs/MERCURIAL_SUPPORT_PLAN.md`)

- Unstage does not run `hg forget` on a newly added file (`lib/HgRepo/src/HgRepoWorkingCopy.cpp:304-339`).
- Discard of an added file keeps the file on disk. The plan says it is deleted.
- There is no "Show closed branches" toggle, because the bridge never sets `includeClosed`.
- The UI cannot create local tags, and Push and Delete from remote are not disabled for them. The error comes only from the library.
- The Strip dialog does not show how many commits it will remove.
- Classic Git Flow finish creates no tag.
- The LFS storage status ignores `lfs.usercache`.
- There is no warning banner when git or svn is missing for Git or Subversion subrepos.
- Incoming, outgoing, topics, phase editing and certificate prompts are not in the code.
- The user cannot cancel a single command. Only closing the tab or the application cancels. Requests from the GUI wait with no time limit.
