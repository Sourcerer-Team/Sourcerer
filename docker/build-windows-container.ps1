$ErrorActionPreference = 'Stop'

Write-Host 'Building Sourcerer project...' -ForegroundColor Blue

# Create build directory if it doesn't exist
if (-not (Test-Path 'C:\workspace\build\Release-Windows')) {
    New-Item -ItemType Directory -Path 'C:\workspace\build\Release-Windows' -Force | Out-Null
}

# Set environment variables for CMake build
$qtRoot = 'C:\Qt\6.10.3\msvc2022_64'
$env:MSYS64_DIR = ''
$env:QT_DIR = 'C:\Qt'
$env:Qt6_DIR = Join-Path $qtRoot 'lib\cmake\Qt6'
$opensslCandidates = @(
    'C:\Qt\Tools\OpenSSLv3\Win_x64'
)
$env:OPENSSL_ROOT_DIR = $null
foreach ($candidate in $opensslCandidates) {
    $libs = Get-ChildItem $candidate -Recurse -Filter 'libcrypto*.dll' -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($libs) {
        $env:OPENSSL_ROOT_DIR = $candidate
        break
    }
}
if (-not $env:OPENSSL_ROOT_DIR) {
    throw 'OpenSSL development files and libraries not found inside the container'
}
$env:CMAKE_PREFIX_PATH = $qtRoot + ';' + $env:OPENSSL_ROOT_DIR

Write-Host 'Environment variables set:' -ForegroundColor Green
Write-Host "  Qt6_DIR = $($env:Qt6_DIR)" -ForegroundColor Gray
Write-Host "  OPENSSL_ROOT_DIR = $($env:OPENSSL_ROOT_DIR)" -ForegroundColor Gray
Write-Host "  CMAKE_PREFIX_PATH = $($env:CMAKE_PREFIX_PATH)" -ForegroundColor Gray

Write-Host 'Configuring and building with the Visual Studio 2022 toolchain...' -ForegroundColor Blue
$buildScript = 'C:\workspace\docker\build-windows-cmake.cmd'
if (-not (Test-Path $buildScript)) {
    throw 'Build script not found inside the container'
}

& cmd /c $buildScript
if ($LASTEXITCODE -ne 0) {
    Write-Host 'Project build failed' -ForegroundColor Red
    exit 1
}

Write-Host 'Project built successfully' -ForegroundColor Green

# Normalize the executable location for the installer scripts
$releaseDir = 'C:\workspace\build\Release-Windows'
$releaseDirFull = (Resolve-Path $releaseDir).Path
$exeCandidate = Get-ChildItem 'C:\workspace\build' -Recurse -Filter 'Sourcerer.exe' -ErrorAction SilentlyContinue |
    Select-Object -First 1
if (-not $exeCandidate) {
    Write-Host 'Sourcerer.exe was not found. Available build outputs:' -ForegroundColor Yellow
    Get-ChildItem 'C:\workspace\build' -Recurse -Filter '*.exe' -ErrorAction SilentlyContinue |
        Select-Object -First 40 |
        ForEach-Object { Write-Host "  $($_.FullName)" -ForegroundColor Yellow }
    throw 'Sourcerer.exe was not found in the build output'
}
$exeDestination = Join-Path $releaseDirFull 'Sourcerer.exe'
if ((Resolve-Path $exeCandidate.FullName).Path -ne (Resolve-Path $exeDestination).Path) {
    Copy-Item $exeCandidate.FullName $exeDestination -Force
    Write-Host "  Copied Sourcerer.exe from $($exeCandidate.FullName)" -ForegroundColor Gray
} else {
    Write-Host '  Sourcerer.exe is already in the release directory' -ForegroundColor Gray
}

# Copy any shared runtime DLLs that the WiX installer expects into the release directory
$openSslBin = Join-Path $env:OPENSSL_ROOT_DIR 'bin'

foreach ($dllName in @('libssl-3-x64.dll', 'libcrypto-3-x64.dll')) {
    if (-not (Test-Path (Join-Path $releaseDirFull $dllName)) -and (Test-Path (Join-Path $openSslBin $dllName))) {
        Copy-Item (Join-Path $openSslBin $dllName) $releaseDirFull -Force
        Write-Host "  Copied $dllName from OpenSSL" -ForegroundColor Gray
    }
}

foreach ($qtModuleDll in @('Qt6PrintSupport.dll', 'Qt6QuickWidgets.dll', 'Qt6Quick.dll', 'Qt6QmlMeta.dll', 'Qt6QmlModels.dll', 'Qt6QmlWorkerScript.dll', 'Qt6Core5Compat.dll', 'Qt6Qml.dll', 'Qt6OpenGL.dll')) {
    $qtModuleSource = Join-Path $qtRoot ('bin\' + $qtModuleDll)
    $qtModuleDestination = Join-Path $releaseDirFull $qtModuleDll
    if (-not (Test-Path $qtModuleDestination) -and (Test-Path $qtModuleSource)) {
        Copy-Item $qtModuleSource $qtModuleDestination -Force
        Write-Host "  Copied $qtModuleDll from Qt bin" -ForegroundColor Gray
    }
}

if (-not (Test-Path (Join-Path $releaseDirFull 'z.dll'))) {
    $zlibDll = Get-ChildItem 'C:\workspace\build\Release-Windows' -Recurse -Filter 'z.dll' -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $zlibDll) {
        $zlibDll = Get-ChildItem 'C:\workspace\build\Release-Windows' -Recurse -Filter 'zlib1.dll' -ErrorAction SilentlyContinue | Select-Object -First 1
    }
    if ($zlibDll) {
        Copy-Item $zlibDll.FullName (Join-Path $releaseDirFull 'z.dll') -Force
        Write-Host "  Copied z.dll from $($zlibDll.FullName)" -ForegroundColor Gray
    }
}

# Run installer build script
Write-Host 'Creating installer...' -ForegroundColor Blue
Set-Location C:\workspace\installer
.\build-installer-wix.bat
if ($LASTEXITCODE -ne 0) {
    Write-Host 'Installer build failed' -ForegroundColor Red
    exit 1
}

Write-Host 'Installer created successfully' -ForegroundColor Green
