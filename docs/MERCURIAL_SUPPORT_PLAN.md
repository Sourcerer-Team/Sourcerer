# Mercurial Support — Implementation Plan

Status: **final plan**, agreed on 2026-10-16. Nothing below is implemented
yet. The decisions are recorded in §6.

Mercurial is added as a third backend next to Git (libgit2) and Subversion
(libsvn). Unlike those two, Mercurial is **not linked in**: the user installs
`hg` themselves, and Sourcerer drives it through the Mercurial *command server*
(`hg serve --cmdserver pipe`). Each opened Mercurial repository container has
its own long-lived `hg` process, owned and controlled by Sourcerer.

The layering copies Subversion's exactly:

| Layer | Git | Subversion | Mercurial (new) |
|---|---|---|---|
| Library (free functions) | `lib/GitRepo` | `lib/SvnRepo` | `lib/HgRepo` |
| Component (FSM + per-repo worker thread) | `component/GitRepoManager` | `component/SvnRepoManager` | `component/HgRepoManager` |
| GUI bridge (`RepoManagerBridge`) | `GitRepoManagerBridgeImpl` | `SvnRepoManagerBridgeImpl` | `HgRepoManagerBridgeImpl` |
| Container type | `ContainerType::GitRepo` | `ContainerType::SvnRepo` | `ContainerType::HgRepo` |
| `RepositoryType` | `Git` | `Subversion` | `Mercurial` |

---

## 1. `lib/HgRepo` — the library

### 1.1 Public API (`include/HgRepo.h`)

Model it on `lib/SvnRepo/include/SvnRepo.h`: namespace `HgRepo`, and **one free
function for every free function declared in `GitRepo.h`**, with the same name,
the same parameters and the same return value, except that the repository
handle replaces `git_repository *`.

- **Handle.** `HgRepo::Repository *` is an opaque class that owns the command
  server process (see 1.2). `openRepository(path, author, email)` returns it and
  `closeRepository(repo)` destroys it. A raw pointer keeps the same ownership
  shape as `git_repository *` in `GitRepoManagerStructs.h`.
- **Types.** Where `GitRepo` types carry no libgit2 dependency, reuse them
  directly: `GitRepo::GitStatus`, `GitHistory`, `GitCommitStatus`,
  `FilesDiff*`, `BlameReport`, `BranchInfo`, `RemoteInfo`, `BisectStatus`,
  `WorktreeInfo`, the workflow types and the `GitCredentials*` types. SvnRepo
  had to duplicate most of these, and its bridge spends hundreds of lines
  converting them back (`convertSvn*ToGit*`, `SvnRepoManagerBridgeImpl.cpp:3015-3690`).
  - Mercurial node ids are 40-hex SHA-1, so `GitCommitInfo::oid` (a `git_oid`)
    can be filled with `git_oid_fromstr`. HgRepo only needs `lib/libgit2/include`
    on its include path, which SvnRepoManager already adds; it links nothing
    from libgit2.
  - With these types reused, the bridge needs no conversion layer.
- **Errors.** Add `enum class HgError` and `struct HgErrorContext` (with
  `error`, `errorCode` = hg exit code, `errorMessage` = the hg stderr tail), and
  `getLastError()`. Keep them thread-local, the same way as SvnRepo.
- **Other functions:**
  - `initForCurrentThread()` / `shutdownForCurrentThread()`.
  - `finishPendingCredentialPrompts()`.
  - `findExecutable()` / `version()`, used by detection (§1.6).
  - Mercurial-only extras, in the way SvnRepo added `ignoreFiles` and
    `resolveTreeConflict`: `markResolved(repo, files)` and `listBookmarks(repo)`.

### 1.2 Command server client (`src/HgCommandServer.{h,cpp}`, private)

There is one `HgCommandServer` per `Repository`. It is created and used **only
on the repository's worker thread** in HgRepoManager, so it needs no locking and
no Qt event loop: it uses blocking `waitForReadyRead` / `waitForBytesWritten`.

**Starting the server.** It uses a `QProcess`:
```
<hg> serve --cmdserver pipe
     --config ui.interactive=False
     --config ui.merge=internal:merge          # conflict markers in the working file
     --config extensions.rebase=
     --config extensions.strip=                # Strip (§1.4.4)
     --config extensions.remotenames=          # remote branches/bookmarks (§1.4.2)
     --config remotenames.bookmarks=True --config remotenames.branches=True
     --config extensions.share=                # worktrees (§1.4.6)
     [--config extensions.lfs=]                # only when .hg/requires contains "lfs" (§1.4.8)
```
All of these extensions ship with Mercurial itself, so the user doesn't need to
install anything else.
- Working directory: the repository root.
- Environment: `HGPLAIN=1` (stable, untranslated output), `HGENCODING=UTF-8`,
  and `HGPLAINEXCEPT=` left empty.
- The user's `HGRCPATH` is **not** overridden, so their `ui.username`,
  extensions and auth settings still apply.
- On Windows, set `CREATE_NO_WINDOW` through
  `setCreateProcessArgumentsModifier` so no console window flashes.

**Protocol.** These are the standard command server framings:
- **Hello.** On start, read the `o` channel hello block. Check that
  `capabilities` contains `runcommand` and that `encoding` is `UTF-8`. Otherwise
  fail with `HgError::ServerHandshakeFailed`.
- **Running a command.** `runcommand(args, stdinData = {})` writes
  `"runcommand\n"`, then a big-endian uint32 length, then the arguments joined
  by `\0`.
- **Reading the reply.** It reads frames until the `r` (result) channel:
  - `o` frames go to stdout;
  - `e` frames go to stderr;
  - `d` (debug) frames are logged;
  - an `I`/`L` input request is answered from `stdinData`. This is used by
    `hg import -` and `hg commit -l -`, so commit messages and patches never go
    through argv or temp files. If there is no `stdinData`, it answers with an
    empty block.
- **Result.** `struct HgCommandResult { int exitCode; QByteArray out; QByteArray err; }`.

**Helpers:**
- `runJson(args)` appends `-T json` and parses the output with
  `QJsonDocument`.
- `runOrFail(HgError, args)` records `HgErrorContext` from `exitCode` and
  `Util::lastLines(err)`.

**Robustness:**
- If the process has died (it crashed, or the user killed it), restart it once
  and retry the command. Do this only for read-only commands; the call carries a
  flag for this.
- **Cancellation:** the command server cannot cancel a running command. When
  the worker's `quit` flag is set during a long command (pull, push, clone,
  history), the client kills the process (`QProcess::kill`). The current call
  returns `HgError::Cancelled`, and the next call restarts the server lazily.
  The quit flag is polled between `waitForReadyRead(100)` slices. `QuitScope`
  works like SvnRepo's (`thread_local const bool *`).
- **Shutdown:** `closeRepository` closes the write channel, so the server exits
  at EOF. It then calls `waitForFinished(3000)`, and `kill()` if the process is
  still running.

### 1.3 Mapping the GitRepo functions to Mercurial

Every function runs through `HgCommandServer`. Commit ids are full node hashes.
The bridge passes them in the same way it passes SHAs for Git.

