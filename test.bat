@echo off
setlocal

:: Check if build type is specified
set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release

:: Check if jsrt.exe exists
if not exist target\%BUILD_TYPE%\jsrt.exe (
    echo jsrt.exe not found in target\%BUILD_TYPE%\
    echo Please run build.bat first
    exit /b 1
)

:: Run tests
echo Running tests...
cd target\%BUILD_TYPE%
ctest --verbose
if errorlevel 1 (
    echo Some tests failed
    exit /b 1
)

echo.
echo All tests passed!
cd ..\..