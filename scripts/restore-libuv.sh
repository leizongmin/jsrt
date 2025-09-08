#!/bin/bash
# Script to restore libuv to original state

echo "Restoring libuv to original state..."

LIBUV_CMAKE="deps/libuv/CMakeLists.txt"
LIBUV_BACKUP="$LIBUV_CMAKE.original"

if [[ -f "$LIBUV_BACKUP" ]]; then
    echo "Restoring from backup: $LIBUV_BACKUP"
    cp "$LIBUV_BACKUP" "$LIBUV_CMAKE"
    echo "libuv CMakeLists.txt restored successfully"
else
    echo "No backup found, using git to restore..."
    cd deps/libuv
    git checkout CMakeLists.txt
    cd ../..
    echo "libuv restored from git"
fi

# Clean any build artifacts
if [[ -d "target/static" ]]; then
    echo "Cleaning build directory..."
    rm -rf target/static
fi

echo "libuv restoration completed"