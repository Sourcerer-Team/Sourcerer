# Docker-based Windows installer builder for Sourcerer (PowerShell)
# This script runs on Windows to build Windows installer using Docker Windows containers
# Usage:
#   .\build-windows-docker.ps1           - Build Windows installer
#   .\build-windows-docker.ps1 -Rebuild  - Rebuild Docker image first

param(
    [switch]$Rebuild
)

# Support GNU-style --arguments
foreach ($arg in $args) {
    if ($arg -eq "--rebuild") { $Rebuild = $true }
}

$ErrorActionPreference = "Stop"

# Script directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = $ScriptDir
$DockerDir = Join-Path $ProjectRoot "docker"

# Docker image name
$ImageName = "sourcerer-windows-builder"
$ContainerName = "sourcerer-windows-build-$(Get-Random)"

Write-Host "=== Sourcerer Windows Installer Builder (Docker) ===" -ForegroundColor Blue

# Check if Docker is installed
try {
    $null = docker --version
} catch {
    Write-Host "Error: Docker is not installed. Please install Docker Desktop for Windows." -ForegroundColor Red
    exit 1
}

# Check if Docker daemon is running
try {
    $null = docker info 2>&1
} catch {
    Write-Host "Error: Docker daemon is not running. Please start Docker Desktop." -ForegroundColor Red
    exit 1
}

# Check if Windows containers are enabled
$dockerInfo = docker info 2>&1 | Out-String
if ($dockerInfo -notmatch "OSType: windows") {
    Write-Host "Error: Docker is not in Windows containers mode." -ForegroundColor Red
    Write-Host "Please switch Docker Desktop to Windows containers:" -ForegroundColor Yellow
    Write-Host "  1. Right-click Docker Desktop system tray icon" -ForegroundColor Yellow
    Write-Host "  2. Select 'Switch to Windows containers...'" -ForegroundColor Yellow
    Write-Host "  3. Wait for Docker to restart" -ForegroundColor Yellow
    Write-Host "  4. Run this script again" -ForegroundColor Yellow
    exit 1
}

# Build Docker image if it doesn't exist or if -Rebuild flag is passed
$imageExists = $false
try {
    $null = docker image inspect $ImageName 2>$null
    $imageExists = $LASTEXITCODE -eq 0
} catch {
    $imageExists = $false
}

if ($Rebuild -or -not $imageExists) {
    if (-not $imageExists) {
        Write-Host "Docker image '$ImageName' not found. Building it now..." -ForegroundColor Yellow
    }
    Write-Host "Building Docker image (this may take 30-60 minutes)..." -ForegroundColor Blue
    Write-Host "Note: Using aqtinstall to download Qt (no account credentials required)" -ForegroundColor Green
    Write-Host "Note: Using WiX Toolset for MSI generation" -ForegroundColor Green
    
    docker build -t $ImageName -f "$DockerDir/Dockerfile.windows" $DockerDir
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Failed to build Docker image" -ForegroundColor Red
        exit 1
    }
    Write-Host "Docker image built successfully." -ForegroundColor Green
}

# Create build output directory
$InstallerDir = Join-Path $ProjectRoot "installer"
if (-not (Test-Path $InstallerDir)) {
    New-Item -ItemType Directory -Path $InstallerDir | Out-Null
}

Write-Host "Running build in Docker container..." -ForegroundColor Blue

