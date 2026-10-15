## 1.1.0

### Summary

Release version 1.1.0 introduces Subversion support, Qt library updates, various bug fixes, and improvements to build stability across Linux and Windows platforms.

### New

* Implemented Subversion support

### Improved

* Updated Qt library to version 6.10.3
* Blame page is shown by default
* AWS and Bitbucket secrets are shown in settings
* macOS porting updates

### Fixes

* Fixed compilation errors on Linux and Windows builds
* Fixed xdg-open linkage error with AppImage
* Fixed GitFlow icons not showing
* Fixed issues with highlighting and missing function definitions
* Fixed GitHub actions triggers and build permissions
* Fixed Docker and CPU-related problems on GitHub builds


## 1.0.43

### Summary

This release adds six new color themes and syntax highlighting for 20 more languages, and it fixes a few bugs in the main window and in the highlighting for Makefiles.

### New

* **Themes:** purple (a darker variant), blue, Monokai, Gruvbox, Solarized, Dracula and Ayu.
* **Syntax highlighting** for BAT, XML, F#, Lua, Scala, Makefile, Ruby, Go, Zig, Kotlin, assembly, Objective-C, Swift, SQL, Terraform, Dockerfile and CMake files.

### Improves

* The sign-off and sign states are now saved between sessions.
* Theme names in the main menu have been renamed.

### Fixes

* A parent line in the commit graph no longer stretches far above the junction with its two children.
* On Windows, closing the main window while the application is maximized now works correctly.
* `.mk` files are now highlighted.


## 1.0.42

### Summary

* Release version updated to 1.0.42.

### New

* Added development updates.

### Improved

* Increased download timeout for update files to 30 seconds.

### Fixes

* Fixed an issue where updates were marked as seen before they finished downloading.
 

## 1.0.41

### Summary

Version 1.0.41 introduces improvements to repository navigation, search capabilities, and conflict resolution workflows, alongside important bug fixes for code editors.

### New

* Added a **Search repository** button in the filtering widget.
* Added support for showing the conflict position in the `1/n` format in the toolbar.

### Improved

* The search button now automatically disappears after the entire repository has been loaded.
* Optimized parent link and navigation buttons to prevent the app from unexpectedly loading the entire repository.

### Fixes

* Fixed an issue where bug and conflict resolution markers were unrecognized.
* Enabled syntax highlighting for the Git blame viewer.
* Fixed syntax highlighting for unified diff editors.


## 1.0.40

### Summary

Release v1.0.40 introduces stability improvements for UI spinners, layout refinements for the navigation bar and commit details group box, and visual adjustments to shortcut button colors.

### New

* Introduce a spinner break timer that hides the spinner if the dispatcher is inactive, with a timeout set to 1000ms.

### Improved

* Update project version to 1.0.40.
* Change shortcuts button colors.

### Fixes

* Adjust layout spacing to prevent the navigation bar from being too close to the commit details group box.
* Fix an issue where the loading spinner hangs during repeated calls to onHistoryChanged.


## 1.0.39

### Summary
Version 1.0.39 introduces syntax highlighting and resolves essential GDPR compliance requirements.

### New
* Implemented syntax highlighting

### Improved
* Updated release version bump to 1.0.39

### Fixes
* Addressed GDPR compliance issues


## 1.0.38

### Summary
Version 1.0.38 introduces macOS porting support, multiple workflow additions (GitLab-Flow, GitHub-Flow, GitFlow-Next, and Graphite.dev), a custom workflow editor, profile user avatars, and drag-and-drop tab reordering, alongside macOS title bar alignment improvements and Windows build fix updates.

### New
* macOS porting
* Implement GitLab-Flow workflow
* Implement GitHub-Flow workflow
* Implement GitFlow-Next workflow
* Implement Graphite.dev workflow
* Implement editor to create custom workflows like GitFlow
* Implement user avatars in profile manager
* Allow reordering of title bar tabs with drag and drop

### Improved
* [Mac] Title bar icons are now positioned the macOS way

### Fixes
* Fixes GitHub build error in Windows build
* Fixes C++ warnings during Windows build
* Fixes CMake warnings during Windows build
* 

## 1.0.37

Sourcerer 1.0.37 adds Git worktrees support and makes the diff and merge viewers easier to read in dark themes.

### New
- **Worktrees:** a new Worktrees panel lists the main worktree and all linked worktrees of a repository. You can open a worktree in its own tab, add a new one, lock or unlock it, and remove it. #635
- **Add worktree dialog:** choose the folder and the branch to base the worktree on from a dropdown. You can also create a new branch from the selected one.

### Improved
- **Diff and merge viewer colors:** the change markers have new colors that look better on dark themes. #634
- **Branch actions with worktrees:** a branch that is checked out in another worktree can't be checked out, renamed or deleted. These actions are disabled in the branch menu, with a message that names the worktree.

### Fixes
- Fixed the release notes being displayed badly. #633
- Fixed the active branch shown for a repository when other worktrees have different branches checked out.


## 1.0.36

Sourcerer 1.0.36 reworks how the commit history is loaded and drawn. It is faster on large repositories and easier to read.

### New
- **Paged history loading:** repository history now loads page by page instead of all at once, so large repositories open faster and stay responsive. (#626
- **All branches (`*`) item:** the local branches panel has a `*` item that shows the combined history of all branches.
- **Ref badges in the graph:** branches and tags now appear as badges on the commits they point at. Hover the graph column to also see the local branches that contain a commit.

### Improved
- **Incremental graph drawing:** the commit graph is drawn in increments as each history page arrives, rather than after the whole history has loaded.
- **Colored branches:** branches and their connections are drawn in different colors, so they are easier to follow.
- **Cleaner history view:** the history view tooltips are gone, and the ref badges replace them.
- **Context-aware actions:** while `*` is selected, actions that need a single branch are disabled in the shortcuts bar and the history context menu. These are Push, Pull, Rename branch, Reset to commit and the Git Flow finish actions. Fetch All stays available.

### Fixes
- Fixed graph arcs being drawn inverted.
- Fixed a join between two branches where the right-hand branch wasn't connected to the join.
- Fixed junctions where the two arcs weren't merged into one: the arc from middle left to middle top and the line from middle right now join at the middle top.


## 1.0.35

### New

- **Squash & merge**: You can now squash and merge branches directly from Sourcerer. (#619, #620)

### Improved

- Added icons to various buttons and menu items for easier navigation and a more polished look. (#621)

### Fixed

- No fixes in this release.


## 1.0.34

Sourcerer 1.0.34 brings a richer stash viewer, icons for section headers and a few polish fixes.

### New
* **Stash notes:** you can now attach a note to a stash, so it is easier to remember what each one contains.
* **More details in the stash viewer:** the viewer shows more information about the selected stash.
* **Icons in section headers:** headers now have icons, matching the rest of the interface.
* **Settings icon:** the Settings entry now has an icon, shown before the Profile manager icon.

### Improved
* The Home tab no longer has a close button, so it can't be closed by accident.
* When a repository has no history, the history headers are hidden instead of showing an empty table.

### Fixed
* Fixed Rebase being shown while a repository was still loading.
* Fixed the loading spinner shaking at the final stage of loading.
