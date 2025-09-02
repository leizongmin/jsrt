#!/bin/bash

# MSYS2 build script for JSRT
# Run this script in MSYS2 MINGW64 terminal

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}===========================================${NC}"
echo -e "${GREEN}    JSRT Build Script for MSYS2${NC}"
echo -e "${GREEN}===========================================${NC}"
echo

# Check if running in MSYS2 MINGW64 environment
if [[ "$MSYSTEM" != "MINGW64" ]]; then
    echo -e "${YELLOW}Warning: Not running in MINGW64 environment${NC}"
    echo "Please run this script in MSYS2 MINGW64 terminal"
    echo "You are currently in: $MSYSTEM"
    echo
fi

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check for required tools
echo "Checking for required tools..."
MISSING_TOOLS=()

if ! command_exists gcc; then
    MISSING_TOOLS+=("mingw-w64-x86_64-gcc")
fi

if ! command_exists cmake; then
    MISSING_TOOLS+=("mingw-w64-x86_64-cmake")
fi

if ! command_exists make; then
    MISSING_TOOLS+=("make")
fi

if ! command_exists git; then
    MISSING_TOOLS+=("git")
fi

# Install missing tools
if [ ${#MISSING_TOOLS[@]} -gt 0 ]; then
    echo -e "${YELLOW}Missing tools detected. Installing:${NC}"
    for tool in "${MISSING_TOOLS[@]}"; do
        echo "  - $tool"
    done
    echo
    echo "Running: pacman -S --needed ${MISSING_TOOLS[*]}"
    pacman -S --needed --noconfirm "${MISSING_TOOLS[@]}"
    echo
fi

# Show tool versions
echo "Tool versions:"
gcc --version | head -n1
cmake --version | head -n1
make --version | head -n1
echo

# Get build type from argument
BUILD_TYPE="${1:-Release}"
echo "Build type: $BUILD_TYPE"
echo

# Initialize git submodules if needed
if [ ! -f "deps/quickjs/quickjs.c" ] || [ ! -f "deps/libuv/CMakeLists.txt" ]; then
    echo "Initializing git submodules..."
    git submodule update --init --recursive
    echo
fi

# Create build directory
BUILD_DIR="target/msys2-${BUILD_TYPE,,}"
mkdir -p "$BUILD_DIR"

# Configure with CMake
echo -e "${GREEN}Configuring with CMake...${NC}"
cd "$BUILD_DIR"
cmake -G "MSYS Makefiles" \
      -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
      -DCMAKE_C_COMPILER=gcc \
      -DCMAKE_CXX_COMPILER=g++ \
      ../..

if [ $? -ne 0 ]; then
    echo -e "${RED}CMake configuration failed${NC}"
    exit 1
fi

# Build
echo
echo -e "${GREEN}Building...${NC}"
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed${NC}"
    exit 1
fi

# Run tests
echo
echo -e "${GREEN}Running tests...${NC}"
ctest --verbose

if [ $? -ne 0 ]; then
    echo -e "${YELLOW}Some tests failed${NC}"
else
    echo -e "${GREEN}All tests passed!${NC}"
fi

# Show result
echo
echo -e "${GREEN}===========================================${NC}"
echo -e "${GREEN}    Build Complete!${NC}"
echo -e "${GREEN}===========================================${NC}"
echo
echo "Executable location: $(pwd)/jsrt.exe"
echo
echo "You can run it with:"
echo "  $(pwd)/jsrt.exe ../../examples/hello.js"
echo