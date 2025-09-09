#!/bin/bash
# Aggressive patch for libuv to force Windows-only build

echo "Patching libuv for Windows-only build..."

LIBUV_DIR="deps/libuv"
LIBUV_CMAKE="$LIBUV_DIR/CMakeLists.txt"

if [[ ! -f "$LIBUV_CMAKE" ]]; then
    echo "Error: libuv CMakeLists.txt not found at $LIBUV_CMAKE"
    exit 1
fi

# Create backup if not exists
if [[ ! -f "$LIBUV_CMAKE.original" ]]; then
    echo "Creating backup of original libuv CMakeLists.txt..."
    cp "$LIBUV_CMAKE" "$LIBUV_CMAKE.original"
fi

echo "Applying aggressive Windows-only patch to libuv..."

# Create the patched version
cat > "$LIBUV_CMAKE" << 'EOF'
# This is a PATCHED version of libuv CMakeLists.txt for Windows-only builds
# Original backed up as CMakeLists.txt.original

cmake_minimum_required(VERSION 3.10)
project(libuv LANGUAGES C)

# FORCE Windows detection - ignore all environment variables
set(CMAKE_SYSTEM_NAME "Windows")
set(WIN32 TRUE) 
set(MSYS FALSE)
set(CYGWIN FALSE)

# Force compiler definitions
add_definitions(-DWIN32=1 -D_WIN32=1 -D__WIN32__=1)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_definitions(-DWIN64=1 -D_WIN64=1 -D__WIN64__=1)  
endif()

include(CMakeParseArguments)
include(CMakePushCheckState)
include(CheckCCompilerFlag)
include(GNUInstalldirs)
include(CTest)

list(APPEND uv_cflags -fvisibility=hidden --std=gnu89)
list(APPEND uv_cflags -Wall -Wextra -Wstrict-prototypes)
list(APPEND uv_cflags -Wno-unused-parameter)

set(uv_sources
    src/fs-poll.c
    src/idna.c  
    src/inet.c
    src/random.c
    src/strscpy.c
    src/threadpool.c
    src/timer.c
    src/uv-common.c
    src/uv-data-getter-setters.c
    src/version.c)

# WINDOWS SOURCES ONLY - no Unix files
list(APPEND uv_sources
    src/win/async.c
    src/win/core.c
    src/win/detect-wakeup.c
    src/win/dl.c
    src/win/error.c
    src/win/fs.c
    src/win/fs-event.c
    src/win/getaddrinfo.c
    src/win/getnameinfo.c
    src/win/handle.c
    src/win/loop-watcher.c
    src/win/pipe.c
    src/win/thread.c
    src/win/poll.c
    src/win/process.c
    src/win/process-stdio.c
    src/win/signal.c
    src/win/snprintf.c
    src/win/stream.c
    src/win/tcp.c
    src/win/tty.c
    src/win/udp.c
    src/win/util.c
    src/win/winapi.c
    src/win/winsock.c)

# Headers
list(APPEND uv_sources
    include/uv.h
    include/uv/errno.h
    include/uv/threadpool.h
    include/uv/version.h
    include/uv/tree.h
    include/uv/win.h
    src/heap-inl.h
    src/idna.h
    src/queue.h
    src/strscpy.h
    src/uv-common.h
    src/win/atomicops-inl.h
    src/win/handle-inl.h
    src/win/internal.h
    src/win/req-inl.h
    src/win/stream-inl.h
    src/win/winapi.h
    src/win/winsock.h)

add_library(uv SHARED ${uv_sources})
add_library(uv_a STATIC ${uv_sources})

# Windows libraries
list(APPEND uv_libraries
    advapi32
    iphlpapi
    psapi
    shell32
    user32
    userenv  
    ws2_32
    dbghelp)

target_compile_definitions(uv PRIVATE BUILDING_UV_SHARED=1)
target_compile_options(uv PRIVATE ${uv_cflags})
target_include_directories(uv
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>)
target_link_libraries(uv ${uv_libraries})

target_compile_options(uv_a PRIVATE ${uv_cflags})
target_include_directories(uv_a
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>  
  PRIVATE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>)
target_link_libraries(uv_a ${uv_libraries})

# For compatibility with existing projects
set_target_properties(uv_a PROPERTIES OUTPUT_NAME uv)

message(STATUS "libuv configured for Windows-only build (patched)")
message(STATUS "libuv libraries: ${uv_libraries}")
EOF

echo "libuv successfully patched for Windows-only build"
echo "Original backed up as: $LIBUV_CMAKE.original"

# Also patch the include structure if needed
LIBUV_WIN_HEADER="$LIBUV_DIR/include/uv/win.h"
if [[ -f "$LIBUV_WIN_HEADER" ]]; then
    echo "libuv Windows header found: $LIBUV_WIN_HEADER"
else
    echo "Warning: libuv Windows header not found: $LIBUV_WIN_HEADER"
fi