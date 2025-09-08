@echo off
REM Windows batch script for building jsrt with static linking
REM This script builds jsrt with all dependencies (including OpenSSL) statically linked

echo Building jsrt with static linking on Windows...
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

REM Check if OpenSSL static libraries are available
if not exist "C:\msys64\ucrt64\lib\libcrypto.a" (
    echo WARNING: OpenSSL static libraries not found
    echo Install with: pacman -S mingw-w64-ucrt-x86_64-openssl
    echo.
)

REM Initialize submodules if needed
echo Checking git submodules...
git submodule update --init --recursive

REM Apply Windows-specific fixes
echo Applying Windows build fixes...
bash scripts/pre-build-windows.sh

REM Create build directory
if not exist target\static mkdir target\static

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

echo.
echo Static build complete. The executable includes all dependencies.
pause