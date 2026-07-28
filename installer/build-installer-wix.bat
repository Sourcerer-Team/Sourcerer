@echo off
REM WiX-based Windows Installer Build Script

setlocal enabledelayedexpansion

REM Set paths
set "PROJECT_ROOT=%~dp0.."
set "BUILD_DIR=%PROJECT_ROOT%\build\Release-Windows"
set "INSTALLER_DIR=%~dp0"
set "WXS_FILE=%INSTALLER_DIR%Sourcerer.wxs"
set "QT_VERSION=6.10.3"
set "QT_DIR=C:\Qt"
set "QT_MSVC_BIN_PATH=%QT_DIR%\%QT_VERSION%\msvc2022_64\bin"

REM Normalize build directory path
if not exist "%BUILD_DIR%" (
    echo [DEBUG] Creating BUILD_DIR: %BUILD_DIR%
    mkdir "%BUILD_DIR%"
)

echo [DEBUG] Script started
echo [DEBUG] PROJECT_ROOT=%PROJECT_ROOT%
echo [DEBUG] BUILD_DIR=%BUILD_DIR%
echo [DEBUG] INSTALLER_DIR=%INSTALLER_DIR%
echo [DEBUG] WXS_FILE=%WXS_FILE%

REM Check for WiX Toolset
echo.
echo [DEBUG] Checking for WiX Toolset...
for %%I in (candle.exe) do (
    set CANDLE_PATH=%%~$PATH:I
)

if not defined CANDLE_PATH (
    echo [DEBUG] candle.exe not in PATH, checking standard locations...
    REM Try common WiX installation paths
    if exist "C:\Program Files (x86)\WiX Toolset v3.14\bin\candle.exe" (
        set "WIX_DIR=C:\Program Files (x86)\WiX Toolset v3.14"
        echo [DEBUG] Found WiX v3.14
    ) else if exist "C:\Program Files (x86)\WiX Toolset v3.13\bin\candle.exe" (
        set "WIX_DIR=C:\Program Files (x86)\WiX Toolset v3.13"
        echo [DEBUG] Found WiX v3.13
    ) else if exist "C:\Program Files (x86)\WiX Toolset v3.12\bin\candle.exe" (
        set "WIX_DIR=C:\Program Files (x86)\WiX Toolset v3.12"
        echo [DEBUG] Found WiX v3.12
    ) else if exist "C:\Program Files (x86)\WiX Toolset v3.11\bin\candle.exe" (
        set "WIX_DIR=C:\Program Files (x86)\WiX Toolset v3.11"
        echo [DEBUG] Found WiX v3.11
    ) else (
        echo Error: WiX Toolset not found
        echo Please install WiX Toolset v3.11 or later from https://wixtoolset.org/
        exit /b 1
    )

    set "CANDLE_PATH=!WIX_DIR!\bin\candle.exe"
    set "LIGHT_PATH=!WIX_DIR!\bin\light.exe"
) else (
    echo [DEBUG] Found candle.exe in PATH
    for %%I in (light.exe) do (
        set LIGHT_PATH=%%~$PATH:I
    )
)

echo.
echo Found WiX Toolset:
echo [DEBUG] Candle: "!CANDLE_PATH!"
echo [DEBUG] Light:  "!LIGHT_PATH!"

REM Verify required files and directories exist
echo.
echo Checking required files and directories...
if not exist "%PROJECT_ROOT%\build\Release-Windows\Sourcerer.exe" (
    echo Error: Sourcerer.exe not found at "%PROJECT_ROOT%\build\Release-Windows\Sourcerer.exe"
    echo Please build the Sourcerer project first
    exit /b 1
)

if not exist "%WXS_FILE%" (
    echo Error: "%WXS_FILE%" not found
    exit /b 1
)

REM Create BUILD_DIR if it doesn't exist
if not exist "%BUILD_DIR%" (
    echo [DEBUG] Creating BUILD_DIR: "%BUILD_DIR%"
    mkdir "%BUILD_DIR%" 2>nul
)

echo [DEBUG] Sourcerer.exe found
echo [DEBUG] Sourcerer.wxs found
echo [DEBUG] Ready to proceed with build

REM Detect Qt MSVC installation
echo.
echo [DEBUG] Detecting Qt MSVC installation...
set QT_BIN_PATH=
if exist "%QT_MSVC_BIN_PATH%" (
    set "QT_BIN_PATH=%QT_MSVC_BIN_PATH%"
    echo [DEBUG] Found Qt MSVC at %QT_DIR%\%QT_VERSION%\msvc2022_64
) else (
    echo Error: Qt MSVC not found at "%QT_MSVC_BIN_PATH%"
    echo Please verify Qt installation
    exit /b 1
)

