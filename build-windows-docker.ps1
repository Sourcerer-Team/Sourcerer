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
        -f "$DockerDir/Dockerfile.windows" $DockerDir
    
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

# Run the build in Docker container
docker run --rm `
    --name $ContainerName `
    -v "${ProjectRoot}:C:\workspace" `
    -w "C:\workspace\installer" `
    $ImageName `
    powershell -Command "& { Set-Location C:\workspace\installer; .\build-installer.bat }"

if ($LASTEXITCODE -ne 0) {
    Write-Host "=== Build failed ===" -ForegroundColor Red
    exit 1
}

# Check what was created and display results
Write-Host ""
Write-Host "=== Checking build artifacts ===" -ForegroundColor Blue

$installers = @()

# Check for Windows installers (.exe files)
$exePattern = "Sourcerer-*.exe"
$exeFiles = Get-ChildItem -Path $InstallerDir -Filter $exePattern -ErrorAction SilentlyContinue
if ($exeFiles) {
    $installers += $exeFiles
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
    Write-Host "  - $exePattern" -ForegroundColor Yellow
    exit 1
}
