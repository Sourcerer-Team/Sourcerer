# Docker-based Linux Installer Builder

This directory contains Docker configuration for building the Sourcerer Linux installer in a containerized environment.

## Overview

The Docker setup provides a consistent build environment with all necessary dependencies pre-installed:
- Ubuntu 24.04 base
- Qt6 development libraries
- Qt Installer Framework 4.10
- Build tools (gcc, g++, cmake)
- Required libraries (libgit2, libzip, libssl, libssh2, etc.)

## Files

- `Dockerfile` - Main Dockerfile for the build environment
- `Dockerfile.linux-builder` - Alternative Dockerfile using Qt online installer (for reference)

## Usage

### On Linux/macOS:

```bash
# Build and run (from project root)
./build-installer-docker.sh

# Rebuild Docker image and build installer
./build-installer-docker.sh --rebuild
```

### On Windows (PowerShell):

```powershell
# Build and run (from project root)
.\build-installer-docker.ps1

# Rebuild Docker image and build installer
.\build-installer-docker.ps1 -Rebuild
```

## Prerequisites

- Docker installed and running
- Linux: Docker Engine
- Windows: Docker Desktop for Windows
- macOS: Docker Desktop for Mac

## What It Does

1. Builds a Docker image with all dependencies (first run only, or with --rebuild)
2. Mounts the project directory into the container
3. Builds Sourcerer in Release mode
4. Runs the build-installer.sh script
5. Outputs the installer in the `installer/` directory

## Output

The installer will be created as:
- `installer/Sourcerer-X.Y.Z-Linux-Installer`

Where X.Y.Z is the version from `installer/config/config.xml`

## Troubleshooting

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
```bash
./build-installer-docker.sh --rebuild
```

### Qt Installer Framework download fails
The Dockerfile will try multiple methods to install Qt IFW. If all fail, you may need to manually download and adjust the Dockerfile.

## Customization

### Changing Qt Version
Edit `Dockerfile` and modify the package versions:
```dockerfile
qt6-base-dev \
qt6-tools-dev \
```

### Adding Dependencies
Add packages to the `apt-get install` command in the Dockerfile:
```dockerfile
RUN apt-get update && apt-get install -y \
    your-package-here \
    && rm -rf /var/lib/apt/lists/*
```

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