REM Detect OpenSSL installation
echo.
echo [DEBUG] Detecting OpenSSL installation...
if not defined OPENSSL_ROOT_DIR (
    if exist "C:\Program Files\OpenSSL" (
        set "OPENSSL_ROOT_DIR=C:\Program Files\OpenSSL"
    ) else if exist "C:\Program Files\OpenSSL-Win64" (
        set "OPENSSL_ROOT_DIR=C:\Program Files\OpenSSL-Win64"
    ) else if exist "C:\Program Files (x86)\OpenSSL" (
        set "OPENSSL_ROOT_DIR=C:\Program Files (x86)\OpenSSL"
    ) else if exist "C:\Program Files (x86)\OpenSSL-Win64" (
        set "OPENSSL_ROOT_DIR=C:\Program Files (x86)\OpenSSL-Win64"
    ) else (
        echo Error: OpenSSL installation not found
        exit /b 1
    )
)
set OPENSSL_BIN_PATH=%OPENSSL_ROOT_DIR%\bin

echo [DEBUG] OpenSSL root: !OPENSSL_ROOT_DIR!

REM Verify runtime files exist or can be deployed
echo.
echo Verifying required runtime files...
if not exist "%OPENSSL_BIN_PATH%\libssl-3-x64.dll" (
    echo Warning: "%OPENSSL_BIN_PATH%\libssl-3-x64.dll" not found
)
if not exist "%OPENSSL_BIN_PATH%\libcrypto-3-x64.dll" (
    echo Warning: "%OPENSSL_BIN_PATH%\libcrypto-3-x64.dll" not found
)

REM Deploy Qt dependencies
echo.
echo Deploying Qt dependencies...
call :deployQtDependencies

REM Ensure required Qt module DLLs are present if windeployqt did not copy them
echo.
echo Checking for Qt module DLLs...
for %%i in (Qt6PrintSupport.dll Qt6QuickWidgets.dll Qt6Quick.dll Qt6QmlMeta.dll Qt6QmlModels.dll Qt6QmlWorkerScript.dll Qt6Core5Compat.dll) do (
    if not exist "%BUILD_DIR%\%%i" (
        if exist "%QT_BIN_PATH%\%%i" (
            copy "%QT_BIN_PATH%\%%i" "%BUILD_DIR%\%%i" >nul
            echo [DEBUG] Copied %%i from Qt bin
        )
    )
)

REM Copy Qt WebEngine process binary and support resources
echo.
echo Checking Qt WebEngine support files...
if not exist "%BUILD_DIR%\QtWebEngineProcess.exe" (
    if exist "%QT_BIN_PATH%\QtWebEngineProcess.exe" (
        copy "%QT_BIN_PATH%\QtWebEngineProcess.exe" "%BUILD_DIR%\QtWebEngineProcess.exe" >nul
        echo [DEBUG] Copied QtWebEngineProcess.exe from Qt bin
    )
)
if not exist "%BUILD_DIR%\resources" mkdir "%BUILD_DIR%\resources"
for %%i in (icudtl.dat qtwebengine_resources.pak qtwebengine_resources_100p.pak qtwebengine_resources_200p.pak qtwebengine_devtools_resources.pak v8_context_snapshot.bin) do (
    if exist "%QT_DIR%\%QT_VERSION%\msvc2022_64\resources\%%i" (
        copy "%QT_DIR%\%QT_VERSION%\msvc2022_64\resources\%%i" "%BUILD_DIR%\resources\%%i" >nul
        echo [DEBUG] Copied %%i to resources folder
    )
)
if exist "%QT_DIR%\%QT_VERSION%\msvc2022_64\translations\qtwebengine_locales" (
    if not exist "%BUILD_DIR%\translations\qtwebengine_locales" mkdir "%BUILD_DIR%\translations\qtwebengine_locales"
    xcopy "%QT_DIR%\%QT_VERSION%\msvc2022_64\translations\qtwebengine_locales\*" "%BUILD_DIR%\translations\qtwebengine_locales\" /Y /I /E >nul
    echo [DEBUG] Copied qtwebengine_locales folder
)

REM Ensure z.dll is present in the release directory for packaging
echo.
echo Checking for z.dll...
if not exist "%BUILD_DIR%\z.dll" (
    for /r "%BUILD_DIR%" %%i in (z.dll) do (
        if not exist "%BUILD_DIR%\z.dll" (
            copy "%%~fi" "%BUILD_DIR%\z.dll" >nul
            echo [DEBUG] Copied z.dll from %%~fi
        )
    )
    for /r "%BUILD_DIR%" %%i in (zlib1.dll) do (
        if not exist "%BUILD_DIR%\z.dll" (
            copy "%%~fi" "%BUILD_DIR%\z.dll" >nul
            echo [DEBUG] Copied z.dll from %%~fi
        )
    )
)

REM Copy OpenSSL DLLs into the release directory if windeployqt did not already do so
echo.
echo Ensuring OpenSSL runtime DLLs are present...
for %%i in (libssl-3-x64.dll libcrypto-3-x64.dll) do (
    if not exist "%BUILD_DIR%\%%i" (
        if exist "%OPENSSL_BIN_PATH%\%%i" (
            copy "%OPENSSL_BIN_PATH%\%%i" "%BUILD_DIR%\%%i" >nul
            echo [DEBUG] Copied %%i from OpenSSL bin
        )
    )
)

