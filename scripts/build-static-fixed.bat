@echo off
REM Windows batch script for building jsrt with static linking and libuv fixes
REM This version includes aggressive fixes for libuv build issues

echo Building jsrt with static linking on Windows (with libuv fixes)...
echo.

REM Check if we're in MSYS2 environment
if not defined MSYSTEM (
    echo ERROR: This script must be run from MSYS2 environment
    echo Please open MSYS2 terminal and run this script again
    echo.
    echo Install MSYS2 from: https://www.msys2.org/
    echo Then install required packages:
    echo   pacman -S mingw-w64-ucrt-x86_64-gcc
    echo   pacman -S mingw-w64-ucrt-x86_64-cmake  
    echo   pacman -S mingw-w64-ucrt-x86_64-openssl
    echo   pacman -S make
    pause
    exit /b 1
)

REM Initialize submodules if needed
echo Checking git submodules...
git submodule update --init --recursive

REM Apply aggressive Windows-specific fixes
echo Applying Windows build fixes and libuv patch...
chmod +x scripts/patch-libuv-windows.sh
chmod +x scripts/pre-build-windows.sh
bash scripts/pre-build-windows.sh

REM Clean any previous build
if exist target\static (
    echo Cleaning previous build...
    rmdir /s /q target\static
)

REM Create build directory
mkdir target\static

REM Configure with CMake
echo Configuring build with static linking...
cd target\static
cmake -G "MSYS Makefiles" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DBUILD_SHARED_LIBS=OFF ^
    -DJSRT_STATIC_OPENSSL=ON ^
    -DCMAKE_MAKE_PROGRAM=/usr/bin/make ^
    %~dp0

if errorlevel 1 (
    echo ERROR: CMake configuration failed
    cd ..\..
    pause
    exit /b 1
)

REM Build
echo Building jsrt...
/usr/bin/make -j%NUMBER_OF_PROCESSORS%

if errorlevel 1 (
    echo ERROR: Build failed
    echo.
    echo If you see libuv Unix file errors, the patch may not have worked.
    echo You can try restoring libuv and rebuilding:
    echo   bash scripts/restore-libuv.sh
    cd ..\..
    pause
    exit /b 1
)

REM Show result
cd ..\..
echo.
echo Build completed successfully!
echo Static executable: target\static\jsrt.exe
echo.
echo Testing the build...
target\static\jsrt.exe --version

REM Restore environment and clean submodules
if exist scripts\post-build-windows.sh (
    echo Restoring environment...
    bash scripts/post-build-windows.sh
)

echo Cleaning up submodule modifications...
chmod +x scripts/cleanup-submodules.sh
bash scripts/cleanup-submodules.sh

echo.
echo Static build complete. The executable includes all dependencies.
echo All submodules have been reset to clean state - safe to commit.
pause