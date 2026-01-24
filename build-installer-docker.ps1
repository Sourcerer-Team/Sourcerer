# Docker-based Linux installer builder for Sourcerer (PowerShell)
# This script runs on Windows to build Linux installer and/or AppImage using Docker
# Usage:
#   .\build-installer-docker.ps1                    - Build Qt IFW installer only
#   .\build-installer-docker.ps1 -AppImage          - Build AppImage only
#   .\build-installer-docker.ps1 -Both              - Build both formats
#   .\build-installer-docker.ps1 -Rebuild           - Rebuild Docker image first

param(
    [switch]$Rebuild,
    [switch]$AppImage,
    [switch]$Both
)

$ErrorActionPreference = "Stop"

# Determine build mode
$BuildMode = ""
if ($AppImage) {
    $BuildMode = "--appimage"
    $BuildDescription = "AppImage"
} elseif ($Both) {
    $BuildMode = "--both"
    $BuildDescription = "Qt IFW installer and AppImage"
} else {
    $BuildMode = ""
    $BuildDescription = "Qt IFW installer"
}

# Script directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = $ScriptDir
$DockerDir = Join-Path $ProjectRoot "docker"

# Docker image name
$ImageName = "sourcerer-linux-builder"
$ContainerName = "sourcerer-build-$(Get-Random)"

Write-Host "=== Sourcerer Linux Installer Builder (Docker) ===" -ForegroundColor Blue
Write-Host "Build mode: $BuildDescription" -ForegroundColor Cyan

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

# Build Docker image if it doesn't exist or if -Rebuild flag is passed
$imageExists = docker image inspect $ImageName 2>&1 | Out-Null; $?
if ($Rebuild -or -not $imageExists) {
    Write-Host "Building Docker image..." -ForegroundColor Blue
    
    # Prompt for Qt account credentials
    Write-Host "Qt account credentials are required for downloading Qt components." -ForegroundColor Blue
    $QtEmail = Read-Host "Qt Account Email"
    $QtPasswordSecure = Read-Host "Qt Account Password" -AsSecureString
    $QtPassword = [System.Runtime.InteropServices.Marshal]::PtrToStringAuto(
        [System.Runtime.InteropServices.Marshal]::SecureStringToBSTR($QtPasswordSecure))
    
    if ([string]::IsNullOrWhiteSpace($QtEmail) -or [string]::IsNullOrWhiteSpace($QtPassword)) {
        Write-Host "Error: Qt account credentials are required." -ForegroundColor Red
        exit 1
    }
    
    docker build -t $ImageName `
        --build-arg QT_ACCOUNT_EMAIL="$QtEmail" `
        --build-arg QT_ACCOUNT_PASSWORD="$QtPassword" `
        -f "$DockerDir/Dockerfile" $DockerDir
    
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

# Convert Windows path to Unix path for Docker volume mount
$UnixProjectRoot = $ProjectRoot -replace '\\', '/' -replace '^([A-Z]):', { "/$($_.Groups[1].Value.ToLower())" }

# Get current user ID (note: on Windows with Docker Desktop, this uses the default Linux user in the container)
# We use a common UID that works with Docker Desktop's file sharing
$UserId = "1000:1000"

# Run the build in Docker container as user 1000:1000 (standard Docker Desktop user)
docker run --rm `
    --name $ContainerName `
    --user $UserId `
    -v "${UnixProjectRoot}:/workspace:z" `
    -w /workspace `
    $ImageName `
    bash -c @"
        set -e
        echo '=== Building Installer ==='
        cd /workspace/installer
        
        # Run the build-installer.sh script with appropriate flags
        bash ./build-installer.sh $BuildMode
        
        echo '=== Build Complete ==='
        ls -lh Sourcerer-*-Linux-Installer* Sourcerer-*-x86_64.AppImage 2>/dev/null || echo 'Build artifacts:'
        ls -lh Sourcerer-* 2>/dev/null || echo 'No build artifacts found'
"@

if ($LASTEXITCODE -ne 0) {
    Write-Host "=== Build failed ===" -ForegroundColor Red
    exit 1
}

# Check what was created and display results
Write-Host ""
Write-Host "=== Checking build artifacts ===" -ForegroundColor Blue

$installers = @()
$appimages = @()

# Check for Qt IFW installers
$installerPattern = "Sourcerer-*-Linux-Installer*"
$installerFiles = Get-ChildItem -Path $InstallerDir -Filter $installerPattern -ErrorAction SilentlyContinue
if ($installerFiles) {
    $installers += $installerFiles
}

# Check for AppImages
$appimagePattern = "Sourcerer-*-x86_64.AppImage"
$appimageFiles = Get-ChildItem -Path $InstallerDir -Filter $appimagePattern -ErrorAction SilentlyContinue
if ($appimageFiles) {
    $appimages += $appimageFiles
}

# Display results
if ($installers.Count -gt 0 -or $appimages.Count -gt 0) {
    Write-Host "=== Success! ===" -ForegroundColor Green
    Write-Host ""
    
    if ($installers.Count -gt 0) {
        Write-Host "Qt IFW Installer(s):" -ForegroundColor Green
        $installers | ForEach-Object { 
            $sizeMB = [math]::Round($_.Length/1MB, 2)
            Write-Host "  $($_.Name) ($sizeMB MB)" -ForegroundColor White
            Write-Host "  Path: $($_.FullName)" -ForegroundColor Gray
        }
        Write-Host ""
    }
    
    if ($appimages.Count -gt 0) {
        Write-Host "AppImage(s):" -ForegroundColor Green
        $appimages | ForEach-Object { 
            $sizeMB = [math]::Round($_.Length/1MB, 2)
            Write-Host "  $($_.Name) ($sizeMB MB)" -ForegroundColor White
            Write-Host "  Path: $($_.FullName)" -ForegroundColor Gray
        }
        Write-Host ""
        Write-Host "To use the AppImage on Linux:" -ForegroundColor Cyan
        Write-Host "  chmod +x $($appimages[0].Name)" -ForegroundColor White
        Write-Host "  ./$($appimages[0].Name)" -ForegroundColor White
    }
} else {
    Write-Host "=== Build failed - no artifacts found ===" -ForegroundColor Red
    Write-Host "Expected to find files matching:" -ForegroundColor Yellow
    Write-Host "  - $installerPattern" -ForegroundColor Yellow
    Write-Host "  - $appimagePattern" -ForegroundColor Yellow
    exit 1
}