| GitRepo function(s) | Mercurial implementation |
|---|---|
| `openRepository` | Check that `<path>/.hg` exists, start the server, run `hg root`. |
| `initRepository` | One-shot `hg init <dir>` (§1.5). `defaultBranch` is ignored: Mercurial's is always `default`. Optionally write `[ui] username` into `.hg/hgrc` from author/email. |
| `cloneRepository` | One-shot `hg clone <url> <dest>` with progress (§1.5). |
| `getStatus` | `hg status -T json --all` (minus `clean`), mapped as in §1.4 *Staging*. Unresolved conflicts come from `hg resolve -l -T json` and go to `conflictedFiles`. |
| `stageFiles` / `unstageFiles` / `commitStaged` / `amend` | See §1.4 *Staging*. Commit uses `hg commit -u "Name <email>" -l - <staged files>`, with the message on stdin. Amend adds `--amend`. `signoff` appends a `Signed-off-by:` trailer. `signCommit` is unsupported. |
| `discardFiles` | `hg revert --no-backup <files>`. Untracked files are deleted from disk, matching Git's behaviour. |
| `diffFiles` / `diffFilesFromCommit` | `hg diff --git [-c REV] <files>`, parsed into `FilesDiffDelta`/hunks. Reuse the unified-diff parser from GitRepo/SvnRepoDiff if one can be shared. Otherwise add one in `HgRepoDiff.cpp`. `repoContent` comes from `hg cat -r <.|REV> <file>`. |
| `blame` | `hg annotate -T json -u -c -l --line-number <files>`. The summary comes from one `hg log -r <set> -T json` lookup per distinct node. |
| `getBranchHistory` (paged) | `hg log -T json -r "limit(reverse(<set>), PAGE_SIZE, page*PAGE_SIZE)"`. `<set>` is `all()` for `ALL_BRANCHES`, `::branch(NAME)` / `::bookmark(NAME)` / `::tag(NAME)` otherwise. `total` comes from `hg log -r "<set>" -T "x"` (counted). Page 0 also fills `headCommit` (`.`), `refTips`/`refLabels` (§1.4.1) and `bisectMarks`. |
| `getCommitStatus` / `getCommitBranches/Tags/Heads/IsHead` | `hg log -r REV -T json` plus `hg status --change REV -T json`. Heads come from the `head()` revset. |
| `getActiveBranch` | `hg branch`, or the active bookmark (`hg log -r . -T "{activebookmark}"`). |
| `listLocalBranches` / `checkoutBranch` / `createBranchFromCommit` / `renameBranch` / `deleteBranch` | See §1.4.1. Additional functions: `listLocalBranchesWithKind`, `createNamedBranchFromCommit`, `closeNamedBranch`. |
| `listTags` / `createTagFromCommit` / `deleteTag` / `renameTag` | `hg tags -T json` (without `tip`). Create is `hg tag -r REV -m MSG NAME`; when `isAnnotated` is false, use `hg tag --local`. Delete is `hg tag --remove NAME`. Rename is remove plus add. |
| `pushTag` / `pushAllTags` / `deleteRemoteTag` | Global tags are commits in `.hgtags` and travel with `push`. `pushTag` and `pushAllTags` push the commit that records the tag: `hg push -r <tag commit>`. `deleteRemoteTag` runs `hg tag --remove NAME` and then pushes. Local tags (`--local`) are never pushed, and the Push to remote and Delete from remote menu items are disabled for them. |
| `checkoutCommit` | `hg update -r REV`. |
| `merge` | `hg update DEST`, `hg merge SOURCE`, then `hg commit -m "Merge SOURCE into DEST"` if there are no conflicts. |
| `getIsMergeInProgress` / `abortMerge` | A merge is in progress if `hg log -r "p2()"` is not empty. Abort is `hg merge --abort`. |
| `rebaseBranches` / `getIsRebaseInProgress` / `abortRebase` / `commitRebase` | rebase extension: `hg rebase -s SOURCE -d DEST`. In progress means `.hg/rebasestate` exists. Abort is `--abort`. Commit is `hg resolve -m` on the staged files, then `hg rebase --continue`. |
| `squashBranches` | `hg rebase --collapse -s SOURCE -d DEST -l -`. |
| `cherryPickCommit` | `hg graft -r REV`. |
| `revertCommit` | `hg backout -r REV -m MSG`. |
| `resetToCommit` | Maps to **Strip** (§1.4.4). An additional function is `strip(repo, rev, keepWorkingCopy, backup)`. |
| `bisect` / `getBisectMarks` | Native `hg bisect --good/--bad/--skip/--reset [REV]`. Marks come from the `bisect(good)`, `bisect(bad)`, `bisect(skip)` and `bisect(current)` revsets. |
| `stash` / `listStashes` / `applyStash` / `deleteStash` / `deleteAllStashes` / `getStashStatus` / `listConflictsForStashFile` | `hg shelve` is in core: `--name`, `--list`, `--unshelve --keep NAME`, `--delete NAME`, `--cleanup`, `--patch`. Index 0 is the newest shelve, as in SvnRepo. |
| `applyPatch` / `createPatchFromCommit` | `hg import --no-commit -` (patch on stdin) and `hg export -r REV`. |
| `pull` / `fetchAll` / `fetchRemote` / `push` | Pull is `hg pull [--update \| --rebase] [PATH]`. Fetch is `hg pull` without update. Push is `hg push [-b BRANCH \| -B BOOKMARK] [-f] [--new-branch]`. |
| `listRemotes` / `getDefaultRemote` / `add/update/removeRemote` / `setDefaultRemote` | `hg paths -T json`. Edits go to the `[paths]` section of `.hg/hgrc` through a small hgrc reader/writer (`HgRc.cpp`) that keeps comments and order. `default` is the default remote. |
| `addTarget` / `editTarget` / `deleteTarget` / `setDefaultTarget` | The active ("Default") target lives in `[paths]`: `fetchUrl` is `<name>`, and `pushUrl` is the `<name>:pushurl` sub-option (or `default-push`). Further targets are stored in `[sourcerer-targets]` as `<remote>.<target>.fetch` / `.push`. `setDefaultTarget` swaps the chosen target into `[paths]`, the same way Git's implementation rewrites the remote's URLs. |
| `listRemoteBranches(WithStatus)` / `deleteRemoteBranch` | Comes from the remotenames extension, with ahead/behind counted against the remote (§1.4.2). |
| `setUpstream` / `getUpstream` | Upstream mapping stored by Sourcerer (§1.4.3). |
| `listSubmodules*` / `getSubmodulePath` / `add/delete/updateSubmodule` | Full subrepo support (§1.4.5). |
| `listConflictsForAFile` | Mirror Git's semantics: read the file with its conflict markers. `hasConflicts` is true when `hg resolve -l` lists unresolved files. |
| `getFileSizeFromCommit` | `hg files -r REV -T "{size}" <file>`. |
| `listWorktrees` / `addWorktree` / `removeWorktree` / `lockWorktree` / `unlockWorktree` | Shares, built on the share extension (§1.4.6). |
| GitFlow, GitLab Flow, GitHub Flow, git-flow-next, `get/setWorkflowType`, `applyGitFlowNextWorkflow` | Implemented on named branches and bookmarks (§1.4.7). |
| `getLfsStorageStatus` + LFS add/remove | Built on the lfs extension (§1.4.8). |
| `signCommit` | The only Git feature left unsupported. Mercurial has no built-in commit signing. The option stays disabled for Mercurial repositories. |
| credentials (`getBasicAuthHeader`, `getSshAuthInfo`, `clearCachedCredentials`) | See §1.7. |

### 1.4 Semantic decisions

- **Staging.** Mercurial has no index. Do the same as SvnRepo's changelist:
  - Keep a Sourcerer-owned **staged set** in `.hg/sourcerer/staged`, one
    repo-relative path per line, rewritten atomically.
  - `stageFiles`:
    - an untracked file (`?`) is `hg add`ed;
    - a missing file (`!`) is `hg remove --after`ed;
    - the path is added to the set.
  - `unstageFiles`:
    - a file added by staging (`A`, not in the parent) is `hg forget`ed;
    - the path is removed from the set.
  - `getStatus` maps the result:
    - a path in the set goes to the `*StagedFiles` lists, otherwise to the
      unstaged lists;
    - `A` maps to new, `M` to modified, `R` and `!` to deleted, and `?` to new
      (untracked);
    - renames come from `hg status -C`.
  - `commitStaged` commits exactly the staged files and then clears the set.
  - After `merge` and `rebase`, Mercurial must commit the whole working
    directory, so the bridge stages everything.
  - Hunk or line staging is not available. It is hidden through a feature flag
    if the container exposes one.
- **Phases** are shown as badges in the history (§1.4.9). Strip and rebase also
  warn when public commits are involved (§1.4.4).

#### 1.4.1 Named branches and bookmarks

*Local branches* shows **both kinds, in two visually separated sections**:
"Named branches" first, then "Bookmarks".

- **Listing.** `listLocalBranchesWithKind(repo)` returns
  `QVector<HgBranchInfo { QString name; HgBranchKind kind; bool active; bool closed; QString node; }>`,
  where `HgBranchKind` is `{NamedBranch, Bookmark}`.
  - Named branches come from `hg branches -T json`. Closed branches are hidden
    by default; a "Show closed branches" toggle in the page's context menu shows
    them dimmed.
  - Bookmarks come from `hg bookmarks -T json`.
  - The plain `listLocalBranches` (API parity) returns named branches first,
    then bookmarks, using the internal names in the next bullet.
