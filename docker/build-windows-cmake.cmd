@echo off
setlocal enabledelayedexpansion

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
if errorlevel 1 (
    echo Error: Failed to initialize Visual Studio 2022 environment
    exit /b 1
)

where cl
if errorlevel 1 (
    echo Error: cl.exe not found after initializing Visual Studio environment
    exit /b 1
)

rem Keep linker/cvtres temp files (e.g. {GUID}.res) off the bind-mounted workspace.
if not exist C:\buildtmp mkdir C:\buildtmp
set TMP=C:\buildtmp
set TEMP=C:\buildtmp

set CC=cl
set CXX=cl
cmake -S C:\workspace\src -B C:\workspace\build\Release-Windows -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY -DCMAKE_PREFIX_PATH="C:\Qt\6.10.3\msvc2022_64;%OPENSSL_ROOT_DIR%" -DOPENSSL_ROOT_DIR="%OPENSSL_ROOT_DIR%" -DSOURCERER_BUILD_SVN_CLI=ON -DUSE_THREADS=win32 -DSTATIC_CRT=OFF -DCMAKE_EXE_LINKER_FLAGS=/MANIFEST:NO -DCMAKE_SHARED_LINKER_FLAGS=/MANIFEST:NO
if errorlevel 1 (
    echo Error: CMake configure failed
    exit /b 1
)

cmake --build C:\workspace\build\Release-Windows --parallel 20
if errorlevel 1 (
    echo Error: CMake build failed
    exit /b 1
)

exit /b 0
