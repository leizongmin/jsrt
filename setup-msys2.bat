@echo off
setlocal enabledelayedexpansion

echo ==========================================
echo    MSYS2 Setup Guide for JSRT
echo ==========================================
echo.

:: Check if MSYS2 is installed
set MSYS2_DIR=C:\msys64
if not exist "%MSYS2_DIR%\msys2.exe" (
    set MSYS2_DIR=C:\msys32
    if not exist "!MSYS2_DIR!\msys2.exe" (
        echo MSYS2 is not installed at the default location.
        echo.
        echo Please install MSYS2 from: https://www.msys2.org/
        echo.
        echo Installation steps:
        echo 1. Download the installer from https://www.msys2.org/
        echo 2. Run the installer and follow the instructions
        echo 3. Install to the default location (C:\msys64)
        echo.
        start https://www.msys2.org/
        pause
        exit /b 1
    )
)

echo MSYS2 found at: %MSYS2_DIR%
echo.

:: Create a batch file to run in MSYS2
echo Creating setup script for MSYS2...
(
echo #!/bin/bash
echo # Auto-generated setup script for JSRT
echo.
echo echo "==========================================="
echo echo "    Setting up JSRT build environment"
echo echo "==========================================="
echo echo
echo.
echo # Update package database
echo echo "Updating package database..."
echo pacman -Sy --noconfirm
echo.
echo # Install required packages
echo echo "Installing required packages..."
echo pacman -S --needed --noconfirm ^
echo     mingw-w64-x86_64-gcc ^
echo     mingw-w64-x86_64-cmake ^
echo     make ^
echo     git
echo.
echo echo
echo echo "==========================================="
echo echo "    Setup Complete!"
echo echo "==========================================="
echo echo
echo echo "Installed packages:"
echo echo "  - GCC: $(gcc --version | head -n1)"
echo echo "  - CMake: $(cmake --version | head -n1)"
echo echo "  - Make: $(make --version | head -n1)"
echo echo "  - Git: $(git --version)"
echo echo
echo echo "You can now build JSRT by running:"
echo echo "  ./build-msys2.sh"
echo echo
echo echo "Press Enter to exit..."
echo read
) > "%TEMP%\msys2_setup.sh"

:: Convert line endings to Unix format
powershell -Command "(Get-Content '%TEMP%\msys2_setup.sh') -replace \"`r`n\", \"`n\" | Set-Content -NoNewline '%TEMP%\msys2_setup.sh'"

echo.
echo ==========================================
echo    Launching MSYS2 MINGW64 Terminal
echo ==========================================
echo.
echo This will open MSYS2 and install required packages.
echo After installation completes, navigate to your JSRT directory:
echo.
echo   cd /c/Users/%USERNAME%/work/jsrt
echo   ./build-msys2.sh
echo.
pause

:: Launch MSYS2 MINGW64 terminal with the setup script
start "" "%MSYS2_DIR%\mingw64.exe" -c "cd '%cd:\=/% && bash %TEMP:\=/%/msys2_setup.sh"

echo.
echo MSYS2 terminal has been launched.
echo Follow the instructions in the terminal to complete setup.
echo.