# Sourcerer

Sourcerer is a Qt-based graphical Git client for Windows and Linux. It provides a desktop UI for common repository tasks such as browsing history, viewing diffs, managing branches, remotes, stashes, tags, submodules, and working with credentials and SSH keys.

## Features

- Repository overview and recent repositories list
- Commit history and tree views
- Diff and merge-diff inspection
- Branch, remote, tag, stash, and submodule management
- Git credential and SSH key handling
- Embedded terminal console
- Configurable UI theme and application settings

## Repository layout

The project is split into several top-level directories:

- `src/` - application entry point and top-level Qt CMake project
- `component/` - reusable application components
- `installer/` - Windows and Linux packaging assets and release scripts
- `cmake/` - shared CMake helper modules
- `lib/` - bundled third-party libraries and utility code
- `tests/` - automated tests
- `docs/` - supporting documentation
- `docker/` - Docker-based build and packaging helpers
- `keys/` - security and signing-related files used by packaging

## Source code structure

### Application entry point

- `src/main.cpp` - application startup and Qt initialization
- `src/CMakeLists.txt` - root CMake target that assembles the final executable

### Core components

Each major area of the application lives in its own component under `component/`:

- `component/ComponentManager/` - component lifecycle and coordination
- `component/ConfigManager/` - application configuration storage and retrieval
- `component/EventManager/` - event dispatching and signal coordination
- `component/GitRepoManager/` - Git repository operations and domain logic
- `component/GuiManager/` - all user interface widgets, dialogs, models, and resources
- `component/StylesheetManager/` - application theme and stylesheet handling

### GUI manager

`component/GuiManager/` is the largest component and contains most of the visible application:

- `include/` - widget, model, dialog, and helper declarations
- `src/` - widget and dialog implementations
- `resources/` - icons, Qt resource files, and other packaged assets
- `translations/` - translation source files

Notable GUI subareas include:

- `src/widgets/` - title bar, console, diff views, overlays, toasts, and editors
- `src/containers/` - main UI containers for Git and workspace workflows
- `src/models/` - Qt item models used by views and tables
- `src/dialogs/` - application dialogs such as About and settings-related dialogs

### Shared libraries

The `lib/` tree contains shared code used across components, including utilities and UI helpers. These are included from the top-level CMake files through shared helper functions in `cmake/helpers.cmake`.

## Build prerequisites

Sourcerer uses:

- CMake
- Qt 6
- libgit2
- libzip
- A C++17-capable compiler

Platform-specific dependencies may also be required for packaging and release creation.

## Building from source

The main application is configured from `src/`.

Typical build flow:

```bash
cmake -S src -B build/Release -DCMAKE_BUILD_TYPE=Release
cmake --build build/Release --config Release
```

The exact generator and toolchain depend on your platform and Qt installation.

## Release packaging

Release artifacts are created separately from the application build.

### Linux release

Linux packaging is driven by the scripts in `installer/`:

- `installer/build-installer.sh` - builds the release binary, prepares the installer payload, and can generate an AppImage
- `installer/build-appimage.sh` - constructs the AppImage layout from the built release output
- `installer/CMakeLists.txt` - CMake-based installer target used during packaging

#### Linux release steps

1. Build the application in release mode.
2. Run the installer script from `installer/`.
3. The script collects the executable, required Qt plugins, shared libraries, icon assets, and configuration files.
4. An AppImage and/or Qt Installer Framework package is produced depending on the script options and environment.

Example:

```bash
cd installer
./build-installer.sh --both
```

The script expects a release build at `build/Release/` relative to the repository root.

### Windows release

Windows packaging is also handled from `installer/`:

- `installer/build-installer-wix.bat` - Windows release packaging script using WiX
- `installer/Sourcerer.wxs` - WiX source file for the per-user MSI
- `installer/LICENSE.rtf` - installer license text
- `installer/Sourcerer-Installer.msi` - generated MSI artifact

#### Windows release steps

1. Build the application in release mode.
2. Install WiX Toolset v3.11 or later.
3. Ensure Qt and MSYS2 runtime dependencies are available in the expected locations.
4. Run `installer/build-installer-wix.bat`.
5. The script gathers the built executable, dependencies, keys, and license files, then compiles and links the MSI.

The Windows MSI installs for the current user under that user's profile instead of system-wide.

Example:

```bat
cd installer
build-installer-wix.bat
```

## Packaging notes

- The Linux scripts bundle Qt plugins and shared libraries needed by the application at runtime.
- The Windows installer uses WiX and may also use `windeployqt` when available to deploy Qt runtime files.
- Release packaging expects the app to be built first; the scripts do not replace the application build step.

## Resources and assets

Application icons and UI assets live in:

- `component/GuiManager/resources/icons/`
- `component/GuiManager/resources/resources.qrc`

## Contributing

If you change UI resources, component wiring, or packaging behavior, update the corresponding CMake files and installer scripts so the release artifacts stay consistent.