REM Copy security and configuration files
echo.
echo Copying security files...
if exist "%PROJECT_ROOT%\keys" (
    if not exist "%BUILD_DIR%\keys" mkdir "%BUILD_DIR%\keys"
    xcopy "%PROJECT_ROOT%\keys\*" "%BUILD_DIR%\keys\" /Y /I >nul
    echo [DEBUG] Security files copied to build directory
) else (
    echo Warning: Keys directory not found at "%PROJECT_ROOT%\keys"
)

REM Copy license file to build directory
echo.
echo Copying license file...
if exist "%INSTALLER_DIR%LICENSE.rtf" (
    copy "%INSTALLER_DIR%LICENSE.rtf" "%BUILD_DIR%\LICENSE.rtf" >nul
    echo [DEBUG] License file copied
) else (
    echo Warning: LICENSE.rtf not found at "%INSTALLER_DIR%LICENSE.rtf"
)

set "LICENSE_RTF_PATH=%BUILD_DIR%\LICENSE.rtf"
if not exist "%LICENSE_RTF_PATH%" (
    if exist "%INSTALLER_DIR%LICENSE.rtf" (
        set "LICENSE_RTF_PATH=%INSTALLER_DIR%LICENSE.rtf"
    )
)
if not exist "%LICENSE_RTF_PATH%" (
    echo Error: LICENSE.rtf not found
    exit /b 1
)

REM Read application version from the shared ConfigManager source
echo.
echo [DEBUG] Reading application version...
set "APP_VERSION="
for /f "tokens=2 delims==" %%A in ('findstr /c:"APPLICATION_VERSION" "%PROJECT_ROOT%\component\ConfigManager\GitVersion.h.in"') do (
    set "APP_VERSION=%%A"
)
set "APP_VERSION=!APP_VERSION:"=!"
set "APP_VERSION=!APP_VERSION:;=!"
for /f "tokens=* delims= " %%A in ("!APP_VERSION!") do set "APP_VERSION=%%A"
if not defined APP_VERSION (
    echo Error: Could not determine Sourcerer version from "%PROJECT_ROOT%\component\ConfigManager\GitVersion.h.in"
    exit /b 1
)
echo [DEBUG] Sourcerer version=!APP_VERSION!

REM Compile WiX source to object file
echo.
echo Compiling WiX source files...
set "WXO_FILE=%BUILD_DIR%\Sourcerer.wixobj"
echo [DEBUG] Running candle: "!CANDLE_PATH!" -out "!WXO_FILE!" -d"SourcererBinPath=%PROJECT_ROOT%\build\Release-Windows" -d"SourcererVersion=!APP_VERSION!" -d"LicenseRtfPath=!LICENSE_RTF_PATH!" "!WXS_FILE!"

"!CANDLE_PATH!" ^
    -out "!WXO_FILE!" ^
    -d"SourcererBinPath=%PROJECT_ROOT%\build\Release-Windows" ^
    -d"SourcererVersion=!APP_VERSION!" ^
    -d"LicenseRtfPath=!LICENSE_RTF_PATH!" ^
    "!WXS_FILE!"

if errorlevel 1 (
    echo Error: WiX candle compilation failed
    exit /b 1
)

REM Link object file to MSI
echo.
echo Linking WiX installer...
set "MSI_OUTPUT=%INSTALLER_DIR%\Sourcerer-Installer.msi"
echo [DEBUG] Running light: "!LIGHT_PATH!" -out "!MSI_OUTPUT!" -ext WixUIExtension -ext WixUtilExtension "!WXO_FILE!"

"!LIGHT_PATH!" ^
    -out "!MSI_OUTPUT!" ^
    -ext WixUIExtension ^
    -ext WixUtilExtension ^
    -sice:ICE91 ^
    "!WXO_FILE!"

if errorlevel 1 (
    echo Error: WiX light linking failed
    exit /b 1
)

echo.
echo Success! Installer created: "!MSI_OUTPUT!"
echo [DEBUG] Build complete
exit /b 0

:deployQtDependencies
setlocal
set QT_VERSION=6.10.3
set QT_DIR=C:\Qt
set QT_BIN_DIR=%QT_DIR%\%QT_VERSION%\msvc2022_64\bin

REM Look for windeployqt
for %%I in (windeployqt.exe) do (
    set WINDEPLOYQT=%%~$PATH:I
)

if not defined WINDEPLOYQT (
    if exist "%QT_BIN_DIR%\windeployqt.exe" (
        set WINDEPLOYQT=%QT_BIN_DIR%\windeployqt.exe
    ) else (
        echo Warning: windeployqt not found. Qt dependencies may not be deployed automatically.
        exit /b 0
    )
)

echo Found windeployqt: "%WINDEPLOYQT%"
echo Deploying to: "%PROJECT_ROOT%\build\Release-Windows"

REM Deploy Qt runtime files and include OpenSSL if available
"%WINDEPLOYQT%" --release --no-translations --openssl-root "%OPENSSL_ROOT_DIR%" "%PROJECT_ROOT%\build\Release-Windows\Sourcerer.exe"
if errorlevel 1 (
    echo Warning: windeployqt returned an error, but continuing...
)
endlocal
exit /b 0
