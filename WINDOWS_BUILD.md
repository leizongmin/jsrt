# Windows Build Guide (MSYS2)

## Quick Start

### 1. Install MSYS2
- Download from: https://www.msys2.org/
- Run the installer, use default path `C:\msys64`

### 2. Install Build Tools
Run in Windows:
```cmd
setup-msys2.bat
```
This will automatically install the required packages.

### 3. Build Project
In MSYS2 MINGW64 terminal:
```bash
cd /c/Users/your-username/work/jsrt
./build-msys2.sh
```

## Detailed Steps

### Step 1: Install MSYS2

1. Visit https://www.msys2.org/
2. Download the installer (msys2-x86_64-*.exe)
3. Run the installer, install to default location `C:\msys64`
4. After installation, MSYS2 will automatically open a terminal window

### Step 2: Install Required Packages

Run in MSYS2 MINGW64 terminal:
```bash
# Update package database
pacman -Syu

# Install build tools
pacman -S --needed \
    mingw-w64-x86_64-gcc \
    mingw-w64-x86_64-cmake \
    make \
    git
```

### Step 3: Clone and Build Project

```bash
# Clone repository
git clone https://github.com/leizongmin/jsrt.git
cd jsrt

# Initialize submodules
git submodule update --init --recursive

# Run build script
./build-msys2.sh

# Or build manually
mkdir -p target/release
cd target/release
cmake -G "MSYS Makefiles" -DCMAKE_BUILD_TYPE=Release ../..
make -j$(nproc)
```

### Step 4: Run Tests

```bash
# In build directory
ctest --verbose

# Or run example
./jsrt.exe ../../examples/hello.js
```

## File Descriptions

- `setup-msys2.bat` - Windows batch script for automatic MSYS2 environment setup
- `build-msys2.sh` - MSYS2 build script that automatically installs dependencies and builds the project
- `test.bat` - Windows test script

## Build Types

Two build types are supported:
```bash
./build-msys2.sh Release  # Optimized build (default)
./build-msys2.sh Debug    # Debug build
```

## Output Location

After building, executables are located at:
- Release build: `target/msys2-release/jsrt.exe`
- Debug build: `target/msys2-debug/jsrt.exe`

## Common Issues

### 1. MSYS2 Not Found
Ensure MSYS2 is installed in the default location `C:\msys64`. If installed elsewhere, modify the path in the scripts.

### 2. Package Installation Failed
If package installation fails, try:
```bash
# Update package database
pacman -Syuu

# Clear cache
pacman -Scc

# Reinstall
pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake make git
```

### 3. CMake Configuration Failed
Ensure you're running in MINGW64 terminal, not MSYS or MINGW32:
- Start Menu → MSYS2 → MSYS2 MINGW64

### 4. Build Failed
Check if submodules are properly initialized:
```bash
git submodule update --init --recursive
```

### 5. Using in Windows Terminal
To use MSYS2 in Windows Terminal:
1. Open Windows Terminal settings
2. Add new profile
3. Set command line to: `C:\msys64\usr\bin\bash.exe -l -c "cd ~ && exec bash"`
4. Set starting directory to: `C:\msys64\home\%USERNAME%`

## Development Tips

### VSCode Integration
To use MSYS2 terminal in VSCode:
1. Install "C/C++" extension
2. Configure terminal in settings:
```json
"terminal.integrated.profiles.windows": {
  "MSYS2": {
    "path": "C:\\msys64\\usr\\bin\\bash.exe",
    "args": ["--login", "-i"],
    "env": {
      "MSYSTEM": "MINGW64",
      "CHERE_INVOKING": "1"
    }
  }
}
```

### Debugging
Using GDB for debugging:
```bash
# Build debug version
./build-msys2.sh Debug

# Use GDB
gdb target/msys2-debug/jsrt.exe
```

## Dependency Version Requirements

- GCC: 7.0+
- CMake: 3.16+
- Make: 3.81+
- Git: 2.0+