- **Internal names.** A branch and a bookmark may have the same name, so the
  bridge and the history API identify an entry by kind + name:
  - named branch → `NAME`;
  - bookmark → `@NAME` (`@` is not allowed at the start of a branch name in
    Sourcerer's dialogs).
  - Only the bare `NAME` is displayed. The section shows the kind.
- **Create.** `createBranchFromCommit` (API parity) creates a bookmark.
  - `createNamedBranchFromCommit(repo, rev, name)` runs `hg update -r REV`, then
    `hg branch NAME`. Mercurial only records a named branch with the next
    commit, so the branch appears in the list as soon as it is set (from
    `hg branch`, marked "(new)") and becomes a real branch at the first commit.
  - The GUI asks which kind to create (§3.2): **Bookmark** or **Named branch**.
- **Checkout.** A named branch runs `hg update BRANCH`, which goes to the branch
  tip. A bookmark runs `hg update BOOKMARK`, which also makes the bookmark
  active.
- **Rename.**
  - Bookmark: `hg bookmark -m OLD NEW`.
  - Named branch: Mercurial cannot rename a branch that already has commits.
    Rename is only offered for a "(new)" branch that has no commits yet (it runs
    `hg branch NEW` again). Otherwise the menu item is disabled, and its tooltip
    explains why.
- **Delete.**
  - Bookmark: `hg bookmark -d NAME`.
  - Named branch: the item is called **Close branch**. It runs `hg update NAME`,
    then `hg commit --close-branch -m "Close branch NAME"`, then updates back to
    the previous revision. This is `closeNamedBranch(repo, name, author, email)`.
- **Drag and drop (rebase/squash/merge)** works for both kinds. The source and
  destination resolve to revisions with `branch(NAME)`'s head or the bookmark.
- **Refs (`refTips` / `refLabels`) for the history graph and badges:**
  - named-branch heads map to `refs/heads/<branch>`;
  - bookmarks map to `refs/bookmarks/<bookmark>`, a new prefix. The badge
    painter shows it with a bookmark style, and colours it like `refs/heads`;
  - tags map to `refs/tags/<tag>`;
  - remote names map to `refs/remotes/<path>/<name>` (§1.4.2).

#### 1.4.2 Remote branches and ahead/behind counts

The `remotenames` extension (shipped with Mercurial) records, at every pull and
push, the remote's branch heads and bookmarks per path, for example
`default/feature`.
- `listRemoteBranches` reads them with
  `hg log -r "remotebranches() or remotebookmarks()" -T "{remotebranches}\n{remotebookmarks}\n"`,
  deduplicated.
- **Ahead/behind are counted against the remote**, like Git counts against its
  remote-tracking refs, which are also updated by fetch:
  - ahead = `len(only(LOCAL, REMOTE))`;
  - behind = `len(only(REMOTE, LOCAL))`;
  - here LOCAL is the local branch head or bookmark, and REMOTE is its upstream
    (§1.4.3), or by default the same name on `default`.
  - Both counts are revset queries, so they cost no network round-trip. The
    counts are refreshed by Fetch, Pull and Push, which contact the remote and
    update the remote names.
  - The results map to `BranchStatusType`: `UpToDate`, `Ahead`, `Behind`,
    `Diverged`, or `NoUpstream` when the remote has no such name.
- `fetchAll` / `fetchRemote` run `hg pull PATH`. Mercurial has no fetch-only
  mode, but `pull` without `--update` leaves the working directory untouched,
  which is the same thing.
- `deleteRemoteBranch`:
  - Bookmark: `hg push PATH -B NAME` after `hg bookmark -d NAME`. This is how
    Mercurial deletes a remote bookmark.
  - Named branch: close it locally (§1.4.1), then push.

#### 1.4.3 Upstream tracking

Mercurial has no per-branch upstream, so Sourcerer stores the mapping in the
repository's `.hg/hgrc`, in a section that hg ignores:
```ini
[sourcerer-upstream]
@feature = default/feature        # bookmark → path/name
stable   = backup/stable          # named branch → path/name
```
- `setUpstream` / `getUpstream` read and write this section through `HgRc`
  (§1.8).
- `pull` and `push` for the active branch use it:
  - `hg pull PATH -b NAME`, or `-B NAME` for a bookmark;
  - `hg push PATH -r .`, adding `-B NAME` for a bookmark and `--new-branch`
    when the remote doesn't have the branch yet;
  - with no upstream configured, they use `default` and the same name.
- The *Upstream* context menu in Local branches is enabled
  (`LocalBranchesContextMenuUpstream`).

#### 1.4.4 Strip (replaces Reset)

The history context menu shows **Strip** instead of "Reset … to commit".
- `strip(repo, rev, keepWorkingCopy, backup)` runs
  `hg strip -r REV [--keep] [--no-backup]`.
  - It removes **the selected commit and all its descendants**. The
    confirmation dialog lists how many commits that is
    (`len(descendants(REV))`).
  - "Keep working directory changes" maps to `--keep`.
  - "Save a backup bundle" is on by default. The backup goes to
    `.hg/strip-backup/`, and the dialog shows its path after success.
  - If any of the commits are public, the dialog warns that they are already
    pushed and will come back at the next pull.
- `resetToCommit(repo, rev, type)` is kept for API parity. Soft and Mixed map to
  `strip(rev, keep=true)`, Hard to `strip(rev, keep=false)`.
- A new feature flag, `HistoryContextMenuStrip`, shows the Strip item, and
  `HistoryContextMenuReset` stays off.

#### 1.4.5 Subrepositories (full, like Git submodules)

A subrepo is defined in `.hgsub` as `path = [kind]source` and pinned in
`.hgsubstate` as `<state> path`. Mercurial supports three kinds, and **all three
are fully supported**:

| Kind | `.hgsub` entry | Pinned state in `.hgsubstate` | Sourcerer backend | Opens in |
|---|---|---|---|---|
| Mercurial | `libs/core = https://…` (no prefix, or `[hg]`) | 40-hex node | HgRepo | `ContainerType::HgRepo` |
| Git | `libs/vendor = [git]https://….git` | 40-hex Git SHA | GitRepo (libgit2) | `ContainerType::GitRepo` |
| Subversion | `libs/legacy = [svn]https://…` | revision number | SvnRepo (libsvn) | `ContainerType::SvnRepo` |

**Where the code lives.** `lib/HgRepo` stays Mercurial-only. It parses and
edits `.hgsub` / `.hgsubstate`, and returns
`QVector<HgSubrepo { QString path; QString source; HgSubrepoKind kind; QString pinnedState; }>`,
where `HgSubrepoKind` is `{Mercurial, Git, Subversion}`. The parts that need the
other backends run in **HgRepoManager's worker thread**, which links GitRepo and
SvnRepo and calls `GitRepo::initForCurrentThread()` /
`SvnRepo::initForCurrentThread()` on that thread next to HgRepo's. That keeps
libgit2 and libsvn out of `lib/HgRepo`.

- **`listSubmodules` / `listSubmodulesWithStatus`**:
  - `SubmoduleInfo.name` is the path, and `commit` is the pinned state;
  - the GUI shows a kind badge (hg / git / svn) next to each entry; it reads the
    kind from a new `SubmoduleInfo::kind` field, which Git leaves empty;
  - `Error` means the subrepo directory is missing or can't be read;
  - otherwise the status depends on the kind:

    | Kind | `UpToDate` when |
    |---|---|
    | Mercurial | the working parent (`hg -R SUB log -r . -T "{node}"`) equals the pinned node |
    | Git | `GitRepo::openRepository` + HEAD equals the pinned SHA |
    | Subversion | the working copy's revision (`SvnRepo` info) equals the pinned revision |

    In every case, a different value means `Outdated`.
- **`getSubmodulePath`** returns the absolute path.
- **Open** emits the new typed signal `openRepoWithTypeRequested(path, type)`
  (§3.3), so the subrepo opens in a container of its kind:
  - Mercurial → `HgRepo`;
  - Git → `GitRepo`;
  - Subversion → `SvnRepo`.
- **`addSubmodule(url, name, branch, kind)`.** The Add dialog gets a **Kind**
  selector (Mercurial / Git / Subversion), shown under a new `SubrepoKinds`
  flag. The steps:
  1. Clone or check out into `<root>/<name>`, with progress, using the same uuid
     as Git's add-submodule progress:
     - Mercurial: `hg clone [-u BRANCH] URL` (one-shot, §1.5);
     - Git: `GitRepo::cloneRepository`, then `checkoutBranch(BRANCH)` when a
       branch is given;
     - Subversion: `SvnRepo::cloneRepository`. For Subversion, `branch` means a
       path below the URL (for example `trunk` or `branches/x`), and the dialog
       placeholder says so.
  2. Append `name = [kind]url` to `.hgsub`, creating the file and running
     `hg add` when needed.
  3. Stage `.hgsub` in the staged set. `.hgsubstate` is written by the next
     commit, which Mercurial does for every kind.
- **`deleteSubmodule(name, path)`** is the same for all kinds:
  - remove the line from `.hgsub` (when `.hgsub` becomes empty, delete it with
    `hg remove .hgsub`);
  - delete the subrepo directory after confirmation (the GUI already confirms
    for Git);
  - stage `.hgsub`.
- **`updateSubmodule(name)`** brings the subrepo to its pinned state:
  - Mercurial: `hg -R SUB pull`, then `hg -R SUB update -r <node>`;
  - Git: `GitRepo::fetchAll`, then `GitRepo::checkoutCommit(<sha>)`;
  - Subversion: `SvnRepo::checkoutCommit(<rev>)`, which is an update to that
    revision.

  To pin a subrepo to its *current* state instead, commit in the parent;
  Mercurial records it in `.hgsubstate`.
- **Commits** in the parent run with `--config ui.commitsubrepos=False`, so a
  commit never silently recurses into a subrepo.
- **Mercurial needs the `git` and `svn` command-line tools.** Sourcerer's own
  operations on Git and Subversion subrepos use libgit2 and libsvn, but
  Mercurial itself runs `git` or `svn` whenever it recurses into such subrepos:
  `hg update`, `hg push`, `hg status -S`, `hg archive` and cloning the parent.
  - When a repository has Git or Subversion subrepos, `openRepository` checks
    that `git` / `svn` are on `PATH`.
  - If one is missing, the container shows a warning banner: "This repository
    has Git subrepos; Mercurial needs git installed for update, push and clone."
  - Mercurial's errors from those commands are shown as they are.
- **Feature flags.**
  - `SubmodulesTitleSubrepos` renames the page to "Subrepos".
  - `SubrepoKinds` shows the Kind selector and the kind badges.
  - The bridge also enables `ExternalsAddDialogPlaceholderSubmoduleName`.

#### 1.4.6 Worktrees (shares)

Worktrees are built on the `share` extension, which ships with Mercurial. A
share is a second working directory that uses the same store.
- **The registry is kept by Sourcerer.** A share knows its source
  (`.hg/sharedpath`), but the source doesn't know its shares. So the source
  keeps `.hg/sourcerer/shares.json`:
  `[{ "name", "path", "locked", "reason" }]`.
- **`listWorktrees`**:
  - the main working directory comes first (`isMain`);
  - then the registry entries sorted by name, each validated
    (`valid` = `<path>/.hg/sharedpath` exists and points back to this store);
  - `branch` comes from `hg -R <path> branch` or the active bookmark, and
    `head` is the short node;
  - `isCurrent` marks the entry whose path is the opened repository.
  - When a share is opened directly, the source is found through
    `.hg/sharedpath`, so the list is the same from every working directory.
- **`addWorktree(name, path, branch, createBranch, baseBranch)`**:
  - runs `hg share -B --noupdate SOURCE PATH` (`-B` shares bookmarks, so they
    behave like Git branches across worktrees);
  - then `hg -R PATH update BRANCH`;
  - with `createBranch`, it first creates a bookmark at `baseBranch` (or `.`),
    or a named branch when the user picked "Named branch" in the add dialog
    (§3.2);
  - finally it writes the registry entry.
- **`removeWorktree(name, force)`**:
  - refuses for the main working directory and for locked entries;
  - without `force`, refuses when `hg -R PATH status` is not empty;
  - otherwise deletes the directory and the registry entry.
- **`lockWorktree` / `unlockWorktree`** set or clear the `locked` flag and the
  reason in the registry. Lock only prevents removal, which matches what users
  see in Git.

#### 1.4.7 Branching workflows (Git Flow, GitLab Flow, GitHub Flow, git-flow-next)

Implement all four, reusing the backend-independent helpers in
`lib/GitRepo/include/WorkflowUtils.h` and `GitFlowNextWorkflow.h`:
`parseGitFlowNextConfig`, `gitFlowNextClassicPreset`,
`gitLabFlowEnvironmentChain`, `gitLabFlowNextEnvironment`,
`gitLabFlowReleaseBranchName`, `gitFlowNextTopicTypeForBranch`,
`gitFlowNextTagName`, `gitFlowNextAutoUpdateBranches` and others. Only the
repository operations are rewritten for Mercurial.

- **Configuration.** Git keeps it in the git config. Mercurial keeps it in
  `.hg/hgrc`:
  - `[sourcerer] workflow = <workflowTypeToString(...)>`;
  - `[gitflow]`, with the same keys as Git without the `gitflow.` prefix
    (`branch.main.type = base`, …). This section is read into
    `QList<QPair<QString,QString>>` with the prefix added back and given to
    `parseGitFlowNextConfig` unchanged;
  - `[sourcerer-gitflownext] id = …, revision = …` for
    `getGitFlowNextWorkflowReference`.
- **Branch kind for workflow branches.** A per-repository setting,
  `[sourcerer] flow-branch-kind = named | bookmark`, defaults to `named`. This is
  the convention used by the hgflow extension, so long-lived `develop` and
  `release/*` branches are visible in history. The workflow setup dialog shows
  it as a radio choice.
  - **Start** creates the branch of that kind from its start point.
  - **Finish** merges it into its parent(s) with the configured strategy:
    - `merge` is `hg merge` + commit;
    - `rebase` is `hg rebase`;
    - `squash` is `hg rebase --collapse`.
    Finish then tags it when configured (`hg tag`). The topic branch is then
    **closed** for a named branch (`commit --close-branch`) or **deleted** for a
    bookmark.
  - **Update** merges or rebases the parent into the topic branch (the
    downstream strategy).
- **GitLab Flow:**
  - environment promotion merges the source into the next environment branch,
    creating it from the source if it's missing;
  - release branches are created from main;
  - cherry-pick to a release is `hg graft` onto the release branch, then
    `hg update` back. On conflicts the release branch stays checked out and
    `conflicted` is set, as in Git.
- **GitHub Flow:** start / update (merge main into the branch) / finish (merge
  into main, then close or delete).
- **Classic Git Flow** (`start/finishGitFlowBranch`) maps to the git-flow-next
  classic preset, as in Git.
- All workflow operations reuse the `GitError` workflow codes, mapped to
  `HgError` equivalents (`HgError::WorkflowNotInitialized`, …), so the GUI
  messages stay the same.

#### 1.4.8 LFS (Mercurial `lfs` extension)

The `lfs` extension ships with Mercurial.
- **Tracking patterns** live in the versioned `.hglfs` file:
  ```ini
  [track]
  path:assets/logo.psd = all()
  ```
  - The container's *LFS → Add / Remove* actions (`addFilesToLfs` /
    `removeFilesFromLfs`, RepoContainer.cpp:10473-10620) currently edit
    `.gitattributes`. They move behind two new bridge methods,
    `addFilesToLfs` and `removeFilesFromLfs`:
    - the Git bridge keeps the existing `.gitattributes` code;
    - the Hg bridge edits `.hglfs` (`path:<file> = all()` lines) and stages
      it.
