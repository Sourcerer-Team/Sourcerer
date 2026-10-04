# Sourcerer

Sourcerer is a Qt-based graphical Git client for Windows, Linux and macOS. It provides a desktop UI for common repository tasks such as browsing history, viewing diffs, managing branches, remotes, stashes, tags, submodules, and working with credentials and SSH keys.

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
- `installer/` - Windows, Linux and macOS packaging assets and release scripts
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

### Building on macOS

Requirements: Xcode command line tools, Qt 6 for macOS (the official kit is universal), CMake and Ninja.

```bash
cmake -S src -B build/Release-macOS -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=$HOME/Qt/6.10.3/macos \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3)
cmake --build build/Release-macOS
```

- Always pass `CMAKE_OSX_DEPLOYMENT_TARGET`; without it the app requires the macOS version of the build machine. It must not be lower than what your Qt version supports (macOS 13 for Qt 6.10).
- Homebrew OpenSSL is arm64-only and dynamically linked. That is fine for local development; release builds use a static universal OpenSSL (see below).
- The result is `Sourcerer.app` with `SourcererLauncher` inside `Contents/MacOS` (used by the updater).
- A frameless custom title bar is used on all platforms.

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
3. Ensure Qt and OpenSSL development files plus runtime dependencies are available in the expected locations.
4. Run `installer/build-installer-wix.bat`.
5. The script gathers the built executable, dependencies, keys, and license files, then compiles and links the MSI.

The Windows MSI installs for the current user under that user's profile instead of system-wide.

Example:

```bat
cd installer
build-installer-wix.bat
```

## Packaging notes

### macOS release

- `installer/macos/build-macos.sh` - builds a universal (arm64 + x86_64) `Sourcerer.app`, runs `macdeployqt`, signs it and creates `installer/Sourcerer-Installer.dmg`
- `installer/macos/build-openssl-universal.sh` - builds the static universal OpenSSL used by the release build
- `installer/macos/entitlements.plist` - hardened runtime entitlements (QtWebEngine needs JIT), used with a Developer ID
- `installer/macos/test-updater.sh` - end-to-end test of the updater with a fake update DMG
- `.github/workflows/macos-dmg-release.yml` - CI build, publishes the DMG to the GitHub release `v<version>`

```bash
QT_PREFIX=$HOME/Qt/6.10.3/macos installer/macos/build-macos.sh
```

#### Signing and Gatekeeper

Without an Apple Developer account the app is only **ad-hoc signed** and not notarized. macOS Gatekeeper blocks it on first launch for users who downloaded it with a browser:

1. Open the DMG and drag `Sourcerer` to `Applications`.
2. Try to open it once (it will be refused), then open **System Settings → Privacy & Security** and click **Open Anyway** next to the Sourcerer message. (Right-click → Open no longer works on current macOS versions.)

To ship a Developer ID signed and notarized build set `CODESIGN_IDENTITY="Developer ID Application: ..."`, `NOTARIZE=1`, `APPLE_ID`, `APPLE_TEAM_ID` and `APPLE_APP_PASSWORD` for the script. In CI the same is enabled by adding the repository secrets `MACOS_CERTIFICATE` (base64 `.p12`), `MACOS_CERTIFICATE_PASSWORD`, `MACOS_CODESIGN_IDENTITY`, `APPLE_ID`, `APPLE_TEAM_ID` and `APPLE_APP_PASSWORD`.

#### Automatic updates on macOS

The update feed item provides `download_url_macos`, the https URL of a `.dmg` containing `Sourcerer.app`. The application downloads it to `~/Library/Application Support/Sourcerer/updates` and on the next start `SourcererLauncher` verifies and mounts the image, checks the contained app (bundle identifier, newer version, supported CPU and macOS version, code signature, and - once the installed app has a Developer ID - the same Team ID), replaces the installed bundle and starts it again. If the application folder is not writable by the current user (for example a standard user with the app in `/Applications`) the replacement is done by the launcher as root after the macOS administrator password prompt. There is no `download_sha256_macos`: integrity is provided by the code signature checks.

Updates are not installed automatically when the app runs from a disk image or from a Gatekeeper-translocated location; the news toast then offers the manual download.

## Other packaging notes

- The Linux scripts bundle Qt plugins and shared libraries needed by the application at runtime.
- The Windows installer uses WiX and `windeployqt` to deploy Qt and OpenSSL runtime files; the Docker build image also installs the OpenSSL development headers and libraries needed for compilation.
- Release packaging expects the app to be built first; the scripts do not replace the application build step.

## Resources and assets

Application icons and UI assets live in:

- `component/GuiManager/resources/icons/`
- `component/GuiManager/resources/resources.qrc`

## Contributing

If you change UI resources, component wiring, or packaging behavior, update the corresponding CMake files and installer scripts so the release artifacts stay consistent.
