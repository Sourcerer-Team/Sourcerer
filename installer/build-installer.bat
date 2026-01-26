@echo off
REM Build script for Windows installer

setlocal

REM Set paths
set PROJECT_ROOT=%~dp0..
set BUILD_DIR=%PROJECT_ROOT%\build
set RELEASE_DIR=%BUILD_DIR%
set INSTALLER_DIR=%~dp0
set DATA_DIR=%INSTALLER_DIR%\packages\com.sourcerer.app\data
set QT_IFW_DIR=C:\Qt\Tools\QtInstallerFramework\4.7
set QT_MINGW_DIR=C:\Qt\6.10.1\mingw_64
set MSYS64_DIR=C:\tools\msys64

REM Check if MSYS64 is installed
if not exist "%MSYS64_DIR%\usr\bin\bash.exe" (
    echo Warning: MSYS64 not found at %MSYS64_DIR%
    echo Please install MSYS64 from https://www.msys2.org/
    echo MSYS64 is required for some project dependencies.
    REM Continue anyway - might not be needed
)

REM Check for Qt MinGW installation and add to PATH
set MINGW_FOUND=
for /d %%i in ("C:\Qt\Tools\mingw*") do (
    if exist "%%i\bin\mingw32-make.exe" (
        echo Found Qt MinGW tools at %%i
        set PATH=%%i\bin;%PATH%
        set MINGW_FOUND=1
        goto mingw_found
    )
)
if not defined MINGW_FOUND (
    if exist "%QT_MINGW_DIR%\bin\mingw32-make.exe" (
        echo Found Qt MinGW at %QT_MINGW_DIR%
        set PATH=%QT_MINGW_DIR%\bin;%PATH%
        set MINGW_FOUND=1
        goto mingw_found
    )
)
if not defined MINGW_FOUND (
    echo Error: Qt MinGW not found. Please install one of the following:
    echo   - Qt MinGW tools in C:\Qt\Tools\mingw*
    echo   - Qt MinGW at %QT_MINGW_DIR%
    exit /b 1
)
:mingw_found

REM Check if Qt IFW is installed
if not exist "%QT_IFW_DIR%\bin\binarycreator.exe" (
    echo Error: Qt Installer Framework not found at %QT_IFW_DIR%
    echo Please install Qt IFW 4.0 or update QT_IFW_DIR path
    exit /b 1
)

REM Build release version
echo Building Sourcerer release version...
cd "%PROJECT_ROOT%\src"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

REM Set libgit2 paths from MSYS64 installation
set LIBGIT2_INCLUDEDIR=%MSYS64_DIR%\mingw64\include
set LIBGIT2_LIBDIR=%MSYS64_DIR%\mingw64\lib

REM Set libzip path
set LIBZIP_INCLUDEDIR=%MSYS64_DIR%\mingw64\include
set LIBZIP_LIBDIR=%MSYS64_DIR%\mingw64\lib

REM Set OpenSSL path from MSYS2
set OPENSSL_ROOT_DIR=%MSYS64_DIR%\mingw64

REM Clean CMake cache to avoid generator conflicts
if exist CMakeCache.txt del CMakeCache.txt
if exist CMakeFiles rmdir /s /q CMakeFiles

cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_MINGW_DIR%" "%PROJECT_ROOT%\src"
if %ERRORLEVEL% neq 0 (
    echo Error: CMake configuration failed
    exit /b 1
)

mingw32-make -j%NUMBER_OF_PROCESSORS%
if %ERRORLEVEL% neq 0 (
    echo Error: Build failed
    exit /b 1
)

REM Clean and prepare data directory
echo Preparing installer data directory...
if exist "%DATA_DIR%" rmdir /s /q "%DATA_DIR%"
mkdir "%DATA_DIR%"

REM Copy application files
echo Copying application files...
copy "%RELEASE_DIR%\Sourcerer.exe" "%DATA_DIR%\" || exit /b 1

REM Copy application icon and images for installer
echo Copying application icon and images...
copy "%PROJECT_ROOT%\component\GuiManager\resources\icons\sorcerer-hat.ico" "%INSTALLER_DIR%\config\" || exit /b 1
if not exist "%INSTALLER_DIR%\config\images" mkdir "%INSTALLER_DIR%\config\images"
copy "%PROJECT_ROOT%\component\GuiManager\resources\icons\sorcerer-hat.png" "%INSTALLER_DIR%\config\images\logo.png" || exit /b 1
copy "%PROJECT_ROOT%\component\GuiManager\resources\icons\sorcerer-hat.png" "%INSTALLER_DIR%\config\images\watermark.png" || exit /b 1

