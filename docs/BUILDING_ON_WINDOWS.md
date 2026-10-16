# Building Sourcerer on Windows using Docker

This guide explains how to build Sourcerer Linux installers and AppImages from Windows using Docker Desktop.

## Prerequisites

1. **Docker Desktop for Windows**
   - Download from: https://www.docker.com/products/docker-desktop
   - Ensure Docker is running (check system tray icon)

2. **PowerShell**
   - Windows PowerShell 5.1+ or PowerShell 7+
   - Comes pre-installed on Windows 10/11

3. **Qt Account** (for first-time Docker image build)
   - Free account at: https://login.qt.io/register
   - Required for downloading Qt components

## Quick Start

Open PowerShell in the project root directory:

```powershell
# Build Qt IFW installer only (default)
.\build-installer-docker.ps1

# Build AppImage only
.\build-installer-docker.ps1 -AppImage

# Build both Qt IFW installer and AppImage
.\build-installer-docker.ps1 -Both

# Rebuild Docker image (if Dockerfile changed)
.\build-installer-docker.ps1 -Rebuild

# Rebuild and build both formats
.\build-installer-docker.ps1 -Rebuild -Both
```

## First Run

On first run, or when using `-Rebuild`, you'll be prompted for Qt credentials:

```
Building Docker image...
Qt account credentials are required for downloading Qt components.
Qt Account Email: your.email@example.com
Qt Account Password: ********
```

The Docker image build takes 10-20 minutes on first run. Subsequent builds use the cached image.

## Build Output

Build artifacts are created in the `installer/` directory:

### Qt IFW Installer
- File: `Sourcerer-X.Y.Z-Linux-Installer.run`
- Format: Self-extracting installer
- Usage on Linux:
  ```bash
  chmod +x Sourcerer-X.Y.Z-Linux-Installer.run
  ./Sourcerer-X.Y.Z-Linux-Installer.run
  ```

### AppImage
- File: `Sourcerer-X.Y.Z-x86_64.AppImage`
- Format: Portable Linux application
- Usage on Linux:
  ```bash
  chmod +x Sourcerer-X.Y.Z-x86_64.AppImage
  ./Sourcerer-X.Y.Z-x86_64.AppImage
  ```

## Command-Line Parameters

| Parameter | Description |
|-----------|-------------|
| (none) | Build Qt IFW installer only (default) |
| `-AppImage` | Build AppImage only |
| `-Both` | Build both Qt IFW installer and AppImage |
| `-Rebuild` | Force rebuild of Docker image before building |

### Examples

```powershell
# Default build (Qt IFW installer)
.\build-installer-docker.ps1

# Build AppImage for portable distribution
.\build-installer-docker.ps1 -AppImage

# Build both formats
.\build-installer-docker.ps1 -Both

# Update Docker image and build both
.\build-installer-docker.ps1 -Rebuild -Both
```

## Troubleshooting

### Docker Not Running

**Error:** `Error: Docker daemon is not running. Please start Docker Desktop.`

**Solution:** Start Docker Desktop from the Start menu or system tray

### Permission Denied

**Error:** Permission denied errors when accessing files

**Solution:** 
- Ensure Docker Desktop has access to the drive (Docker Settings → Resources → File Sharing)
- Run PowerShell as Administrator if needed

### Build Failures

**Error:** Build fails with CMake or compilation errors

**Solution:**
1. Try rebuilding the Docker image: `.\build-installer-docker.ps1 -Rebuild`
2. Check if all source files are committed to git
3. Ensure the project builds successfully on Linux first

### Network Issues

**Error:** Cannot download Qt components

**Solution:**
- Check internet connection
- Verify Qt credentials are correct
- Check if firewall/antivirus is blocking Docker

### Out of Disk Space

**Error:** Docker out of space errors

**Solution:**
```powershell
# Clean up Docker resources
docker system prune -a
```

## Advanced Configuration

### Using Different Qt Versions

Edit `docker/Dockerfile.linux` to change Qt version:
```dockerfile
ENV QT_VERSION=6.10.1
```

Then rebuild:
```powershell
.\build-installer-docker.ps1 -Rebuild
```

### Custom Build Options

To pass custom build options, modify the bash command in `build-installer-docker.ps1`:

```powershell
bash -c @"
    set -e
    cd /workspace/installer
    bash ./build-installer.sh --both --custom-option
"@
```

## Performance Tips

1. **First Build:** 10-20 minutes (downloads Qt)
2. **Subsequent Builds:** 2-5 minutes (uses cached image)
3. **SSD Recommended:** Significantly faster than HDD
4. **RAM:** Allocate at least 4GB to Docker Desktop
5. **CPU:** More cores = faster compilation

### Optimize Docker Desktop

1. Open Docker Desktop → Settings
2. Resources → Advanced
   - CPUs: Set to number of physical cores - 1
   - Memory: Set to 4-8 GB
   - Disk image size: Ensure at least 20 GB free

## File Locations

### On Windows
- Project root: `C:\path\to\Sourcerer`
- Build output: `C:\path\to\Sourcerer\installer\`

### Inside Docker Container
- Project root: `/workspace`
- Build output: `/workspace/installer/`

Files are synchronized via Docker volumes.

## CI/CD Integration

For automated builds (GitHub Actions, Azure Pipelines, etc.):

```powershell
# Set credentials as environment variables
$env:QT_EMAIL = "your.email@example.com"
$env:QT_PASSWORD = "your_password"

# Run non-interactive build
docker build -t sourcerer-linux-builder `
    --build-arg QT_ACCOUNT_EMAIL=$env:QT_EMAIL `
    --build-arg QT_ACCOUNT_PASSWORD=$env:QT_PASSWORD `
    -f docker/Dockerfile docker/

# Build artifacts
docker run --rm `
    -v "${PWD}:/workspace:z" `
    sourcerer-linux-builder `
    bash -c "cd /workspace/installer && bash ./build-installer.sh --both"
```

## Related Documentation

- [AppImage Support](installer/APPIMAGE.md) - Detailed AppImage documentation
- [Docker Build System](docker/README.md) - Docker build details
- [Linux Installation](README.md) - Main project README

## Support

For issues specific to Windows builds:
1. Check Docker Desktop logs
2. Verify PowerShell version: `$PSVersionTable.PSVersion`
3. Ensure WSL2 backend is enabled in Docker Desktop
4. Try running in Windows Terminal for better output formatting
