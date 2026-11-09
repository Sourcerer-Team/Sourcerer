# Portable Mercurial for the Windows installer: a relocatable CPython from
# python-build-standalone in installer\3rdparty\hg\python, with the Mercurial
# wheel unpacked into its Lib\site-packages. The hg.exe launcher
# (hg-launcher\hg.c) is built by Docker and copied next to python\ by
# build-installer-wix.bat, which then makes UTF-8 the code page of
# python\python.exe with set-python-utf8-codepage.ps1. fetch-hg-unix.sh
# bundles the same versions for Linux and macOS.
#
# Downloads are checked against pinned SHA-256 sums and cached next to this
# script. Windows PowerShell 5.1 runs it, and tar.exe (Windows 10 and later)
# unpacks both archives.

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$PythonRelease = '20261001'
$PythonVersion = '3.13.16'
$PythonSha256 = '0a0665c40b14a9ae079cfa9924439c92714079ac408ffbe0cfa520992444a610'
$HgVersion = '7.2.4'
$WheelFile = "mercurial-$HgVersion-cp313-cp313-win_amd64.whl"
$WheelUrl = "https://files.pythonhosted.org/packages/06/80/268d8a0750bd7683d4405ed0082896361eee180b016e1aaaf49ae5e416e3/$WheelFile"
$WheelSha256 = 'fb19f5232c4938abc6147dd826c10c4ea28a081771d92fbcaebc2678b8620793'

$ScriptDir = $PSScriptRoot
$OutDir = Join-Path $ScriptDir 'hg'

function Test-Sha256([string]$Path, [string]$Expected) {
    (Test-Path $Path) -and ((Get-FileHash -Algorithm SHA256 $Path).Hash -eq $Expected)
}

# Downloads $Url into the cache as $File unless a copy with $Sha256 is there.
function Get-Download([string]$Url, [string]$File, [string]$Sha256) {
    $path = Join-Path $ScriptDir $File
    if (Test-Sha256 $path $Sha256) {
        Write-Host "Using cached $File"
    } else {
        Write-Host "Downloading $Url"
        Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile "$path.part"
        if (-not (Test-Sha256 "$path.part" $Sha256)) {
            Remove-Item -Force "$path.part"
            throw "Checksum mismatch for $File"
        }
        Move-Item -Force "$path.part" $path
    }
    return $path
}

function Invoke-Tar([string[]]$Arguments) {
    & tar.exe @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "tar.exe $($Arguments -join ' ') failed"
    }
}

$pythonFile = "cpython-$PythonVersion+$PythonRelease-x86_64-pc-windows-msvc-install_only.tar.gz"
$pythonUrl = "https://github.com/astral-sh/python-build-standalone/releases/download/$PythonRelease/" +
    $pythonFile.Replace('+', '%2B')
$pythonArchive = Get-Download $pythonUrl $pythonFile $PythonSha256
$wheelArchive = Get-Download $WheelUrl $WheelFile $WheelSha256

# python\ is replaced; a hg.exe copied here by hand stays. bin\ is the Unix
# launcher of fetch-hg-unix.sh, which shares this folder: it is removed so
# that the Linux and macOS builds fetch again instead of bundling this
# Windows Python.
$pythonDir = Join-Path $OutDir 'python'
foreach ($dir in @($pythonDir, (Join-Path $OutDir 'bin'))) {
    if (Test-Path $dir) {
        Remove-Item -Recurse -Force $dir
    }
}
New-Item -ItemType Directory -Force $OutDir | Out-Null
# The archive holds a single top-level "python" directory.
Invoke-Tar @('-xzf', $pythonArchive, '-C', $OutDir)
$python = Join-Path $pythonDir 'python.exe'
if (-not (Test-Path $python)) {
    throw "No python.exe in $pythonFile"
}

# A wheel is a zip that is installed by unpacking it into site-packages.
$sitePackages = Join-Path $pythonDir 'Lib\site-packages'
Invoke-Tar @('-xf', $wheelArchive, '-C', $sitePackages)
if (-not (Test-Path (Join-Path $sitePackages 'mercurial\__main__.py'))) {
    throw "Mercurial not found in $WheelFile"
}

# The same command line as the hg.exe launcher.
$version = & $python -I -m mercurial version -T '{ver}'
if ($LASTEXITCODE -ne 0) {
    throw 'The bundled Mercurial does not run'
}
if ($version -ne $HgVersion) {
    throw "The bundled Mercurial reports $version, expected $HgVersion"
}
Write-Host "Bundled Mercurial $version in $OutDir"
