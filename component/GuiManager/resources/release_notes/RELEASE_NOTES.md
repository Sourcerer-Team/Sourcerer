

## 1.0.36

Sourcerer 1.0.36 reworks how the commit history is loaded and drawn. It is faster on large repositories and easier to read.

### New
- **Paged history loading:** repository history now loads page by page instead of all at once, so large repositories open faster and stay responsive. ([#626](https://github.com/Sourcerer-Team/Sourcerer/pull/626))
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