- **First use.** Adding the first LFS pattern writes `extensions.lfs=` into
  `.hg/hgrc`. On the next commit Mercurial adds the `lfs` requirement to
  `.hg/requires`. Before this, the user must confirm in a dialog: "This
  repository will require the lfs extension; clients without it cannot read it,
  and the server must support LFS." After that, the command server is restarted
  with the extension enabled.
- **`getLfsStorageStatus`** counts the objects and bytes in
  `.hg/store/lfs/objects` and, when configured, in `lfs.usercache`.
- **Transfers** are handled by hg itself during pull and push. The remote
  endpoint is the `lfs.url` config, or it is derived from the path. Credentials
  use the same per-command `auth.*` injection (§1.7).

#### 1.4.9 Phase badges

Every Mercurial commit has a phase: **public** (pushed or published),
**draft** (local, not pushed yet) or **secret** (never pushed).
- **Data.** Add `QString phase` to `GitRepo::GitCommitInfo`, and include it in
  `operator==`, so that a history refresh after a push notices draft commits
  becoming public. Git and Subversion leave it empty, so nothing changes for
  them.
  - HgRepo fills it from the `phase` field that `hg log -T json` already returns
    (§1.3, `getBranchHistory`). No extra command is needed.
  - `getCommitStatus` also returns it, through a new `GitCommitStatus::phase`
    field, for the commit details panel.