# Run the complete build process in Docker container (build project + create installer)
docker run --rm `
    --name $ContainerName `
    -v "${ProjectRoot}:C:\workspace" `
    -w "C:\workspace" `
    $ImageName `
    powershell -Command @"
    Write-Host 'Building Sourcerer project...' -ForegroundColor Blue

    # Create build directory if it doesn't exist
    if (-not (Test-Path 'C:\workspace\build\Release')) {
        New-Item -ItemType Directory -Path 'C:\workspace\build\Release' -Force | Out-Null
    }
    
    # Set environment variables for CMake build
    `$env:MSYS64_DIR = 'C:\tools\msys64'
    `$env:LIBGIT2_INCLUDEDIR = 'C:\tools\msys64\mingw64\include'
    `$env:LIBGIT2_LIBDIR = 'C:\tools\msys64\mingw64\lib'
    `$env:LIBZIP_INCLUDEDIR = 'C:\tools\msys64\mingw64\include'
    `$env:LIBZIP_LIBDIR = 'C:\tools\msys64\mingw64\lib'
    `$env:OPENSSL_ROOT_DIR = 'C:\tools\msys64\mingw64'
    `$env:CMAKE_PREFIX_PATH = 'C:\tools\msys64\mingw64'
    
    Write-Host 'Environment variables set:' -ForegroundColor Green
    Write-Host "  LIBGIT2_INCLUDEDIR = `$(`$env:LIBGIT2_INCLUDEDIR)" -ForegroundColor Gray
    Write-Host "  LIBGIT2_LIBDIR = `$(`$env:LIBGIT2_LIBDIR)" -ForegroundColor Gray
    Write-Host "  LIBZIP_INCLUDEDIR = `$(`$env:LIBZIP_INCLUDEDIR)" -ForegroundColor Gray
    Write-Host "  LIBZIP_LIBDIR = `$(`$env:LIBZIP_LIBDIR)" -ForegroundColor Gray
    Write-Host "  OPENSSL_ROOT_DIR = `$(`$env:OPENSSL_ROOT_DIR)" -ForegroundColor Gray
    Write-Host "  CMAKE_PREFIX_PATH = `$(`$env:CMAKE_PREFIX_PATH)" -ForegroundColor Gray
    
    # Use MSYS2 bash for building with proper pthread/compiler compatibility
    Write-Host 'Building via MSYS2 bash shell...' -ForegroundColor Blue
    `$msys2Bash = 'C:\tools\msys64\usr\bin\bash.exe'
    
    # Verify ninja is available before building
    Write-Host 'Verifying ninja is available...' -ForegroundColor Gray
    & `$msys2Bash -lc 'export PATH=/c/tools/msys64/mingw64/bin:/usr/local/bin:/usr/bin:/bin:/mingw64/bin && which ninja && ninja --version'
    if (`$LASTEXITCODE -ne 0) {
        Write-Host 'ERROR: ninja not found in MSYS2 environment' -ForegroundColor Red
        Write-Host 'Attempting to list mingw64/bin contents...' -ForegroundColor Gray
        & `$msys2Bash -lc 'ls -la /c/tools/msys64/mingw64/bin | grep -i ninja || echo "ninja not found in directory"'
        exit 1
    }
    
    & `$msys2Bash -lc 'export PATH=/c/tools/msys64/mingw64/bin:/usr/local/bin:/usr/bin:/bin:/mingw64/bin && cd /c/workspace/build/Release && cmake ../../src -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=/mingw64/bin/ninja && ninja -j4'
    
    if (`$LASTEXITCODE -ne 0) {
        Write-Host 'Project build failed' -ForegroundColor Red
        exit 1
    }
    
    Write-Host 'Project built successfully' -ForegroundColor Green
    
    # Copy MSYS2 runtime libraries to Release directory
    Write-Host 'Copying MSYS2 runtime libraries...' -ForegroundColor Blue
    
    # Copy specific runtime DLLs
    `$msys2RuntimeDlls = @(
        'C:\tools\msys64\mingw64\bin\libstdc++-6.dll',
        'C:\tools\msys64\mingw64\bin\libgcc_s_seh-1.dll',
        'C:\tools\msys64\mingw64\bin\libwinpthread-1.dll'
    )
    foreach (`$dll in `$msys2RuntimeDlls) {
        if (Test-Path `$dll) {
            Copy-Item `$dll 'C:\workspace\build\Release\' -Force
            Write-Host "  Copied $(Split-Path `$dll -Leaf)" -ForegroundColor Gray
        }
    }
    
    # Find and copy OpenSSL libraries
    Write-Host 'Finding OpenSSL libraries...' -ForegroundColor Blue
    `$openssl_bin = 'C:\tools\msys64\mingw64\bin'
    `$openssl_dlls = @()
    `$openssl_dlls += @(Get-ChildItem `$openssl_bin -Filter 'libssl*.dll' -ErrorAction SilentlyContinue)
    `$openssl_dlls += @(Get-ChildItem `$openssl_bin -Filter 'libcrypto*.dll' -ErrorAction SilentlyContinue)
    
    if (`$openssl_dlls.Count -eq 0) {
        Write-Host 'WARNING: OpenSSL DLL files not found in C:\tools\msys64\mingw64\bin' -ForegroundColor Yellow
        Write-Host 'Available DLL files containing "ssl" or "crypto":' -ForegroundColor Yellow
        Get-ChildItem `$openssl_bin -Filter '*.dll' -ErrorAction SilentlyContinue | Where-Object { `$_.Name -like '*ssl*' -or `$_.Name -like '*crypto*' } | ForEach-Object {
            Write-Host "  - $($_.Name)" -ForegroundColor Yellow
        }
        Write-Host 'First 30 available DLL files:' -ForegroundColor Yellow
        Get-ChildItem `$openssl_bin -Filter '*.dll' | Select-Object -First 30 | ForEach-Object {
            Write-Host "  - $($_.Name)" -ForegroundColor Yellow
        }
    } else {
        foreach (`$dll in `$openssl_dlls) {
            Copy-Item `$dll.FullName 'C:\workspace\build\Release\' -Force
            Write-Host "  Copied $($dll.Name)" -ForegroundColor Gray
        }
    }
    
    # Copy Qt libraries if they exist in mingw64/bin
    Write-Host 'Copying Qt libraries...' -ForegroundColor Blue
    Get-ChildItem `$openssl_bin -Filter 'Qt6*.dll' -ErrorAction SilentlyContinue | ForEach-Object {
        Copy-Item `$_.FullName 'C:\workspace\build\Release\' -Force
        Write-Host "  Copied $($_.Name)" -ForegroundColor Gray
    }
    
    # Copy libzip and libgit2 libraries
    Write-Host 'Copying dependency libraries...' -ForegroundColor Blue
    Get-ChildItem `$openssl_bin -Filter 'libzip*.dll' -ErrorAction SilentlyContinue | ForEach-Object {
        Copy-Item `$_.FullName 'C:\workspace\build\Release\' -Force
        Write-Host "  Copied $($_.Name)" -ForegroundColor Gray
    }
    Get-ChildItem `$openssl_bin -Filter 'libgit2*.dll' -ErrorAction SilentlyContinue | ForEach-Object {
        Copy-Item `$_.FullName 'C:\workspace\build\Release\' -Force
        Write-Host "  Copied $($_.Name)" -ForegroundColor Gray
    }
    
    # List what was actually copied to Release directory
    Write-Host 'DLL files in Release directory:' -ForegroundColor Blue
    Get-ChildItem 'C:\workspace\build\Release\' -Filter '*.dll' -ErrorAction SilentlyContinue | ForEach-Object {
        Write-Host "  - $($_.Name)" -ForegroundColor Gray
    }
       
    Write-Host 'Creating installer...' -ForegroundColor Blue
    
    # Run installer build script
    Set-Location C:\workspace\installer
    .\build-installer-wix.bat
    if (`$LASTEXITCODE -ne 0) {
        Write-Host 'Installer build failed' -ForegroundColor Red
        exit 1
    }
    
    Write-Host 'Installer created successfully' -ForegroundColor Green
"@

if ($LASTEXITCODE -ne 0) {
    Write-Host "=== Build failed ===" -ForegroundColor Red
    exit 1
}

# Check what was created and display results
Write-Host ""
Write-Host "=== Checking build artifacts ===" -ForegroundColor Blue

$installers = @()

# Check for Windows MSI installers
$msiPattern = "Sourcerer-*.msi"
$msiFiles = Get-ChildItem -Path $InstallerDir -Filter $msiPattern -ErrorAction SilentlyContinue
if ($msiFiles) {
    $installers += $msiFiles
}

# Display results
if ($installers.Count -gt 0) {
    Write-Host "=== Success! ===" -ForegroundColor Green
    Write-Host ""
    Write-Host "Windows Installer(s):" -ForegroundColor Green
    $installers | ForEach-Object { 
        $sizeMB = [math]::Round($_.Length/1MB, 2)
        Write-Host "  $($_.Name) ($sizeMB MB)" -ForegroundColor White
        Write-Host "  Path: $($_.FullName)" -ForegroundColor Gray
    }
    Write-Host ""
} else {
    Write-Host "=== Build failed - no artifacts found ===" -ForegroundColor Red
    Write-Host "Expected to find files matching:" -ForegroundColor Yellow
    Write-Host "  - $msiPattern" -ForegroundColor Yellow
    exit 1
}
