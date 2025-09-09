# Script to fix libuv Windows build issues in MSYS2 environment
# This script patches libuv's platform detection to force Windows build

message(STATUS "Applying Windows build fixes for libuv...")

# Check if we're in a problematic build environment
if(WIN32 AND (DEFINED ENV{MSYSTEM} OR DEFINED ENV{CYGWIN}))
    message(STATUS "MSYS2/Cygwin environment detected, applying libuv Windows build fixes...")
    
    # Force Windows platform detection variables
    set(CMAKE_SYSTEM_NAME "Windows" CACHE STRING "System name" FORCE)
    set(WIN32 TRUE CACHE BOOL "WIN32 flag" FORCE)
    set(MSYS FALSE CACHE BOOL "MSYS flag" FORCE) 
    set(CYGWIN FALSE CACHE BOOL "CYGWIN flag" FORCE)
    
    # Clear MSYS2 environment variables temporarily
    set(ENV{MSYSTEM} "")
    set(ENV{CYGWIN} "")
    
    # Set compiler definitions for Windows
    add_compile_definitions(WIN32=1 _WIN32=1 __WIN32__=1)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        add_compile_definitions(WIN64=1 _WIN64=1 __WIN64__=1)
    endif()
    
    message(STATUS "Windows build environment configured for libuv")
endif()

# Function to check if libuv was built correctly
function(verify_libuv_build)
    if(TARGET uv_a)
        get_target_property(UV_SOURCES uv_a SOURCES)
        if(UV_SOURCES)
            # Check for problematic Unix source files
            foreach(source ${UV_SOURCES})
                if(source MATCHES "unix/cygwin" OR source MATCHES "unix/bsd-ifaddrs")
                    message(WARNING "libuv included problematic Unix source: ${source}")
                endif()
            endforeach()
        endif()
    endif()
endfunction()