- **Badge.** The history view's badge painter, the one that draws branch and
  tag badges from `refLabels`, draws a phase badge after the ref badges when
  `phase` is set:
  - **draft** gets a filled badge in the accent colour;
  - **secret** gets a filled badge in the warning colour, with a lock glyph;
  - **public** gets **no badge**. Most history is public, so a badge on every
    row would only add noise. The absence of a badge means public, and the
    commit details panel shows "Phase: public" explicitly.
  - Colours come from `StylesheetManager`, so the badges follow light and dark
    themes like the existing ones.
- **Commit details panel.** It shows a "Phase:" row (draft / secret / public)
  under the SHA row.
- **Feature flag.** A new `HistoryPhaseBadges` flag, set only by the Hg bridge,
  shows the badge and the details row.

### 1.5 One-shot commands (clone, init)

Clone and init run before any repository exists. `HgRepo::runOneShot(args, cwd)`
starts a plain `hg` process: the same environment and `CREATE_NO_WINDOW` as the
server, but not as a command server.

Clone progress uses
`--config progress.assume-tty=True --config progress.delay=0 --config progress.format="topic number"`.
- The `\r`-separated stderr lines are parsed into `CloneProgress` events and
  sent with the existing `GitRepoManagerCloneRepoProgress` shape (as an
  `HgRepoManager*` event).
- If parsing yields nothing, the clone reports indeterminate progress, with
  only started and finished.

### 1.6 Locating hg

- A new config key, `ConfigKey::MercurialExecutable`, defaults to empty.
- Empty means `QStandardPaths::findExecutable("hg")`. On Windows it also checks
  `%ProgramFiles%\TortoiseHg\hg.exe` and `%ProgramFiles%\Mercurial\hg.exe`.
- `HgRepo::version()` runs `hg version -T "{ver}"` and requires **Mercurial ≥
  5.0**, for `merge --abort`, JSON templates on every command used, and shelve in
  core. On failure it sets `HgError::MercurialNotFound` or
  `HgError::MercurialTooOld`, with a message telling the user to install
  Mercurial or to set its path in Settings.
- Add a field for the path in the Settings container, next to the other
  external-tool settings.

### 1.7 Credentials

Run with `ui.interactive=False`, so hg never blocks on a prompt. For HTTP(S)
remotes:
1. Look up `GitCredentialStorage` for (repo path, URL), the same as
   `SvnRepo.cpp:193-277`.
2. If credentials are found, pass them for this command only:
   `--config auth.sourcerer.prefix=<scheme://host> --config auth.sourcerer.username=… --config auth.sourcerer.password=…`.
   Arguments go over the command server pipe, not the process command line, so
   they don't show up in process listings.
3. If the command fails with an authorization error (`abort: http authorization
   required` / HTTP 401/403), do the following:
   - send `GitRepoManagerGetCredentials` with a `GitCredentialsRequest`, reusing
     the Git overlay as SvnRepo does;
   - wait for `Submit` or `Cancel`;
   - optionally store the credentials, then retry once;
   - finish the prompt with `GitRepoManagerGetCredentialsFinished/Invalid`.
4. SSH remotes:
   - When `GitKeyStorage` / `getSshAuthInfo` has a key for the host, Sourcerer
     passes `--config ui.ssh="ssh -i <key> -o IdentitiesOnly=yes"` for that
     command. On Windows it uses the bundled or user `ssh.exe`, or
     TortoisePlink when that is the user's configured `ui.ssh`.
   - Without a stored key, the user's own `ui.ssh` and agent apply.
   - Key passphrases are not prompted through hg. A key with a passphrase needs
     ssh-agent or Pageant, and the GUI explains that when SSH auth fails.

### 1.8 Files and build

```
lib/HgRepo/
  CMakeLists.txt        # STATIC, Qt6::Core only; includeLib GitRepo (headers), Log, Util;
                        # include dir lib/libgit2/include for git_oid (header-only use)
  path.cmake            # set(HgRepo_INCLUDE_PATH .../include)
  include/HgRepo.h
  src/HgRepoPrivate.h   # setLastError, setUnsupported, QuitScope, path helpers
  src/HgCommandServer.{h,cpp}
  src/HgRc.{h,cpp}      # .hg/hgrc editing: [paths], [sourcerer*], [gitflow], [extensions]
  src/HgRepo.cpp        # open/close/init/clone, credentials, executable lookup
  src/HgRepoWorkingCopy.cpp   # status, staging, commit, discard, conflicts, patch
  src/HgRepoHistory.cpp       # log paging, commit status, bisect
  src/HgRepoBranches.cpp      # named branches, bookmarks, tags, merge, rebase, graft, backout, strip
  src/HgRepoDiff.cpp          # diff parse, blame, cat, file size
  src/HgRepoRemotes.cpp       # paths, remotenames, ahead/behind, upstream, pull, push
  src/HgRepoShelves.cpp
  src/HgRepoSubrepos.cpp      # .hgsub / .hgsubstate
  src/HgRepoShares.cpp        # worktrees via share + registry
  src/HgRepoWorkflows.cpp     # Git Flow / GitLab Flow / GitHub Flow / git-flow-next
  src/HgRepoLfs.cpp           # .hglfs, storage status
  src/HgRepoUnsupported.cpp   # commit signing only
```

`git_oid_fromstr` is the only libgit2 symbol used. Libgit2 is already linked
into the final executable, so HgRepo can call it without its own link dependency.
An alternative is to write the 20 bytes by hand and avoid the dependency
entirely. That is preferred, because HgRepo then stays header-only with respect
to libgit2.

---

## 2. `component/HgRepoManager`

This is a file-for-file copy of `component/SvnRepoManager`, with `Svn` renamed to
`Hg`:

- `HgRepoManager.{h,cpp}`
  - A `ComponentManager::Component`.
  - The constructor subscribes to every `HgRepoManager*` request event, the
    destructor unsubscribes them.
  - `eventCallback` wraps each request in an `HgRepoManagerEvent` and calls
    `_fsm.send`.
- `HgRepoManagerFsm`, `HgRepoManagerState`, `HgRepoManagerInitializationState`
  and `HgRepoManagerEvent`: unchanged logic.
- `HgRepoManagerStructs.h`:
  - `HgRepoCommandType`, plus `HgRepoCommand` with `uuid` and `syncUuid`;
  - one `final` command struct per operation;
  - `HgRepoResult {uuid, type, QVariant value}` and `HgRepoError`;
  - `HgRepoContext`:
    ```cpp
    struct HgRepoContext {
      QString uuid;
      BlockingQueue<HgRepoCommand *> commandsQueue;
      QThread *workersThread = nullptr;
      HgRepo::Repository *hgRepo = nullptr;   // owns the hg command server
      bool quit = false;
    };
    ```
  - `Q_DECLARE_METATYPE` for every value carried in `QVariant`.
- `HgRepoManagerProcessingState`:
  - **One worker thread per opened repository**, created by
    `scheduleWorkerForOpenRepo`. This thread owns the `hg serve` process, which
    gives the required "one hg instance per container".
  - The worker loop calls `HgRepo::initForCurrentThread()` at the start and
    `shutdownForCurrentThread()` at the end.
  - Following GitRepoManager (`GitRepoManagerProcessingState.cpp:6851`), it calls
    `HgRepo::finishPendingCredentialPrompts()` after **every** command.
  - The command set is **all of GitRepoManager's commands**: worktrees,
    GitFlow/Workflow, LFS storage status, rebase and bisect included.
    Mercurial-specific commands are added on top: `MarkResolved`,
    `ListLocalBranchesWithKind`, `CreateNamedBranchFromCommit`,
    `CloseNamedBranch`, `Strip`, `AddFilesToLfs` and `RemoveFilesFromLfs`.
  - **Subrepos of the Git and Subversion kinds** (§1.4.5) are handled here:
    clone, status and update go through `GitRepo` / `SvnRepo` on the same
    worker thread. So `component/HgRepoManager/CMakeLists.txt` adds
    `includeLib(HgRepoManager GitRepo)`, `includeLib(HgRepoManager SvnRepo)`
    and the subversion/libgit2 include directories, the same ones
    SvnRepoManager uses.
