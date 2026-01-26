# Sourcerer Installer

This directory contains the Qt Installer Framework 4 project for creating installers for Sourcerer on Windows, Linux, and macOS.

## Prerequisites

- Qt Installer Framework 4.0 or later
  - Download from: https://download.qt.io/official_releases/qt-installer-framework/
  - Install to default location or set `QT_IFW_DIR` environment variable

### Windows-Specific Prerequisites

- **Qt 6.10.1 with MinGW**
  - Download from: https://www.qt.io/download-qt-installer
  - During installation, select:
    - Qt 6.10.1 → MinGW 64-bit
    - Developer and Designer Tools → MinGW 13.1.0 64-bit (or similar version)
    - Qt Installer Framework 4.7 or later
  - Qt MinGW provides the compiler and build tools (gcc, g++, mingw32-make)
  - Default installation: `C:\Qt\6.10.1\mingw_64` and `C:\Qt\Tools\mingw*`

- **MSYS2 x86_64** (For Runtime Libraries Only)
  
  **Option 1: Install via Chocolatey (Recommended)**
  ```cmd
  choco install -y msys2
  ```
  This installs MSYS2 to `C:\tools\msys64` which is the default path expected by `build-installer.bat`.
  
  **Option 2: Manual Installation**
  - Download from: https://www.msys2.org/
  - Install to `C:\tools\msys64` (or update paths in `build-installer.bat` if using a different location)
  - Note: The default MSYS2 installation path is `C:\msys64`, but `build-installer.bat` expects `C:\tools\msys64` to match Docker environment
  
- **MSYS2 Required Packages (Runtime Libraries Only - No Compiler)**
  
  After installing MSYS2, open MSYS2 MINGW64 shell and install the following packages:
  
  ```bash
  pacman -S mingw-w64-x86_64-libgit2
  pacman -S mingw-w64-x86_64-libzip
  pacman -S mingw-w64-x86_64-libssh2
  ```
  
  **Important:** Do NOT install `mingw-w64-x86_64-gcc` or other compiler packages from MSYS2. 
  The build system uses Qt's MinGW compiler, not MSYS2's compiler. MSYS2 is only needed for runtime libraries.
  
  These packages provide the required DLL files:
  - libgit2.dll (Git repository operations)
  - libzip.dll (ZIP archive support)
  - libssh2-1.dll (SSH2 protocol support)
  - And their dependencies: libbz2-1.dll, liblzma-5.dll, libzstd.dll, zlib1.dll

## Building the Installer

### Method 1: Using CMake (Recommended)

From the main project build directory:

```bash
# Configure and build the main project
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release

# Build the installer
cmake --build . --target installer
```

The installer will be created in the `installer/` directory.

### Method 2: Using Build Scripts

#### Windows (Local Build)

**Prerequisites:**
- Qt 6.10.1 with MinGW installed (includes compiler in `C:\Qt\Tools\mingw*`)
- MSYS2 installed at `C:\tools\msys64` with runtime libraries (see Windows-Specific Prerequisites above)
- Qt Installer Framework 4.7 or later

```cmd
cd installer
build-installer.bat
```

This will:
1. Locate and use Qt's MinGW compiler (from `C:\Qt\Tools\mingw*\bin`)
2. Build Sourcerer in Release mode using Qt MinGW
3. Copy runtime libraries from MSYS2 (`C:\tools\msys64\mingw64\bin`)
4. Deploy Qt dependencies using windeployqt
5. Create the Windows installer executable

**Note:** The build script automatically detects Qt MinGW tools. If you installed Qt to a non-standard location, update the `QT_MINGW_DIR` variable in the script.

#### Linux/macOS

```bash
cd installer
chmod +x build-installer.sh
./build-installer.sh
```

This will:
1. Build Sourcerer in Release mode
2. Deploy Qt dependencies (macdeployqt on macOS, manual ldd-based on Linux)
3. Create the installer

#### Docker (Linux builds on any platform)

For building Linux installers using Docker (works on Windows, macOS, or Linux):

**PowerShell (Windows):**
```powershell
# Build installer
.\build-installer-docker.ps1

# Rebuild Docker image and build installer
.\build-installer-docker.ps1 -Rebuild
# or
.\build-installer-docker.ps1 --rebuild

# Build AppImage instead of Qt IFW installer
.\build-installer-docker.ps1 -AppImage

# Build both installer and AppImage
.\build-installer-docker.ps1 -Both
```

**Bash (Linux/macOS):**
```bash
# Build installer
./build-installer-docker.sh

# Rebuild Docker image and build installer
./build-installer-docker.sh --rebuild
```

The Docker build will:
1. Build (or use existing) Docker image with all dependencies
2. Build Sourcerer in Release mode inside container
3. Deploy all shared libraries and Qt plugins
4. Create the Linux installer in `installer/` directory

See `docker/README.md` for more details on Docker-based builds.

## Directory Structure

```
installer/
├── config/
│   ├── config.xml           # Main installer configuration
│   └── controlscript.js     # Installer UI control script
├── packages/
│   └── com.sourcerer.app/
│       ├── meta/
│       │   ├── package.xml     # Package metadata
│       │   ├── installscript.js # Installation logic
│       │   └── license.txt     # License agreement
│       └── data/               # Application files (auto-populated during build)
├── build-installer.bat      # Windows build script
├── build-installer.sh       # Linux/macOS build script
├── CMakeLists.txt          # CMake installer target
└── README.md               # This file
```

## Customization

### Changing Installer Settings

Edit `config/config.xml` to modify:
- Application name and version
- Publisher information
- Default installation directory
- Start menu entries
- UI customization

### Modifying Installation Logic

Edit `packages/com.sourcerer.app/meta/installscript.js` to:
- Add custom installation steps
- Create shortcuts
- Register file associations
- Add post-installation actions

### License Agreement

Replace `packages/com.sourcerer.app/meta/license.txt` with your license text.

## Platform-Specific Notes

### Windows
- The installer will be named `Sourcerer-Windows-Installer.exe`
- Creates Start Menu and Desktop shortcuts
- Requires Visual C++ runtime (deployed automatically)

### macOS
- The installer will be named `Sourcerer-macOS-Installer`
- Creates .app bundle in Applications folder
- Uses macdeployqt for dependency deployment
- May need code signing for distribution

### Linux
- The installer will be named `Sourcerer-X.Y.Z-Linux-Installer.run`
- Creates desktop entry
- Includes startup script with LD_LIBRARY_PATH
- Docker build automatically includes all required shared libraries
- Manual dependency deployment may be needed for some libraries when building natively

## Troubleshooting

### Qt IFW not found
Set the `QT_IFW_DIR` environment variable to your Qt IFW installation directory:
```bash
export QT_IFW_DIR=/path/to/QtIFW-4.0.0
```

### Missing Qt dependencies
- Windows: Ensure windeployqt is in PATH
- macOS: Ensure macdeployqt is in PATH
- Linux: Use linuxdeployqt or manually copy required libraries

### Installer fails to run
- Check that the Release build completed successfully
- Verify all required DLLs/shared libraries are in the data directory
- Check installer logs in the temp directory

## Distribution

The created installer is a standalone executable that can be distributed to end users. It includes:
- The Sourcerer application
- All Qt dependencies
- Required runtime libraries
- Installation wizard
- Uninstaller (MaintenanceTool)
