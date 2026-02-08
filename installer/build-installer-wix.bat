@echo off
REM WiX-based Windows Installer Build Script

setlocal enabledelayedexpansion

REM Set paths
set "PROJECT_ROOT=%~dp0.."
set "BUILD_DIR=%PROJECT_ROOT%\build\Release"
set "INSTALLER_DIR=%~dp0"
set "WXS_FILE=%INSTALLER_DIR%Sourcerer.wxs"

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
if not exist "%PROJECT_ROOT%\build\Release\Sourcerer.exe" (
    echo Error: Sourcerer.exe not found at "%PROJECT_ROOT%\build\Release\Sourcerer.exe"
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

REM Detect MSYS2 installation (use mingw64\bin for compiled libraries)
echo.
echo [DEBUG] Detecting MSYS2 installation...
set MSYS2_BIN_PATH=
if exist "C:\tools\msys64\mingw64\bin" (
    set "MSYS2_BIN_PATH=C:\tools\msys64\mingw64\bin"
    echo [DEBUG] Found MSYS2 at C:\tools\msys64
) else if exist "C:\msys64\mingw64\bin" (
    set "MSYS2_BIN_PATH=C:\msys64\mingw64\bin"
    echo [DEBUG] Found MSYS2 at C:\msys64
) else if exist "%ProgramFiles%\msys64\mingw64\bin" (
    set "MSYS2_BIN_PATH=%ProgramFiles%\msys64\mingw64\bin"
    echo [DEBUG] Found MSYS2 at %ProgramFiles%\msys64
) else (
    echo Error: MSYS2 installation not found
    echo Searched in: C:\tools\msys64, C:\msys64, %ProgramFiles%\msys64
    echo Please install MSYS2 from https://www.msys2.org/
    exit /b 1
)

REM Detect Qt MinGW bin directory
echo.
echo [DEBUG] Detecting Qt MinGW installation...
set QT_MINGW_BIN_PATH=
set QT_VERSION=6.10.1
set QT_DIR=C:\Qt
set "QT_MINGW_DIR=%QT_DIR%\%QT_VERSION%\mingw_64"

if exist "%QT_MINGW_DIR%\bin" (
    set "QT_MINGW_BIN_PATH=%QT_MINGW_DIR%\bin"
    echo [DEBUG] Found Qt MinGW at %QT_MINGW_DIR%
) else (
    echo Error: Qt MinGW not found at "%QT_MINGW_DIR%"
    echo Please verify Qt installation
    exit /b 1
)

REM Verify all required DLL files exist
echo.
echo Verifying required DLL files...
set MISSING_DLLS=
for %%i in (
    libbz2-1.dll
    libgit2-1.9.dll
    libhttp_parser-2.dll
    libiconv-2.dll
    liblzma-5.dll
    libpcre2-8-0.dll
    libssh2-1.dll
    libzip.dll
    libzstd.dll
    zlib1.dll
) do (
    if not exist "%MSYS2_BIN_PATH%\%%i" (
        echo Warning: "%MSYS2_BIN_PATH%\%%i" not found
        set MISSING_DLLS=!MISSING_DLLS! %%i
    )
)

if defined MISSING_DLLS (
    echo.
    echo Warning: Some MSYS2 DLL files were not found:!MISSING_DLLS!
    echo The installer may not work correctly without these dependencies.
)

REM Deploy Qt dependencies
echo.
echo Deploying Qt dependencies...
call :deployQtDependencies

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
    echo Warning: LICENSE.rtf not found at "%BUILD_DIR%\LICENSE.rtf"
)

REM Compile WiX source to object file
echo.
echo Compiling WiX source files...
set "WXO_FILE=%BUILD_DIR%\Sourcerer.wixobj"
echo [DEBUG] Running candle: "!CANDLE_PATH!" -out "!WXO_FILE!" -d"SourcererBinPath=%PROJECT_ROOT%\build\Release" -d"MSYS2BinPath=!MSYS2_BIN_PATH!" -d"QtMinGWBinPath=!QT_MINGW_BIN_PATH!" -d"LicenseRtfPath=%BUILD_DIR%\LICENSE.rtf" "!WXS_FILE!"

"!CANDLE_PATH!" ^
    -out "!WXO_FILE!" ^
    -d"SourcererBinPath=%PROJECT_ROOT%\build\Release" ^
    -d"MSYS2BinPath=!MSYS2_BIN_PATH!" ^
    -d"QtMinGWBinPath=!QT_MINGW_BIN_PATH!" ^
    -d"LicenseRtfPath=%BUILD_DIR%\LICENSE.rtf" ^
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
set QT_VERSION=6.10.1
set QT_DIR=C:\Qt
set QT_BIN_DIR=%QT_DIR%\%QT_VERSION%\mingw_64\bin

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
echo Deploying to: "%PROJECT_ROOT%\build\Release"

"%WINDEPLOYQT%" --release --no-translations "%PROJECT_ROOT%\build\Release\Sourcerer.exe"
if errorlevel 1 (
    echo Warning: windeployqt returned an error, but continuing...
)
endlocal
exit /b 0