- **Context lifetime, which differs from Svn/Git:**
  - `CloseRepo` sets `quit`, then closes the repository so the hg process
    exits.
  - It then **destroys the context and joins the thread** (subscribe
    `HgRepoManagerCloseRepoResult` to `destroyContext`). This prevents idle hg
    processes and threads from piling up while the app runs.
  - FSM `exit()` → `destroyContextsMap()` still cleans up on shutdown.
  - `destroyContext` must run off the EventManager thread, or with a bounded
    wait, because `waitForFinished` can take up to 3 s.

Wiring:
- `component/EventManager/include/Events.h`: add a `/* HgRepoManager */` block to
  `EVENT_TYPE_LIST`, with `HgRepoManagerX`, `HgRepoManagerXResult` and
  `HgRepoManagerXError` for every operation, plus `CloneRepoProgress`.
  Credentials reuse the `GitRepoManagerGetCredentials*` events.
- `src/main.cpp`: `#include <HgRepoManager.h>` and
  `componentManager.addThread<HgRepoManager::HgRepoManager>();` next to the SVN
  manager (line 185).
- `src/CMakeLists.txt`: `includeComponent(Sourcerer HgRepoManager)` and
  `includeLib(Sourcerer HgRepo)`. Also add
  `option(SOURCERER_BUILD_HGREPO_TESTS ...)`, mirroring the SvnRepo tests option.
- `component/GuiManager/CMakeLists.txt`: add an **explicit**
  `includeComponent(GuiManager HgRepoManager)` and `includeLib(GuiManager HgRepo)`.
  Do not rely on the directory-scope include leak that SvnRepoManager
  currently depends on. Also add the explicit `includeComponent(GuiManager SvnRepoManager)`
  while there.

---

## 3. GUI: `HgRepoManagerBridgeImpl` and `RepoContainer`

### 3.1 Bridge

Add `include/containers/HgRepoManagerBridgeImpl.h` and
`src/containers/HgRepoManagerBridgeImpl.cpp`. The class is
`final : public RepoManagerBridge`, structured like `SvnRepoManagerBridgeImpl`.
- Every method follows the same pattern:
  - `startWait`;
  - send `HgRepoManagerX` with an `HgRepoXCommand{uuid, syncUuid, …}`;
  - `waitForAnswer` on `{XResult, XError}`;
  - dispatch the UI update to the main thread.
- Because HgRepo returns GitRepo types (§1.1), it has **no `convert*`
  helpers**.
- `reportUnsupportedOperation(op)` shows "%1 is not supported for Mercurial
  repositories." It must go through `Util::Dispatcher::dispatchToMainThread`.
  The SVN version calls `showErrorToast` directly from the worker thread, which
  is a bug to fix in the same change.
- `ignoreFilesByProperty` returns `false`, and the container writes the ignore
  file (§3.2).

