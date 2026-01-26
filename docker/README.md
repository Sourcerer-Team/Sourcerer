# Docker-based Installer Builders

This directory contains Docker configurations for building Sourcerer installers in containerized environments.

## Overview

The Docker setup provides consistent build environments with all necessary dependencies pre-installed:

### Linux Build Environment
- Ubuntu 24.04 base
- Qt 6.10.1 (gcc_64)
- Qt Installer Framework 4.10
- Build tools (gcc, g++, cmake, ninja)
- Required libraries (libgit2, libzip, libssl, libssh2, etc.)

### Windows Build Environment
- Windows Server Core 2022 base
- Qt 6.10.1 (MinGW 64-bit) - Qt framework and tools
- Qt MinGW 13.1.0 64-bit - Compiler and build tools (gcc, g++, mingw32-make)
- Qt Installer Framework 4.8
- MSYS2 (installed via Chocolatey at C:\tools\msys64) - Provides runtime libraries only (libgit2, libzip, libssh2, openssl)
- CMake, Git, 7-Zip, Python (via Chocolatey)

**Note:** The build uses Qt's MinGW compiler, not MSYS2's compiler. MSYS2 is only for runtime libraries.

## Files

- `Dockerfile.linux` - Linux build environment
- `Dockerfile.windows` - Windows build environment
- `README.md` - This file

## Usage

### Linux Installer Build

#### On Linux/macOS:

```bash
# Build and run (from project root)
./build-installer-docker.sh

# Rebuild Docker image and build installer
./build-installer-docker.sh --rebuild
```

#### On Windows (PowerShell):

```powershell
# Build Linux installer (from project root)
.\build-installer-docker.ps1

# Rebuild Docker image and build installer
.\build-installer-docker.ps1 -Rebuild
# or using GNU-style argument
.\build-installer-docker.ps1 --rebuild

# Build AppImage instead
.\build-installer-docker.ps1 -AppImage
# or
.\build-installer-docker.ps1 --appimage

# Build both installer types
.\build-installer-docker.ps1 -Both
# or
.\build-installer-docker.ps1 --both
```

**Note:** The script supports both PowerShell-style (`-Rebuild`) and GNU-style (`--rebuild`) arguments.

### Windows Installer Build

#### On Windows (PowerShell):

**Prerequisites:**
- Docker Desktop must be in **Windows containers mode**
- To switch: Right-click Docker Desktop tray icon → "Switch to Windows containers..."

```powershell
# Build Windows installer (from project root)
.\build-windows-docker.ps1

# Rebuild Docker image and build installer
.\build-windows-docker.ps1 -Rebuild
# or using GNU-style argument
.\build-windows-docker.ps1 --rebuild
```

**Note:** First build will take 30-60 minutes to download and install Qt, MSYS2, and dependencies.

## Prerequisites

### Linux Builds
- Docker installed and running
- Linux: Docker Engine
- Windows: Docker Desktop for Windows (in Linux containers mode)
- macOS: Docker Desktop for Mac

### Windows Builds
- Docker Desktop for Windows in **Windows containers mode**
- Windows 10 version 1809 or later, or Windows Server 2019 or later
- No Qt account needed (uses aqtinstall for automated Qt installation)

## What It Does

### Linux Build Process
1. Builds a Docker image with all dependencies (first run only, or with --rebuild)
2. Mounts the project directory into the container
3. Builds Sourcerer in Release mode using gcc
4. Runs the build-installer.sh script
5. Outputs the installer/AppImage in the `installer/` directory

### Windows Build Process
1. Builds a Windows Docker image with all dependencies (first run only, or with --rebuild)
   - Installs Qt 6.10.1 via aqtinstall
   - Installs Qt MinGW compiler tools (version 11.2.0)
   - Installs Qt IFW 4.7
   - Installs MSYS2 runtime libraries (not compiler)
2. Mounts the project directory into the container
3. Builds Sourcerer in Release mode using Qt MinGW compiler
4. Runs the build-installer.bat script
5. Outputs the installer in the `installer/` directory

## Output

### Linux Build
The installer will be created as:
- `installer/Sourcerer-X.Y.Z-Linux-Installer.run` (Qt IFW installer)
- `installer/Sourcerer-X.Y.Z-x86_64.AppImage` (AppImage, if --appimage or --both used)

### Windows Build
The installer will be created as:
- `installer/Sourcerer-X.Y.Z-Installer.exe`

Where X.Y.Z is the version from `installer/config/config.xml`

## Troubleshooting

### Windows Containers Mode (Windows builds only)

**Error:** `OSType: linux` detected when trying Windows build

