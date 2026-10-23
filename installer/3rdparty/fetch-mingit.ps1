# Fetches MinGit, the portable git for Windows, into installer\3rdparty\git.
# Sourcerer puts 3rdparty\git\cmd on PATH, so only cmd\git.exe is used
# directly; the rest of the archive is its runtime.
#
# The download is checked against a pinned SHA-256 and cached next to this
# script. Windows PowerShell 5.1 runs it (build-installer-wix.bat).

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$MinGitTag = 'v2.56.0.windows.2'
$MinGitFile = 'MinGit-2.56.0.2-64-bit.zip'
$MinGitSha256 = 'da35e72aa21c005a5a0d298cfbae110bc1609a815730ea0dde84b01a1b3cd3be'

$ScriptDir = $PSScriptRoot
$OutDir = Join-Path $ScriptDir 'git'
$Archive = Join-Path $ScriptDir $MinGitFile

function Test-Sha256([string]$Path, [string]$Expected) {
    (Test-Path $Path) -and ((Get-FileHash -Algorithm SHA256 $Path).Hash -eq $Expected)
}

if (Test-Sha256 $Archive $MinGitSha256) {
    Write-Host "Using cached $MinGitFile"
} else {
    $url = "https://github.com/git-for-windows/git/releases/download/$MinGitTag/$MinGitFile"
    Write-Host "Downloading $url"
    Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile "$Archive.part"
    if (-not (Test-Sha256 "$Archive.part" $MinGitSha256)) {
        Remove-Item -Force "$Archive.part"
        throw "Checksum mismatch for $MinGitFile"
    }
    Move-Item -Force "$Archive.part" $Archive
}

if (Test-Path $OutDir) {
    Remove-Item -Recurse -Force $OutDir
}
Expand-Archive -Path $Archive -DestinationPath $OutDir

$git = Join-Path $OutDir 'cmd\git.exe'
if (-not (Test-Path $git)) {
    throw "No cmd\git.exe in $MinGitFile"
}
& $git --version
if ($LASTEXITCODE -ne 0) {
    throw 'The bundled git does not run'
}
Write-Host "Bundled MinGit in $OutDir"