`features()` for Mercurial:
```
GitFlow | Shelves | Remotes | Worktrees | GitLFS |
CommitInfoSha | CommitInfoEmail | HistoryColumnSha | HistoryColumnEmail | SearchBarSha |
HistoryContextMenuCopyToClipboard | HistoryContextMenuCopyToClipboardSHA |
HistoryViewContextMenuCheckoutCommit | HistoryViewContextMenuBisect | PushAvailable |
StashInfoTitleShelf | StashInfoAuthor | LocalBranchesDragAndDropRebase |
LocalBranchesDragAndDropSquash | CherryPickDialogSHA | LocalBranchesContextMenuUpstream |
TagsContextMenuPushToRemote | TagsContextMenuDeleteFromRemote |
ExternalsAddDialogPlaceholderSubmoduleName |
// new flags:
PullMenu | PushMenu | AllBranchesItem | HistoryContextMenuStrip | BranchKinds |
SubmodulesTitleSubrepos | SubrepoKinds | HistoryPhaseBadges
```
Only `HistoryContextMenuReset` (replaced by Strip) and `Externals`
(Subversion's naming) are left out.

New `RepoFeatures` flags. Bit 12 is free; the others are `1ULL << 37` and up.

| Flag | Meaning | Git | Svn | Hg |
|---|---|---|---|---|
| `PullMenu` | Pull button menu: Pull / Pull with rebase / Fetch (replaces `gitType`, RepoContainer.cpp:1196-1220) | ✓ | | ✓ |
| `PushMenu` | Push button menu: Push / Force push (replaces `gitType`, lines 1233-1249) | ✓ | | ✓ |
| `AllBranchesItem` | "All branches" item in Local branches (replaces `gitType`, line 613) | ✓ | | ✓ |
| `HistoryContextMenuStrip` | Strip item and dialog (§1.4.4) | | | ✓ |
| `BranchKinds` | Bookmark / Named branch choice when creating, sectioned Local branches page (§3.2) | | | ✓ |
| `SubmodulesTitleSubrepos` | "Subrepos" page title and labels | | | ✓ |
| `HistoryPhaseBadges` | Draft and secret badges in history, plus a "Phase:" row in commit details (§1.4.9) | | | ✓ |
| `SubrepoKinds` | Kind selector (Mercurial / Git / Subversion) in the Add dialog and kind badges in the list (§1.4.5) | | | ✓ |

New `RepoManagerBridge` virtuals. Each has a default that keeps today's
behaviour, so the Git and Svn bridges compile unchanged:
- `enum class BranchKind { Default, Bookmark, NamedBranch };`.
  `createBranchFromCommit(..., BranchKind kind = BranchKind::Default)` gets the
  extra defaulted parameter; `addWorktree` gets the same parameter.
- `closeBranch(guard, name)`, which defaults to `deleteBranch`.
- `addSubmodule(guard, url, name, branch, RepositoryType kind = RepositoryType::Git)`:
  a new defaulted parameter. The Git and Svn bridges ignore it.
- `strip(guard, commit, keepWorkingCopy, backup)`, which defaults to
  `reportUnsupportedOperation`.
- `addFilesToLfs(guard, files)` / `removeFilesFromLfs(guard, files)`. The Git
  bridge moves the current RepoContainer `.gitattributes` code here (§1.4.8).
- `getLocalBranchSections()`. It fills the Local branches page with
  `{title, branches}` sections; the default is a single untitled section, which
  is what the page shows today.

### 3.2 RepoContainer changes (`RepoContainer.{h,cpp}`)

- `createRepoBridge()` (line 496): add
  `case RepositoryType::Mercurial: return std::make_unique<HgRepoManagerBridgeImpl>(container);`.
  Convert it to a `switch`.
- Header: forward declaration and `friend class HgRepoManagerBridgeImpl;`, next
  to lines 71-79.
- `gitType` (line 611 and its uses at 613, 1196-1249): replace with
  `hasFeature(AllBranchesItem / PullMenu / PushMenu)`.
- **Local branches page, in two sections.** `LocalGitBranchesPage` gets
  `updateModel(const QVector<BranchSection> &sections)`. `BranchesModel` adds a
  non-selectable, non-draggable header row per titled section ("Named
  branches", "Bookmarks"), drawn by the delegate as a small caps label with a
  separator line, the same way the Graphite mode already uses its own delegate.
  - Named-branch rows use a branch icon; bookmark rows use a bookmark icon.
  - Closed named branches are dimmed, and shown only when "Show closed
    branches" is on.
  - The existing `updateModel(QStringList)` wraps a single untitled section, so
    Git and Svn are unaffected.
  - Selection, active-branch highlight and drag and drop keep using the
    internal names (`NAME` / `@NAME`, §1.4.1).
- **Choosing the kind when creating a branch.** Every "Create branch" entry
  point goes through one helper, `requestBranchKind(QPoint)`:
  - from a commit (line 3268);
  - from the Local branches page (4488, 4541);
  - from tags and remotes (7206, 7277, 7308);
  - from line 7668;
  - from the worktree add dialog.

  With `BranchKinds`, the helper first shows a `QMenu` with **Bookmark** and
  **Named branch**, then opens the existing name dialog with the title "Create
  bookmark" or "Create named branch". It passes the kind to
  `createBranchFromCommit`. Without the flag, it returns
  `BranchKind::Default` right away, so Git and Svn don't change.
  - Context menus on Local branches entries depend on the kind:
    - bookmark: Rename… / Delete;
    - named branch: Rename… (only while new, §1.4.1) / **Close branch**.
- **Strip.** With `HistoryContextMenuStrip`, the history context menu
  (lines 3954-4011) shows **Strip…** where the Reset submenu would be. It opens
  a confirmation dialog: the number of commits to strip, a "Keep working
  directory changes" checkbox, a "Save a backup bundle" checkbox (checked), and
  the public-commit warning. Then it calls `_bridge->strip(...)`.
- **LFS menu** (lines 8306, 8617, 8904, 9195, 9500, 9593): the actions call
  `_bridge->addFilesToLfs` / `removeFilesFromLfs` instead of the container's own
  `.gitattributes` code. The Hg bridge shows the first-use confirmation from
  §1.4.8.
- **Workflow setup dialog**: with an Hg bridge it shows the "Workflow branches
  are: Named branches / Bookmarks" choice (§1.4.7).
- `repoAdminSubdir()` (line 491): return `".hg"` for Mercurial.
  - Line 10993 hard-codes `GIT_SUBDIR`; change it to `repoAdminSubdir(_path)`.
- Ignore: the `.gitignore` path (`GIT_IGNORE_FILENAME`, line 116) becomes
  `ignoreFileName()`, which returns `.hgignore` for Mercurial.
  - When it creates a new `.hgignore`, it writes a `syntax: glob` header first,
    so the existing glob patterns keep working.
- **Own column layouts.** Change `layoutConfigKey(gitKey, svnKey)` (line 7921)
  to `layoutConfigKey(gitKey, svnKey, hgKey)`. Add `HgRepo*` keys to
  `ConfigKey.h` (copies of the `SvnRepo*` keys at lines 40-44: splitters,
  hidden columns, column widths), with settings names `hgRepo/...` and
  defaults in `ConfigManager.cpp`. Update every caller: 1739, 1747, 2995,
  3011, 3040, 7942, 7970 and 10828.
  - The key is chosen by `ContainerType`, as today: `HgRepo` → hg key,
    `SvnRepo` → svn key, otherwise the git key. Every Mercurial repository runs
    in an `HgRepo` container, including subrepos and shares (§3.3), so no other
    rule is needed.

### 3.3 Repository type plumbing

- `include/RepositoryType.h`:
  - Add `Mercurial` (append it, because the value may be persisted).
  - `repositoryTypeOf` checks `.git`, then `.hg`, then `.svn`.
  - `repositoryTypeKey` / `repositoryTypeFromKey` handle `"hg"`. Make
    `repositoryTypeKey` a `switch`: the current ternary writes every non-SVN
    type as "git".
- `include/widgets/ContainerWidget.h`: append `HgRepo` before `last`. Its
  integer value is stored in workspace files.
- `src/MainWindow.cpp`:
  - **Remove** the duplicate `OpenedRepositoryType` / `detectRepositoryType`
    (lines 61-76) and use `RepositoryType`. Keep the "Unknown" outcome through a
    separate `isRepository(path)` check, so a non-repository folder still warns
    instead of opening as Git.
  - `openRepositoryByType` (lines 505-520): add Mercurial → `addHgRepoContainer`.
    Before opening, call `HgRepo::version()`. If hg is missing, show an error
    toast and don't open a tab.
  - Merge `addGitRepoContainer` and `addSvnRepoContainer` (lines 1212-1286) into
    `addRepoContainer(ContainerType, path)`, which only connects
    `openPullRequestRequested` for Git, and route all three types through it.
  - **`onOpenRepo` (lines 606-612) stays unchanged.** It receives
    `RepoContainer::openRepoRequested(path)` and keeps opening Git submodules,
    Git worktrees and SVN externals exactly as today.
  - Add a new signal, `RepoContainer::openRepoWithTypeRequested(const QString &path, RepositoryType type)`,
    and a slot, `MainWindow::onOpenRepoWithType`, that opens the container for
    `type`: `HgRepo`, `GitRepo` or `SvnRepo`. Connect it in
    `addRepoContainer` next to `openRepoRequested`. Only Mercurial
    repositories emit it:
    - `HgRepoManagerBridgeImpl::openSubmodule` emits it with the subrepo's kind
      from `.hgsub`. A Mercurial subrepo opens in `HgRepo`, a `[git]` subrepo in
      `GitRepo`, and an `[svn]` subrepo in `SvnRepo`.
    - Opening a worktree from an `HgRepo` container (the call at
      RepoContainer.cpp:7812) emits it with `RepositoryType::Mercurial`, so
      shares open in `HgRepo`. For Git containers, the existing
      `openRepoRequested` call stays.
    - Before opening, the slot checks that the path really contains `.hg`,
      `.git` or `.svn` for the requested type. If it doesn't (for example, the
      subrepo isn't checked out yet), it shows an error toast that suggests
      Update.
  - `promptOpenClonedRepository(path, bool isSvn)` becomes
    `promptOpenClonedRepository(path, RepositoryType)`.
  - Workspace restore (lines 972-1000): handle `SvnRepo` (currently dropped) and
    `HgRepo`.
  - Subscribe and unsubscribe `HgRepoManagerCloneRepoResult/Error/Progress` and
    `HgRepoManagerInitRepoResult/Error` (lines 259-317, 1699-1757).
    - While there, fix the SVN clone-progress handler at around line 1727,
      which reads a `GitRepoResult`.
- `src/widgets/GitProgressToast.cpp:63-117`: add the `HgRepoManagerCloneRepo*`
  events.

---

## 4. Home screen

- **Icon.** Add `resources/icons/repo-type-hg.svg`.
  - Use the same 24×24, `stroke="currentColor"`, 1.6 stroke-width rounded
    style as `repo-type-svn.svg`, so `renderSvgToPixmap` can tint it: the
    rounded-rect frame with a lowercase "hg" glyph inside, or a simple droplet.
  - Don't use the official Mercurial logo bitmap.
  - Register it in `resources/resources.qrc` after line 77.
- **`RecentlyOpenedReposModel`**:
  - Add an `_hgIcon` member.
  - Render it in `repositoryTypeIcon()` (lines 113-128).
  - Turn the return into a `switch` on the type.
  - `makeRecentEntry` and `parseRecentEntry` work through `repositoryTypeKey`
    and `repositoryTypeFromKey`, so recents are persisted as `path|hg` with no
    other change.
  - Old entries without a suffix fall back to `repositoryTypeOf`, which now
    detects `.hg`.
- **Click → container.** `HomeContainer::recentClicked(path)` goes to
  `MainWindow::onRecentClicked`, then to `openRepositoryByType`. That opens an
  `HgRepo` container once §3.3 is done. Adding it to recents needs no change:
  `HgRepoManagerBridgeImpl::openRepo` calls `_container.saveRepoInRecentList()`
  after a successful open, as the SVN bridge does (line 4150).
### 4.1 Mercurial clone and init (required)

- **Home menus** (`HomeContainer.cpp:414-432`): add a "Mercurial clone" entry
  to the Clone button menu, after "Subversion checkout", and a "Mercurial init"
  entry to the Init button menu, after "Subversion create".
  - New `HomeContainer` signals, `cloneMercurial()` and `initMercurial()`, next
    to `cloneSubversion()` / `initSubversion()` (header lines 52-64). Their
    slots follow the pattern at lines 686-698 and 1771-1792.
  - In `MainWindow`, connect them in `addHomeContainer` (lines 1291-1319) to new
    `onCloneMercurial` / `onInitMercurial` slots, next to `onCloneSubversion` /
    `onInitSubversion` (lines 558-597).
  - When hg isn't found (§1.6), both entries are disabled and their tooltip
    says "Install Mercurial or set its path in Settings".
- **Clone** (`CloneRepoContainer`):
  - Add `CloneType::Mercurial` to the enum at `CloneRepoContainer.h:17`.
  - The labels (lines 53-57) become a `switch` on the type, replacing the
    `gitClone` bool: "Clone" / "Mercurial URL"; the destination dialog title
    is at line 97.
  - The command (lines 125-146): send `HgRepoCloneCommand` as
    `HgRepoManagerCloneRepo`.
  - HgRepoManager runs the one-shot `hg clone` with progress (§1.5). It sends
    `HgRepoManagerCloneRepoProgress`, which `GitProgressToast` shows (§3.3),
    then `...Result` or `...Error`.
  - On `HgRepoManagerCloneRepoResult`, `MainWindow` calls
    `promptOpenClonedRepository(path, RepositoryType::Mercurial)`.
  - HTTP authentication during the clone uses the credentials flow from §1.7.
    There is no repository path yet, so stored credentials are looked up by URL
    only, as the Git clone does.
- **Init** (`InitRepoContainer`):
  - Add `InitType::Mercurial` (`InitRepoContainer.h:17`).
  - The labels (lines 37-43) become a `switch`. The default-branch field is
    hidden, because Mercurial's first branch is always `default`.
  - Send `HgRepoInitRepoCommand` as `HgRepoManagerInitRepo`, and subscribe to
    `HgRepoManagerInitRepoResult` / `...Error` like the SVN code at lines
    57-81 and 186-205.
  - HgRepoManager runs `hg init <parent>/<name>`. When the author and email
    are set, it writes `[ui] username = Name <email>` into `.hg/hgrc`, so the
    first commit has the right author even without a global hg config.
  - On `HgRepoManagerInitRepoResult`, `MainWindow` calls `addHgRepoContainer`,
    like the SVN init (lines 1751-1757).
- **TopBarWidget** `open` / `clone` / `init` (MainWindow.cpp lines 207-209)
  stay Git-only, as they are for Subversion today.

---

## 5. Tests and verification

- `tests/hgrepo/tst_hgrepo.cpp` (QtTest), built when
  `SOURCERER_BUILD_HGREPO_TESTS=ON`. Each test case does `QSKIP` when
  `HgRepo::findExecutable()` is empty, so CI without hg stays green. The tests
  cover:
  - init, open and close (the hg process exits after close);
  - the status mapping and the staged set across add, modify, remove, missing
    and untracked files;
  - commit and amend of staged files only;
  - history paging (`hasMore`, `total`, ALL vs. a branch);
  - `phase` in history and commit status: draft after a commit, public after a
    push to a local remote, secret after `hg phase --secret --force`;
  - init with an author writes `[ui] username`; clone from a local path reports
    progress and finishes;
  - named branches: create (visible before the first commit), checkout, close,
    rename while new, the "show closed" listing;
  - bookmarks: create, rename, delete, the active bookmark;
  - a name used by both a branch and a bookmark stays distinct (`NAME` vs.
    `@NAME`);
  - tags CRUD, local vs. global, push and delete on the remote;
  - remote names after pull; ahead/behind/diverged counts against a second
    local repository used as the remote; deleting a remote bookmark;
  - upstream: set/get round-trip in `.hg/hgrc`, and pull/push using it;
  - strip: with and without `--keep`, the backup bundle created, descendants
    removed;
  - subrepos of all three kinds (Mercurial, Git, Subversion): add (clone or
    checkout + `[kind]` entry in `.hgsub`), commit records `.hgsubstate`, list
    with kind and UpToDate/Outdated status, update to the pinned state, delete.
    These tests run against local Git and Subversion repositories created by
    the test, and are skipped when the `git` / `svn` command-line tools that
    Mercurial needs are missing;
  - shares: add with a new bookmark and a new named branch, list from the source
    and from the share, lock prevents remove, remove with and without force;
  - workflows: git-flow-next classic init/start/finish/update with both branch
    kinds; GitLab Flow promote/release/cherry-pick-to-release; GitHub Flow
    start/update/finish; workflow type round-trip;
  - LFS: adding a pattern writes `.hglfs` and enables the extension; storage
    status after committing a tracked file;
  - merge with and without conflicts, `abortMerge`, rebase and abort, graft,
    backout;
  - shelve, unshelve and delete;
  - bisect marks;
  - diff and blame parsing;
  - `[paths]` editing round-trip (comments preserved);
  - pull and push between two local repositories;
  - cancellation: set quit during a slow command, then check the process is
    killed and the next call restarts it;
  - server crash recovery: kill the process, then check that a read-only command
    retries successfully.
- Manual checks:
  - open an hg repository from the Open dialog and from Recents (hg icon
    shown);
  - restart the app, then check the workspace restores the hg tab;
  - open two hg repositories and confirm there are two `hg serve` processes,
    and that closing a tab ends its process;
  - hg missing from PATH: an error toast is shown and no tab opens;
  - an HTTP remote that needs auth shows the credentials overlay;
  - Local branches shows "Named branches" and "Bookmarks" sections, and "Create
    branch" offers the Bookmark / Named branch menu from every entry point;
  - Strip dialog: the commit count is right, and the backup path is shown;
  - opening subrepos from an hg repository: a Mercurial subrepo opens in an
    HgRepo tab, a `[git]` subrepo in a GitRepo tab, and an `[svn]` subrepo in an
    SvnRepo tab. A share (worktree) opens in an HgRepo tab;
  - an hg repository with a `[git]` subrepo and no `git` on PATH shows the
    warning banner;
  - opening a Git submodule, a Git worktree or an SVN external behaves exactly
    as before;
  - the Mercurial column layout is saved separately from Git's;
  - Home → Clone → "Mercurial clone" clones over HTTP(S) with progress and
    offers to open the result in an HgRepo tab. Home → Init → "Mercurial init"
    creates a repository whose `.hg/hgrc` has the username, and opens it. Both
    entries are disabled when hg isn't found;
  - phase badges: a new local commit shows "draft", and after a push the badge
    disappears. `hg phase --secret --force` on a commit shows "secret".

---

## 6. Decisions

These were open questions in the first draft and are now settled:

1. **Branch model.** Local branches shows named branches and bookmarks in two
   separated sections. Creating a branch asks **Bookmark** or **Named branch**,
   and both are implemented (§1.4.1, §3.2).
2. **Remote status.** Ahead/behind are counted against the remote (§1.4.2).
3. **Reset** is mapped to `hg strip` and called **Strip** (§1.4.4).
4. **Subrepos** have full functionality, like Git submodules, for all three
   kinds Mercurial supports: Mercurial, `[git]` and `[svn]` (§1.4.5).
5. **Layouts.** Mercurial has its own column layouts (§3.2).
6. **Also planned:** worktrees (§1.4.6), all branching workflows (§1.4.7), LFS
   (§1.4.8) and upstream tracking (§1.4.3). Commit signing is the only Git
   feature left out, because Mercurial has no built-in signing.
7. **Opening nested repositories.** From a Mercurial repository, each subrepo
   opens in a container of its kind: Mercurial in `HgRepo`, `[git]` in
   `GitRepo`, and `[svn]` in `SvnRepo`. Mercurial worktrees (shares) open in
   `HgRepo`. This goes through the new `openRepoWithTypeRequested` signal. Git
   submodules, Git worktrees and SVN externals keep their current behaviour
   through the unchanged `onOpenRepo` (§3.3).

8. **Mercurial clone and init screens are required** (§4.1).
9. **Phases** are shown as history badges: draft and secret get a badge, public
   gets none. The commit details panel shows the phase (§1.4.9).

Smaller defaults, also confirmed:
- Workflow branches default to **named branches**, as in hgflow, and can be
  switched to bookmarks per repository (§1.4.7).
- A bookmark is shown with an `@` internal name only in code; the UI shows the
  bare name in the "Bookmarks" section.
- Enabling LFS asks for confirmation, because it adds a repository requirement
  (§1.4.8).

## 7. Suggested order of work

1. `lib/HgRepo` core: command server client, open/close/status/staging/commit,
   log paging, diff/blame, with tests.
2. `lib/HgRepo`: branches and bookmarks, tags, merge, rebase, graft, backout,
   strip, shelves, bisect, remotes, remote names and ahead/behind, upstream.
3. `lib/HgRepo`: subrepos, shares, workflows, LFS. Steps 2 and 3 are
   independent groups of files and can be built in parallel.
4. `component/HgRepoManager`, events in `Events.h`, `main.cpp`, CMake.
5. GUI groundwork that doesn't depend on Mercurial: the new `RepoFeatures`
   flags replacing `gitType`, the new bridge virtuals with defaults, the
   sectioned Local branches page, the `requestBranchKind` helper, moving the LFS
   file editing into the bridges, `layoutConfigKey` with a third key. Git and
   Svn must behave exactly as before.
6. `HgRepoManagerBridgeImpl` and the RepositoryType/MainWindow plumbing (§3),
   the Strip dialog, the workflow branch-kind option.
7. Home screen icon and recents (§4), then Mercurial clone and init (§4.1).

Steps 1-3 can be developed and tested without any GUI. Step 5 can run in
parallel with steps 1-4. Step 6 needs 4 and 5. The §3.3 refactors (the
duplicated type detection, workspace restore, the SVN clone-progress type) are
worth landing first as a separate change, because they also fix existing
Subversion bugs.