**Solution:** Switch Docker Desktop to Windows containers mode:
1. Right-click Docker Desktop system tray icon
2. Select "Switch to Windows containers..."
3. Wait for Docker to restart
4. Run the build script again

**Note:** To switch back for Linux builds, repeat the process and select "Switch to Linux containers..."

### Permission denied: Docker socket (Linux)
**Error:** `permission denied while trying to connect to the Docker daemon socket`

**Solution:** Add your user to the docker group:
```bash
sudo usermod -aG docker $USER
```

Then you MUST log out and log back in (or reboot) for the group change to take effect.

**Quick test without logout (temporary):**
```bash
newgrp docker
```

**Verify it works:**
```bash
docker run hello-world
```

### Docker not found
Make sure Docker is installed and in your PATH.

**For openSUSE Tumbleweed:**
```bash
sudo zypper install docker
sudo systemctl enable docker
sudo systemctl start docker
```

**For Ubuntu/Debian:**
```bash
sudo apt-get install docker.io
sudo systemctl enable docker
sudo systemctl start docker
```

### Build fails
Try rebuilding the Docker image:

**Linux builds (Linux/macOS):**
```bash
./build-installer-docker.sh --rebuild
```

**Linux builds (Windows PowerShell):**
```powershell
.\build-installer-docker.ps1 -Rebuild
# or
.\build-installer-docker.ps1 --rebuild
```

**Windows builds (Windows PowerShell):**
```powershell
.\build-windows-docker.ps1 -Rebuild
# or
.\build-windows-docker.ps1 --rebuild
```

### Qt installation fails

**Issue:** Qt online installer requires account credentials

**Solution:** 
- Ensure you have a valid Qt account (free at https://login.qt.io/register)
- Enter correct email and password when prompted
- If installation still fails, try downloading Qt manually and adjusting the Dockerfile

### Slow Windows Docker image build

**Expected behavior:** First-time Windows Docker image build takes 30-60 minutes due to:
- Downloading Windows Server Core base image (~4 GB)
- Installing MSYS2 and packages
- Downloading and installing Qt 6.10.1
- Installing Qt Installer Framework

This is a one-time operation. Subsequent builds use the cached image unless you use `-Rebuild`.

### Qt Installer Framework download fails
The Dockerfile will try to install Qt IFW via the online installer. If it fails, you may need to manually download and adjust the Dockerfile.

## Customization

### Changing Qt Version

**Linux (edit `Dockerfile.linux`):**
Modify the Qt version variables:
```dockerfile
ENV QT_VERSION=6.11.0
```
And update the package IDs:
```dockerfile
qt.qt6.6110.linux_gcc_64 \
```

**Windows (edit `Dockerfile.windows`):**
Modify the Qt version variables:
```dockerfile
ENV QT_VERSION=6.11.0
```
And update the package IDs:
```dockerfile
qt.qt6.6110.win64_mingw \
```

### Adding Dependencies

**Linux (edit `Dockerfile.linux`):**
Add packages to the `apt-get install` command:
```dockerfile
RUN apt-get update && apt-get install -y \
    your-package-here \
    && rm -rf /var/lib/apt/lists/*
```

**Windows (edit `Dockerfile.windows`):**
Add MSYS2 packages:
```dockerfile
RUN C:\tools\msys64\usr\bin\bash.exe -lc 'pacman -S --noconfirm mingw-w64-x86_64-your-package'
```

Or add Chocolatey packages:
```dockerfile
RUN choco install -y your-package
```

**Note:** MSYS2 is installed via Chocolatey to `C:\tools\msys64` to ensure consistency between Docker and local builds.

## Benefits of Docker-based Builds

1. **Consistency** - Same build environment regardless of host system configuration
2. **Isolation** - Build dependencies don't interfere with host system
3. **Reproducibility** - Anyone can build with identical results
4. **Cross-platform** - Build Linux installers from Windows, and vice versa
5. **Clean environment** - Fresh build environment every time
6. **No local setup** - No need to install Qt, build tools, or dependencies on host

## Limitations

### Windows Builds
- Requires Windows 10/11 Pro, Enterprise, or Education (Hyper-V support)
- Windows containers can only run on Windows hosts
- First build is slow (30-60 minutes) due to large downloads
- Image size is large (~15-20 GB) due to Windows Server Core base

### Linux Builds
- Can run on any platform (Linux, Windows, macOS) with Docker
- Faster build times than Windows
- Smaller image size (~5-8 GB)

Then rebuild the image.

## Clean Up

Remove the Docker image:
```bash
docker rmi sourcerer-linux-builder
```

Remove all build artifacts:
```bash
rm -rf src/build installer/Sourcerer-*-Linux-Installer*
```
