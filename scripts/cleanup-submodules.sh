#!/bin/bash
# Script to clean up all submodule modifications after build

echo "Cleaning up all submodule modifications..."

# Reset libuv to original state (most important for our patches)
if [[ -d "deps/libuv" ]]; then
    cd deps/libuv
    git checkout -- . 2>/dev/null || true
    if [[ -f "CMakeLists.txt.original" ]]; then
        cp CMakeLists.txt.original CMakeLists.txt
        echo "libuv restored from backup"
    fi
    cd ../..
    echo "✓ libuv submodule reset"
fi

# Reset wamr
if [[ -d "deps/wamr" ]]; then
    cd deps/wamr
    git restore core/version.h 2>/dev/null || true
    git checkout -- . 2>/dev/null || true
    cd ../..
    echo "✓ wamr submodule reset"
fi

# Reset quickjs  
if [[ -d "deps/quickjs" ]]; then
    cd deps/quickjs
    git checkout -- . 2>/dev/null || true
    cd ../..
    echo "✓ quickjs submodule reset"
fi

# Reset llhttp
if [[ -d "deps/llhttp" ]]; then
    cd deps/llhttp
    git checkout -- . 2>/dev/null || true
    cd ../..
    echo "✓ llhttp submodule reset"
fi

echo "All submodules cleaned and reset to original state"
echo "Safe to commit - no submodule modifications will be included"