REM Deploy Qt dependencies using windeployqt
echo Deploying Qt dependencies...
"%QT_MINGW_DIR%\bin\windeployqt.exe" --release --no-translations "%DATA_DIR%\Sourcerer.exe"
if %ERRORLEVEL% neq 0 (
    echo Warning: windeployqt reported errors
)

REM Copy additional runtime dependencies
echo Copying runtime dependencies...

REM Copy MSYS64 dependencies
echo Copying MSYS64 dependencies...
set MSYS64_BIN=%MSYS64_DIR%\mingw64\bin

if not exist "%MSYS64_BIN%\libbz2-1.dll" (echo Error: libbz2-1.dll not found in %MSYS64_BIN% & exit /b 1)
copy "%MSYS64_BIN%\libbz2-1.dll" "%DATA_DIR%\" || exit /b 1

if not exist "%MSYS64_BIN%\libgit2-1.9.dll" (echo Error: libgit2-1.9.dll not found in %MSYS64_BIN% & exit /b 1)
copy "%MSYS64_BIN%\libgit2-1.9.dll" "%DATA_DIR%\" || exit /b 1

if not exist "%MSYS64_BIN%\libhttp_parser-2.dll" (echo Error: libhttp_parser-2.dll not found in %MSYS64_BIN% & exit /b 1)
copy "%MSYS64_BIN%\libhttp_parser-2.dll" "%DATA_DIR%\" || exit /b 1

if not exist "%MSYS64_BIN%\libiconv-2.dll" (echo Error: libiconv-2.dll not found in %MSYS64_BIN% & exit /b 1)
copy "%MSYS64_BIN%\libiconv-2.dll" "%DATA_DIR%\" || exit /b 1

if not exist "%MSYS64_BIN%\liblzma-5.dll" (echo Error: liblzma-5.dll not found in %MSYS64_BIN% & exit /b 1)
copy "%MSYS64_BIN%\liblzma-5.dll" "%DATA_DIR%\" || exit /b 1

if not exist "%MSYS64_BIN%\libpcre2-8-0.dll" (echo Error: libpcre2-8-0.dll not found in %MSYS64_BIN% & exit /b 1)
copy "%MSYS64_BIN%\libpcre2-8-0.dll" "%DATA_DIR%\" || exit /b 1

if not exist "%MSYS64_BIN%\libssh2-1.dll" (echo Error: libssh2-1.dll not found in %MSYS64_BIN% & exit /b 1)
copy "%MSYS64_BIN%\libssh2-1.dll" "%DATA_DIR%\" || exit /b 1

if not exist "%MSYS64_BIN%\libzip.dll" (echo Error: libzip.dll not found in %MSYS64_BIN% & exit /b 1)
copy "%MSYS64_BIN%\libzip.dll" "%DATA_DIR%\" || exit /b 1

if not exist "%MSYS64_BIN%\libzstd.dll" (echo Error: libzstd.dll not found in %MSYS64_BIN% & exit /b 1)
copy "%MSYS64_BIN%\libzstd.dll" "%DATA_DIR%\" || exit /b 1

if not exist "%MSYS64_BIN%\zlib1.dll" (echo Error: zlib1.dll not found in %MSYS64_BIN% & exit /b 1)
copy "%MSYS64_BIN%\zlib1.dll" "%DATA_DIR%\" || exit /b 1

REM Copy Visual C++ runtime if needed
REM copy "C:\Windows\System32\vcruntime*.dll" "%DATA_DIR%\" 2>nul
REM copy "C:\Windows\System32\msvcp*.dll" "%DATA_DIR%\" 2>nul

REM Extract version from config.xml
echo Extracting version from config.xml...
for /f "tokens=3 delims=<>" %%v in ('findstr "<Version>" "%INSTALLER_DIR%\config\config.xml"') do (
    if not defined APP_VERSION set APP_VERSION=%%v
)
if not defined APP_VERSION (
    echo Warning: Could not extract version from config.xml, using default 1.0.0
    set APP_VERSION=1.0.0
)
echo Application version: %APP_VERSION%

REM Create installer
echo Creating installer...
cd "%INSTALLER_DIR%"
"%QT_IFW_DIR%\bin\binarycreator.exe" -c config\config.xml -p packages Sourcerer-%APP_VERSION%-Installer.exe
if %ERRORLEVEL% neq 0 (
    echo Error: Installer creation failed
    exit /b 1
)

echo.
echo Installer created successfully: %INSTALLER_DIR%\Sourcerer-%APP_VERSION%-Installer.exe
exit /b 0
