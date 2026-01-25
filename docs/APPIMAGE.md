# AppImage Support for Sourcerer

## Overview

Sourcerer now supports building AppImage packages in addition to Qt Installer Framework installers. AppImages are portable, self-contained application bundles that run on most Linux distributions without installation.

## Benefits of AppImage

- **No installation required** - Just download, make executable, and run
- **No root privileges needed** - Runs from user's home directory
- **Cross-distribution** - Works on most modern Linux distributions
- **Self-contained** - All dependencies bundled
- **Portable** - Can run from USB drives or network shares
- **Clean removal** - Just delete the AppImage file

## Building AppImages

### Quick Start

```bash
# Build AppImage only
cd installer
./build-installer.sh --appimage

# Build both Qt IFW installer and AppImage
./build-installer.sh --both

# Build Qt IFW installer only (default)
./build-installer.sh
```

### Using the standalone script

You can also build AppImages directly:

```bash
cd installer
./build-appimage.sh
```

### Requirements

- Sourcerer must be built first (`src/build/Installer/Sourcerer` must exist)
- `wget` (for downloading appimagetool)
- `ldd` (for dependency detection)
- Internet connection (first time only, to download appimagetool)

## Using AppImages

### Running the AppImage

```bash
# Make executable (first time only)
chmod +x Sourcerer-*.AppImage

# Run it
./Sourcerer-*.AppImage
```

On first run with a desktop environment, the AppImage will automatically prompt you to integrate it with your desktop. This prompt uses `zenity` (GNOME) or `kdialog` (KDE) if available.

### Desktop Integration

#### Automatic Integration (First Run)

When you run the AppImage for the first time, it will ask if you want to integrate with your desktop:

- **GNOME/GTK environments:** Uses `zenity` dialog
- **KDE/Qt environments:** Uses `kdialog` dialog
- **Headless/Server:** Skips automatic prompting

If you accept:
- A `.desktop` file is created in `~/.local/share/applications/`
- The application icon is installed
- Sourcerer appears in your application menu
- A flag file prevents future prompts

If you decline:
- The prompt won't appear again
- You can manually integrate later

#### Manual Integration

If you skipped the automatic prompt or want to re-integrate:

```bash
# Integrate with desktop
./Sourcerer-*.AppImage --integrate

# Remove integration
./Sourcerer-*.AppImage --unintegrate

# Show integration help
./Sourcerer-*.AppImage --help-integration
```

**Note:** The standard AppImage arguments (`--appimage-extract`, `--appimage-mount`, etc.) are also available and handled by the AppImage runtime itself.

#### Silent Mode

To run the AppImage without the integration prompt (useful for scripting):

```bash
APPIMAGE_SILENT_INSTALL=1 ./Sourcerer-*.AppImage
```

#### Integration Files

When integrated, these files are created:
- Desktop file: `~/.local/share/applications/sourcerer-Sourcerer-X.Y.Z-x86_64.AppImage.desktop`
- Icon: `~/.local/share/icons/hicolor/256x256/apps/sourcerer.png`
- Flag: `~/.local/share/applications/.sourcerer-appimage-integrated`

### Extracting the AppImage

If you want to extract and inspect the contents:

```bash
./Sourcerer-*.AppImage --appimage-extract
# Contents will be in ./squashfs-root/
```

## Docker Build Support

The Docker build scripts automatically support AppImage creation:

```bash
# Modify build-installer-docker.sh to use --appimage flag
# Or build both formats with --both
```

## Technical Details

### AppImage Structure

The AppImage contains:
- Main executable: `usr/bin/Sourcerer`
- All shared libraries: `usr/lib/`
- Qt plugins: `usr/lib/qt6/plugins/`
- Desktop file: `usr/share/applications/sourcerer.desktop`
- Icon: `usr/share/icons/hicolor/scalable/apps/sourcerer.svg`
- AppRun launcher script (sets up environment)

### Library Bundling

The build script automatically:
- Detects dependencies using `ldd`
- Copies required shared libraries
- Excludes system libraries (glibc, X11, GL, etc.)
- Includes Qt plugins (platforms, themes, image formats)
- Sets up proper library paths via AppRun

### Qt Configuration

A `qt.conf` file is included to help Qt locate plugins:
```ini
[Paths]
Plugins = ../lib/qt6/plugins
```

## Troubleshooting

### AppImage won't run

1. **Check executable permission:**
   ```bash
   chmod +x Sourcerer-*.AppImage
   ```

2. **Check FUSE support:**
   ```bash
   # AppImages require FUSE
   sudo apt-get install fuse libfuse2
   ```

3. **Extract and run directly:**
   ```bash
   ./Sourcerer-*.AppImage --appimage-extract
   ./squashfs-root/AppRun
   ```

### Missing libraries

If you get library errors, the AppImage might need additional dependencies:
- Check the build log for warnings
- Modify `build-appimage.sh` to include additional libraries
- Check the `exclude_libs` list - you may need to bundle more libraries

### Qt plugin errors

If Qt can't find plugins:
- Verify `usr/lib/qt6/plugins/` contains the necessary plugins
- Check that `qt.conf` is present next to the binary
- Set `QT_DEBUG_PLUGINS=1` environment variable for detailed plugin loading info:
  ```bash
  QT_DEBUG_PLUGINS=1 ./Sourcerer-*.AppImage
  ```

## Distribution

AppImages can be distributed via:
- GitHub Releases (attach the .AppImage file)
- Direct download from website
- Package managers (AppImageHub, AppImage Pool)

## Further Reading

- [AppImage Documentation](https://docs.appimage.org/)
- [AppImage Best Practices](https://github.com/AppImage/AppImageKit/wiki/AppImage-Best-Practices)
- [appimagetool](https://github.com/AppImage/AppImageKit)
