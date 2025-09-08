cmake_minimum_required(VERSION 3.10)
project(jsrt C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CFLAGS "-Wall -Wextra")

# Check if git submodules are initialized
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/deps/quickjs/quickjs.h")
    message(FATAL_ERROR "Git submodules are not initialized. Please run: git submodule update --init --recursive")
endif()
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/deps/libuv/CMakeLists.txt")
    message(FATAL_ERROR "Git submodules are not initialized. Please run: git submodule update --init --recursive")
endif()
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/deps/llhttp/package.json")
    message(FATAL_ERROR "Git submodules are not initialized. Please run: git submodule update --init --recursive")
endif()
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/deps/wamr/CMakeLists.txt")
    message(FATAL_ERROR "Git submodules are not initialized. Please run: git submodule update --init --recursive")
endif()

# optimization flags
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s")
endif()

# debug flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DDEBUG")

    # enable address sanitizer
    if(ENABLE_ASAN)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    endif()
endif()

# enable coverage
if(ENABLE_COVERAGE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

# Find libffi for enhanced FFI support
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBFFI QUIET libffi)
endif()

# Optional: Enable libffi integration
# Note: On macOS, libffi will be dynamically linked even in static builds since static libffi is not available
# On Linux, static libffi is available through libffi-dev/libffi-devel packages
option(ENABLE_LIBFFI "Enable libffi integration for robust FFI calling conventions" ${LIBFFI_FOUND})

if(JSRT_STATIC_OPENSSL AND ENABLE_LIBFFI AND LIBFFI_FOUND)
    if(APPLE)
        message(STATUS "libffi will be dynamically linked in static builds (static libffi not available on macOS)")
    else()
        message(STATUS "libffi can be statically linked in static builds on this platform")
    endif()
endif()

if(ENABLE_LIBFFI AND LIBFFI_FOUND)
    message(STATUS "libffi support enabled")
    add_definitions(-DHAVE_LIBFFI=1)
    include_directories(${LIBFFI_INCLUDE_DIRS})
    set(FFI_LIBRARIES ${LIBFFI_LIBRARIES})
else()
    message(STATUS "libffi support disabled - using basic calling conventions")
    set(FFI_LIBRARIES "")
endif()

include_directories(src deps/quickjs deps/libuv/include deps/libuv/src deps/readline deps/llhttp/build deps/wamr/core/iwasm/include)

# Read jsrt version from VERSION file
file(STRINGS VERSION JSRT_VERSION)

# build libquickjs
set(QUICKJS_LIB_FILES
    deps/quickjs/cutils.c
    deps/quickjs/dtoa.c
    deps/quickjs/libregexp.c
    deps/quickjs/libunicode.c
    deps/quickjs/quickjs-libc.c
    deps/quickjs/quickjs.c
    )
add_library(quickjs STATIC ${QUICKJS_LIB_FILES})
file(STRINGS deps/quickjs/VERSION QUICKJS_VERSION)
target_compile_definitions(quickjs PRIVATE CONFIG_VERSION="${QUICKJS_VERSION}" _GNU_SOURCE)

# In debug builds, disable the strict GC assertion that can fail with complex runtime environments
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(quickjs PRIVATE NDEBUG)
endif()

# build qjs and qjsc, use flag -DBUILD_QJS=ON to enable
option(BUILD_QJS "Build qjs and qjsc executable" OFF)
if(BUILD_QJS)
    add_executable(qjsc deps/quickjs/qjsc.c deps/quickjs/quickjs-libc.c)
    target_compile_definitions(qjsc PRIVATE CONFIG_VERSION="${QUICKJS_VERSION}" _GNU_SOURCE)
    target_link_libraries(qjsc quickjs)
    # build qjsrepl.c
    add_custom_command(OUTPUT qjsrepl.c
    COMMAND qjsc -c -o qjsrepl.c -m ${CMAKE_CURRENT_SOURCE_DIR}/deps/quickjs/repl.js
    DEPENDS qjsc)
    set_source_files_properties(qjsrepl.c PROPERTIES GENERATED TRUE)
    # build qjs
    add_executable(qjs deps/quickjs/qjs.c qjsrepl.c deps/quickjs/quickjs-libc.c)
    target_compile_definitions(qjs PRIVATE CONFIG_VERSION="${QUICKJS_VERSION}" _GNU_SOURCE)
    target_link_libraries(qjs quickjs)
endif()

# build libuv
# Force libuv to use Windows build on MinGW to avoid Cygwin path conflicts
if(WIN32)
    # Override system detection variables before libuv processes them
    set(CMAKE_SYSTEM_NAME "Windows" CACHE STRING "Force Windows system name for libuv" FORCE)
    set(WIN32 TRUE CACHE BOOL "Force WIN32 for libuv" FORCE)
    set(MSYS FALSE CACHE BOOL "Force MSYS to FALSE for libuv" FORCE)
    set(CYGWIN FALSE CACHE BOOL "Force CYGWIN to FALSE for libuv" FORCE)
    
    # Also unset any MSYS detection that might have happened
    unset(MSYS CACHE)
    unset(CYGWIN CACHE)
    
    # Force libuv to treat this as pure Windows build
    set(CMAKE_HOST_SYSTEM_NAME "Windows" CACHE STRING "Override host system for libuv" FORCE)
    set(CMAKE_HOST_WIN32 TRUE CACHE BOOL "Force host WIN32 for libuv" FORCE)
    
    # Additional forced variables for libuv platform detection
    set(__MSYS__ FALSE CACHE BOOL "Force MSYS detection off" FORCE)
    set(__CYGWIN__ FALSE CACHE BOOL "Force CYGWIN detection off" FORCE)
    set(__MINGW32__ TRUE CACHE BOOL "Force MinGW32 detection on" FORCE)
    set(__MINGW64__ TRUE CACHE BOOL "Force MinGW64 detection on" FORCE)
    
    # Set environment variables that libuv checks
    set(ENV{MSYSTEM} "")
    set(ENV{CYGWIN} "")
    
    # Force compiler predefined macros for Windows
    add_definitions(-DWIN32=1 -D_WIN32=1 -D__WIN32__=1)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        add_definitions(-DWIN64=1 -D_WIN64=1 -D__WIN64__=1)
    endif()
    
    message(STATUS "Windows detected: Forcing libuv to use Windows build path (not Cygwin/MSYS/Unix)")
    message(STATUS "CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
    message(STATUS "WIN32: ${WIN32}")
    message(STATUS "MSYS: ${MSYS}")
    message(STATUS "CYGWIN: ${CYGWIN}")
endif()

# Apply Windows fixes for libuv before building
if(WIN32)
    include(scripts/fix-libuv-windows.cmake)
endif()

add_subdirectory(deps/libuv)

# Verify and fix libuv build after configuration
if(WIN32)
    # Get the uv_a target properties for diagnostics
    get_target_property(UV_SOURCES uv_a SOURCES)
    if(UV_SOURCES)
        list(LENGTH UV_SOURCES UV_SOURCES_COUNT)
        message(STATUS "libuv configured with ${UV_SOURCES_COUNT} source files")
        
        # Check for problematic files
        foreach(source ${UV_SOURCES})
            if(source MATCHES "unix/(cygwin|bsd-ifaddrs)")
                message(WARNING "libuv included problematic Unix source: ${source}")
                message(STATUS "This may cause build failures. Consider using libuv Windows-only build.")
            endif()
        endforeach()
    endif()
endif()

# build WAMR
# Configure WAMR for minimal interpreter-only build
# Let WAMR auto-detect platform (will use CMAKE_HOST_SYSTEM_NAME)
set(WAMR_BUILD_INTERP 1)           # Enable interpreter
set(WAMR_BUILD_AOT 0)              # Disable AOT for minimal build
set(WAMR_BUILD_JIT 0)              # Disable JIT for minimal build
set(WAMR_BUILD_FAST_JIT 0)         # Disable Fast JIT
set(WAMR_BUILD_LIBC_BUILTIN 1)    # Use built-in libc subset
set(WAMR_BUILD_LIBC_WASI 0)       # Disable WASI initially
set(WAMR_BUILD_MINI_LOADER 1)     # Enable mini loader for smaller size
set(WAMR_BUILD_BULK_MEMORY 1)     # Enable bulk memory operations
set(WAMR_BUILD_MEMORY64 0)        # Disable 64-bit memory for simplicity
set(WAMR_BUILD_MULTI_MODULE 0)    # Disable multi-module for simplicity
set(WAMR_BUILD_SHARED_MEMORY 0)   # Disable shared memory for now
set(WAMR_BUILD_SIMD 0)            # Disable SIMD to ensure correct assembly file selection
set(WAMR_BUILD_INVOKE_NATIVE_GENERAL 1)  # Use C implementation instead of assembly for MinGW compatibility

# Windows specific configuration for WAMR
if(WIN32)
    # Explicitly set WAMR platform for Windows to ensure correct assembly file selection
    # Use CACHE FORCE to ensure this variable is available in subdirectories
    set(WAMR_BUILD_PLATFORM "windows" CACHE STRING "WAMR build platform" FORCE)
    message(STATUS "Windows detected: Setting WAMR_BUILD_PLATFORM to 'windows'")
    
    # Explicitly set WAMR target architecture for correct invokeNative selection
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(WAMR_BUILD_TARGET "X86_64" CACHE STRING "WAMR build target" FORCE)
        message(STATUS "64-bit Windows: Setting WAMR_BUILD_TARGET to 'X86_64'")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(WAMR_BUILD_TARGET "X86_32" CACHE STRING "WAMR build target" FORCE)
        message(STATUS "32-bit Windows: Setting WAMR_BUILD_TARGET to 'X86_32'")
    endif()
    
    # Ensure MINGW variable is set for MinGW builds
    # WAMR relies on this variable to select the correct assembly files
    # Use CACHE FORCE to ensure this propagates to subdirectories
    if(NOT MSVC AND (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID MATCHES ".*[Mm]ingw.*"))
        set(MINGW TRUE CACHE BOOL "MinGW build detected" FORCE)
        message(STATUS "MinGW detected: Setting MINGW variable for WAMR assembly file selection")
    endif()
    
    if(NOT MSVC)
        # For MinGW builds, disable hardware bound checking to avoid MSVC-specific __try/__except
        # MinGW doesn't support structured exception handling, only MSVC does
        set(WAMR_DISABLE_HW_BOUND_CHECK 1)
        message(STATUS "MinGW detected: Disabling WAMR hardware bound checking to avoid MSVC-specific SEH")
    endif()

    add_subdirectory(deps/wamr)
endif()

# For non-Windows platforms, add WAMR with default settings
if(NOT WIN32)
    add_subdirectory(deps/wamr)
endif()

# Fix WAMR link libraries for Windows
# WAMR unconditionally links -ldl which doesn't exist on Windows
if(WIN32)
    # Get current link libraries from vmlib target
    get_target_property(WAMR_LINK_LIBS vmlib INTERFACE_LINK_LIBRARIES)
    if(WAMR_LINK_LIBS)
        # Remove -ldl from the list
        list(REMOVE_ITEM WAMR_LINK_LIBS "-ldl")
        # Set the cleaned link libraries back
        set_target_properties(vmlib PROPERTIES INTERFACE_LINK_LIBRARIES "${WAMR_LINK_LIBS}")
    endif()
    message(STATUS "Windows detected: Removed -ldl from WAMR link libraries")
endif()

# build llhttp (minimal implementation)
# Check if llhttp generated files exist, if not, build them
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/deps/llhttp/build/c/llhttp.c")
    message(STATUS "llhttp build files not found, attempting to build llhttp...")
    
    # Cross-platform approach to finding and running npm
    set(NPM_COMMAND "")
    set(NPM_INSTALL_COMMAND "")
    set(NPM_BUILD_COMMAND "")
    
    if(WIN32)
        # On Windows, try multiple approaches to find working npm
        find_program(NPM_EXECUTABLE npm HINTS "C:/Program Files/nodejs" "C:/Program Files (x86)/nodejs")
        find_program(NODE_EXECUTABLE node HINTS "C:/Program Files/nodejs" "C:/Program Files (x86)/nodejs")
        find_program(CMD_EXECUTABLE cmd)
        find_program(SH_EXECUTABLE sh)
        
        # Check if npm is a shell script (common in MSYS2/MinGW environments)
        set(NPM_IS_SHELL_SCRIPT FALSE)
        if(NPM_EXECUTABLE)
            file(READ "${NPM_EXECUTABLE}" NPM_FILE_CONTENT LIMIT 100)
            if(NPM_FILE_CONTENT MATCHES "^#!/")
                set(NPM_IS_SHELL_SCRIPT TRUE)
                message(STATUS "Detected npm as shell script: ${NPM_EXECUTABLE}")
            endif()
        endif()
        
        # Strategy 1: Use node.exe directly with npm-cli.js (most reliable)
        find_file(NPM_CLI_JS npm-cli.js HINTS 
            "C:/Program Files/nodejs/node_modules/npm/bin" 
            "C:/Program Files (x86)/nodejs/node_modules/npm/bin"
            "${NPM_EXECUTABLE}/../lib/node_modules/npm/bin"
            "${NPM_EXECUTABLE}/../../lib/node_modules/npm/bin")
        
        if(NODE_EXECUTABLE AND NPM_CLI_JS)
            set(NPM_INSTALL_COMMAND "${NODE_EXECUTABLE}" "${NPM_CLI_JS}" "install")
            set(NPM_BUILD_COMMAND "${NODE_EXECUTABLE}" "${NPM_CLI_JS}" "run" "build")
            message(STATUS "Found node at: ${NODE_EXECUTABLE} with npm-cli.js: ${NPM_CLI_JS}")
        elseif(NPM_EXECUTABLE AND NOT NPM_IS_SHELL_SCRIPT AND CMD_EXECUTABLE)
            # Strategy 2: Use cmd.exe with npm.exe (native Windows npm)
            set(NPM_INSTALL_COMMAND "${CMD_EXECUTABLE}" "/c" "${NPM_EXECUTABLE}" "install")
            set(NPM_BUILD_COMMAND "${CMD_EXECUTABLE}" "/c" "${NPM_EXECUTABLE}" "run" "build")
            message(STATUS "Found npm executable at: ${NPM_EXECUTABLE} (using cmd.exe)")
        elseif(NPM_EXECUTABLE AND NPM_IS_SHELL_SCRIPT AND SH_EXECUTABLE)
            # Strategy 3: Use sh.exe for shell script npm (MSYS2/MinGW)
            set(NPM_INSTALL_COMMAND "${SH_EXECUTABLE}" "-c" "${NPM_EXECUTABLE} install")
            set(NPM_BUILD_COMMAND "${SH_EXECUTABLE}" "-c" "${NPM_EXECUTABLE} run build")
            message(STATUS "Found npm shell script at: ${NPM_EXECUTABLE} (using sh.exe)")
        endif()
    else()
        # On Unix-like systems, npm should work normally
        find_program(NPM_EXECUTABLE npm)
        if(NPM_EXECUTABLE)
            set(NPM_INSTALL_COMMAND ${NPM_EXECUTABLE} install)
            set(NPM_BUILD_COMMAND ${NPM_EXECUTABLE} run build)
            message(STATUS "Found npm at: ${NPM_EXECUTABLE}")
        endif()
    endif()
    
    if(NPM_INSTALL_COMMAND)
        # Try npm install
        execute_process(
            COMMAND ${NPM_INSTALL_COMMAND}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/deps/llhttp
            RESULT_VARIABLE NPM_INSTALL_RESULT
            OUTPUT_VARIABLE NPM_INSTALL_OUTPUT
            ERROR_VARIABLE NPM_INSTALL_ERROR
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_STRIP_TRAILING_WHITESPACE
        )
        
        if(NOT NPM_INSTALL_RESULT EQUAL 0)
            message(STATUS "npm install failed with code: ${NPM_INSTALL_RESULT}")
            message(STATUS "npm install output: ${NPM_INSTALL_OUTPUT}")
            message(STATUS "npm install error: ${NPM_INSTALL_ERROR}")
            
            # On Windows, provide specific troubleshooting
            if(WIN32)
                message(FATAL_ERROR 
                    "Failed to build llhttp dependencies on Windows.\n"
                    "This commonly happens due to Node.js/npm execution issues in Windows build environments.\n"
                    "\n"
                    "Possible solutions:\n"
                    "  1. Use WSL (Windows Subsystem for Linux) for building: wsl git clone ... && wsl make\n"
                    "  2. Use pre-built binaries from the releases page\n"
                    "  3. Manually run in Command Prompt: cd deps\\llhttp && npm install && npm run build\n"
                    "  4. Install Node.js from nodejs.org (not via package managers like chocolatey)\n"
                    "\n"
                    "Debug info:\n"
                    "  npm executable: ${NPM_EXECUTABLE}\n"
                    "  node executable: ${NODE_EXECUTABLE}\n"
                    "  npm-cli.js: ${NPM_CLI_JS}\n"
                    "  npm is shell script: ${NPM_IS_SHELL_SCRIPT}\n"
                    "  npm install command: ${NPM_INSTALL_COMMAND}\n"
                    "  exit code: ${NPM_INSTALL_RESULT}\n"
                    "  output: ${NPM_INSTALL_OUTPUT}\n"
                    "  error: ${NPM_INSTALL_ERROR}")
            else()
                message(FATAL_ERROR 
                    "Failed to build llhttp dependencies.\n"
                    "Please either:\n"
                    "  1. Install Node.js and npm, then run: npm install && npm run build in deps/llhttp\n"
                    "  2. Or use a pre-built release that includes the generated llhttp files")
            endif()
        endif()
        
        # Try npm run build
        execute_process(
            COMMAND ${NPM_BUILD_COMMAND}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/deps/llhttp
            RESULT_VARIABLE NPM_BUILD_RESULT
            OUTPUT_VARIABLE NPM_BUILD_OUTPUT
            ERROR_VARIABLE NPM_BUILD_ERROR
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_STRIP_TRAILING_WHITESPACE
        )
        
        if(NOT NPM_BUILD_RESULT EQUAL 0)
            message(STATUS "npm run build failed with code: ${NPM_BUILD_RESULT}")
            message(STATUS "npm run build output: ${NPM_BUILD_OUTPUT}")
            message(STATUS "npm run build error: ${NPM_BUILD_ERROR}")
            
            if(WIN32)
                message(FATAL_ERROR 
                    "Failed to generate llhttp C files on Windows.\n"
                    "This requires Node.js with TypeScript support.\n"
                    "\n"
                    "Recommended solutions:\n"
                    "  1. Use WSL for building: wsl git clone ... && wsl make\n"
                    "  2. Use pre-built binaries from the releases page\n"
                    "  3. Build on Linux/macOS where Node.js toolchain works more reliably\n"
                    "\n"
                    "Debug info:\n"
                    "  npm build command: ${NPM_BUILD_COMMAND}\n"
                    "  exit code: ${NPM_BUILD_RESULT}\n"
                    "  output: ${NPM_BUILD_OUTPUT}\n"
                    "  error: ${NPM_BUILD_ERROR}")
            else()
                message(FATAL_ERROR 
                    "Failed to generate llhttp C files.\n"
                    "This requires Node.js with TypeScript support.\n"
                    "Please either:\n"
                    "  1. Ensure Node.js 18+ is installed and run: npm install && npm run build in deps/llhttp\n"
                    "  2. Or use a pre-built release that includes the generated llhttp files")
            endif()
        endif()
        
        message(STATUS "llhttp build completed successfully")
    else()
        # No npm found
        if(WIN32)
            message(FATAL_ERROR 
                "llhttp build files are missing and Node.js/npm is not available on Windows.\n"
                "The llhttp library requires generated C files that are built from TypeScript sources.\n"
                "\n"
                "Recommended solutions:\n"
                "  1. Install Node.js from https://nodejs.org/\n"
                "  2. Use pre-built binaries from GitHub releases\n"
                "  3. Use WSL (Windows Subsystem for Linux): wsl git clone ... && wsl make\n"
                "  4. Build on Linux/macOS where the toolchain is more reliable\n"
                "\n"
                "If Node.js is installed but not found, check your PATH environment variable.")
        else()
            message(FATAL_ERROR 
                "llhttp build files are missing and npm is not available.\n"
                "The llhttp library requires generated C files that are built from TypeScript sources.\n"
                "Please either:\n"
                "  1. Install Node.js and npm, then run: git submodule update --init --recursive && cd deps/llhttp && npm install && npm run build\n"
                "  2. Or use a pre-built release that includes the generated llhttp files")
        endif()
    endif()
endif()

add_library(llhttp STATIC
    deps/llhttp/build/c/llhttp.c
    deps/llhttp/src/native/api.c
    deps/llhttp/src/native/http.c
)
target_include_directories(llhttp PUBLIC deps/llhttp/build deps/llhttp/src/native)
target_compile_definitions(llhttp PRIVATE _GNU_SOURCE)

# Add option for Node.js compatibility
option(JSRT_NODE_COMPAT "Enable Node.js compatibility layer" ON)

# Add option for static OpenSSL linking
option(JSRT_STATIC_OPENSSL "Enable static OpenSSL linking instead of runtime loading" OFF)

# Find OpenSSL for static linking when enabled
set(OPENSSL_LIBRARIES "")
set(OPENSSL_INCLUDE_DIRS "")
if(JSRT_STATIC_OPENSSL)
    if(WIN32)
        # Windows-specific OpenSSL static linking
        message(STATUS "Looking for OpenSSL static libraries on Windows...")
        
        # Common MSYS2 OpenSSL installation paths
        set(OPENSSL_SEARCH_PATHS
            "C:/msys64/ucrt64/lib"
            "C:/msys64/mingw64/lib"
            "C:/msys64/usr/lib"
            "${CMAKE_INSTALL_PREFIX}/lib"
            "${CMAKE_PREFIX_PATH}/lib"
        )
        
        # Look for static library files in MSYS2 paths
        find_library(OPENSSL_CRYPTO_STATIC
            NAMES libcrypto.a crypto.lib libcrypto-3-x64.lib libcrypto-3.lib
            PATHS ${OPENSSL_SEARCH_PATHS}
            NO_DEFAULT_PATH
        )
        
        find_library(OPENSSL_SSL_STATIC  
            NAMES libssl.a ssl.lib libssl-3-x64.lib libssl-3.lib
            PATHS ${OPENSSL_SEARCH_PATHS}
            NO_DEFAULT_PATH
        )
        
        # Look for include directory
        find_path(OPENSSL_INCLUDE_DIR
            NAMES openssl/opensslconf.h openssl/ssl.h
            PATHS
                "C:/msys64/ucrt64/include"
                "C:/msys64/mingw64/include" 
                "C:/msys64/usr/include"
                "${CMAKE_INSTALL_PREFIX}/include"
                "${CMAKE_PREFIX_PATH}/include"
            NO_DEFAULT_PATH
        )
        
        if(OPENSSL_CRYPTO_STATIC AND OPENSSL_SSL_STATIC AND OPENSSL_INCLUDE_DIR)
            set(OPENSSL_LIBRARIES ${OPENSSL_SSL_STATIC} ${OPENSSL_CRYPTO_STATIC})
            set(OPENSSL_INCLUDE_DIRS ${OPENSSL_INCLUDE_DIR})
            
            # Add required Windows system libraries for static OpenSSL
            list(APPEND OPENSSL_LIBRARIES ws2_32 crypt32 bcrypt)
            
            message(STATUS "Found OpenSSL static libraries for Windows:")
            message(STATUS "  SSL: ${OPENSSL_SSL_STATIC}")  
            message(STATUS "  Crypto: ${OPENSSL_CRYPTO_STATIC}")
            message(STATUS "  Include: ${OPENSSL_INCLUDE_DIR}")
        else()
            message(STATUS "OpenSSL static libraries not found on Windows, trying fallback...")
            find_package(OpenSSL QUIET)
            if(OpenSSL_FOUND)
                message(STATUS "Using OpenSSL dynamic libraries as fallback")
                set(OPENSSL_LIBRARIES OpenSSL::Crypto OpenSSL::SSL)
                set(OPENSSL_INCLUDE_DIRS ${OPENSSL_INCLUDE_DIR})
            else()
                message(WARNING "JSRT_STATIC_OPENSSL enabled but OpenSSL not found - falling back to dynamic loading")
                set(JSRT_STATIC_OPENSSL OFF)
            endif()
        endif()
    else()
        # Unix/Linux/macOS static linking (existing logic)
        find_package(OpenSSL QUIET)
        if(OpenSSL_FOUND)
            message(STATUS "OpenSSL found for static linking: ${OPENSSL_VERSION}")
            
            # For true static linking, we need to find the .a files instead of using imported targets
            get_target_property(OPENSSL_CRYPTO_LIB OpenSSL::Crypto IMPORTED_LOCATION)
            get_target_property(OPENSSL_SSL_LIB OpenSSL::SSL IMPORTED_LOCATION)
            
            # Convert .dylib paths to .a paths for static linking
            if(OPENSSL_CRYPTO_LIB)
                string(REPLACE ".dylib" ".a" OPENSSL_CRYPTO_STATIC ${OPENSSL_CRYPTO_LIB})
                string(REPLACE ".so" ".a" OPENSSL_CRYPTO_STATIC ${OPENSSL_CRYPTO_STATIC})
            endif()
            if(OPENSSL_SSL_LIB) 
                string(REPLACE ".dylib" ".a" OPENSSL_SSL_STATIC ${OPENSSL_SSL_LIB})
                string(REPLACE ".so" ".a" OPENSSL_SSL_STATIC ${OPENSSL_SSL_STATIC})
            endif()
            
            # Check if static libraries exist
            if(EXISTS "${OPENSSL_CRYPTO_STATIC}" AND EXISTS "${OPENSSL_SSL_STATIC}")
                set(OPENSSL_LIBRARIES ${OPENSSL_SSL_STATIC} ${OPENSSL_CRYPTO_STATIC})
                message(STATUS "Using OpenSSL static libraries:")
                message(STATUS "  SSL: ${OPENSSL_SSL_STATIC}")
                message(STATUS "  Crypto: ${OPENSSL_CRYPTO_STATIC}")
            else()
                message(WARNING "OpenSSL static libraries not found, using dynamic linking")
                message(STATUS "  Looked for: ${OPENSSL_SSL_STATIC}")
                message(STATUS "  Looked for: ${OPENSSL_CRYPTO_STATIC}")
                set(OPENSSL_LIBRARIES OpenSSL::Crypto OpenSSL::SSL)
            endif()
            
            set(OPENSSL_INCLUDE_DIRS ${OPENSSL_INCLUDE_DIR})
        else()
            # Try pkg-config as fallback
            find_package(PkgConfig QUIET)
            if(PkgConfig_FOUND)
                pkg_check_modules(OPENSSL_PKG QUIET openssl)
                if(OPENSSL_PKG_FOUND)
                    message(STATUS "OpenSSL found via pkg-config for static linking")
                    set(OPENSSL_LIBRARIES ${OPENSSL_PKG_LIBRARIES})
                    set(OPENSSL_INCLUDE_DIRS ${OPENSSL_PKG_INCLUDE_DIRS})
                endif()
            endif()
            
            if(NOT OPENSSL_LIBRARIES)
                message(WARNING "JSRT_STATIC_OPENSSL enabled but OpenSSL not found - falling back to dynamic loading")
                set(JSRT_STATIC_OPENSSL OFF)
            endif()
        endif()
    endif()
endif()

# Try to find system readline with robust detection
set(READLINE_WORKING FALSE)

# First try pkg-config
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(READLINE QUIET readline)
    if(READLINE_FOUND)
        set(READLINE_TEST_LIBRARIES ${READLINE_LIBRARIES})
        set(READLINE_TEST_INCLUDE_DIRS ${READLINE_INCLUDE_DIRS})
    endif()
endif()

# If pkg-config didn't work, try manual detection
if(NOT READLINE_FOUND)
    find_library(READLINE_LIBRARY NAMES readline)
    find_path(READLINE_INCLUDE_DIR readline/readline.h)
    if(READLINE_LIBRARY AND READLINE_INCLUDE_DIR)
        set(READLINE_TEST_LIBRARIES ${READLINE_LIBRARY})
        set(READLINE_TEST_INCLUDE_DIRS ${READLINE_INCLUDE_DIR})
        
        # Also try to find termcap/ncurses which readline usually depends on
        find_library(TERMCAP_LIBRARY NAMES termcap ncurses)
        if(TERMCAP_LIBRARY)
            list(APPEND READLINE_TEST_LIBRARIES ${TERMCAP_LIBRARY})
        endif()
        
        set(READLINE_FOUND TRUE)
    endif()
endif()

# If we found readline candidates, test if they actually work
if(READLINE_FOUND)
    include(CheckCSourceCompiles)
    set(CMAKE_REQUIRED_LIBRARIES ${READLINE_TEST_LIBRARIES})
    set(CMAKE_REQUIRED_INCLUDES ${READLINE_TEST_INCLUDE_DIRS})
    
    check_c_source_compiles("
        #include <stdio.h>
        #include <readline/readline.h>
        #include <readline/history.h>
        int main() {
            char *line = readline(\"test: \");
            if (line) {
                add_history(line);
                rl_on_new_line();
                rl_redisplay();
                free(line);
            }
            return 0;
        }
    " READLINE_COMPILES_AND_LINKS)
    
    if(READLINE_COMPILES_AND_LINKS)
        set(READLINE_WORKING TRUE)
        set(READLINE_LIBRARIES ${READLINE_TEST_LIBRARIES})
        set(READLINE_INCLUDE_DIRS ${READLINE_TEST_INCLUDE_DIRS})
        message(STATUS "Using system readline (verified working)")
    else()
        message(STATUS "System readline found but doesn't work properly")
        set(READLINE_WORKING FALSE)
    endif()
    
    # Clean up test variables
    unset(CMAKE_REQUIRED_LIBRARIES)
    unset(CMAKE_REQUIRED_INCLUDES)
endif()

if(READLINE_WORKING)
    # Use system readline
    add_library(readline INTERFACE)
    target_link_libraries(readline INTERFACE ${READLINE_LIBRARIES})
    target_include_directories(readline INTERFACE ${READLINE_INCLUDE_DIRS})
    target_compile_definitions(readline INTERFACE HAVE_READLINE=1)
else()
    # Provide a minimal readline replacement
    message(STATUS "Using minimal readline fallback")
    add_library(readline STATIC src/minimal_readline.c)
    target_include_directories(readline PUBLIC src)
    target_compile_definitions(readline PUBLIC MINIMAL_READLINE=1)
endif()

# Platform-specific libraries
if(WIN32)
    # Windows doesn't need dl, m is built into MSVCRT, pthread is handled by libuv
    # For static builds, we may need additional system libraries
    set(JSRT_PLATFORM_LIBS)
    
    # Add additional Windows libraries for static linking
    if(JSRT_STATIC_OPENSSL)
        # These are already added to OPENSSL_LIBRARIES but ensure they're available
        list(APPEND JSRT_PLATFORM_LIBS ws2_32 crypt32 bcrypt)
    endif()
else()
    # Unix/Linux/macOS need these libraries
    set(JSRT_PLATFORM_LIBS dl m pthread)
endif()

# build jsrt
file(GLOB_RECURSE JSRT_STD_FILES "src/std/*.c")
file(GLOB_RECURSE JSRT_UTIL_FILES "src/util/*.c")
file(GLOB_RECURSE JSRT_HTTP_FILES "src/http/*.c")
file(GLOB_RECURSE JSRT_WASM_FILES "src/wasm/*.c")

# For static OpenSSL builds, exclude modules that need runtime loading
if(JSRT_STATIC_OPENSSL)
    list(REMOVE_ITEM JSRT_STD_FILES 
        "${CMAKE_SOURCE_DIR}/src/std/crypto_digest.c"
        "${CMAKE_SOURCE_DIR}/src/std/crypto_hmac.c" 
        "${CMAKE_SOURCE_DIR}/src/std/crypto_kdf.c"
        "${CMAKE_SOURCE_DIR}/src/std/crypto_rsa.c"
        "${CMAKE_SOURCE_DIR}/src/std/crypto_subtle.c"
        "${CMAKE_SOURCE_DIR}/src/std/crypto_symmetric.c"
        "${CMAKE_SOURCE_DIR}/src/std/crypto_ec.c")
    list(REMOVE_ITEM JSRT_HTTP_FILES 
        "${CMAKE_SOURCE_DIR}/src/http/fetch.c")
    list(REMOVE_ITEM JSRT_UTIL_FILES 
        "${CMAKE_SOURCE_DIR}/src/util/http_client.c")
    # Add full static implementations
    list(APPEND JSRT_STD_FILES "${CMAKE_SOURCE_DIR}/src/std/crypto_subtle_static.c")
    list(APPEND JSRT_HTTP_FILES "${CMAKE_SOURCE_DIR}/src/http/fetch_static.c")
    list(APPEND JSRT_UTIL_FILES "${CMAKE_SOURCE_DIR}/src/util/http_client_static.c")
    message(STATUS "Static OpenSSL: Using full static implementations")
endif()

# Add Node.js compatibility sources
set(NODE_SOURCES "")
if(JSRT_NODE_COMPAT)
    file(GLOB_RECURSE NODE_SOURCES "src/node/*.c")
endif()

set(JSRT_LIB_FILES
    ${JSRT_STD_FILES}
    ${JSRT_UTIL_FILES}
    ${JSRT_HTTP_FILES}
    ${JSRT_WASM_FILES}
    ${NODE_SOURCES}
    src/jsrt.c
    src/repl.c
    src/runtime.c)
set(JSRT_LINK_LIBS
    jsrtcore
    quickjs
    uv_a
    llhttp
    readline
    vmlib
    ${JSRT_PLATFORM_LIBS}
    ${FFI_LIBRARIES}
    ${OPENSSL_LIBRARIES})
add_library(jsrtcore STATIC ${JSRT_LIB_FILES})
target_compile_definitions(jsrtcore PRIVATE JSRT_VERSION="${JSRT_VERSION}")
if(JSRT_NODE_COMPAT)
    target_compile_definitions(jsrtcore PRIVATE JSRT_NODE_COMPAT)
endif()
if(JSRT_STATIC_OPENSSL)
    target_compile_definitions(jsrtcore PRIVATE JSRT_STATIC_OPENSSL)
    target_include_directories(jsrtcore PRIVATE ${OPENSSL_INCLUDE_DIRS})
endif()
target_link_libraries(jsrtcore PRIVATE readline)
add_executable(jsrt src/main.c src/build.c)
target_compile_definitions(jsrt PRIVATE JSRT_VERSION="${JSRT_VERSION}")
target_link_libraries(jsrt ${JSRT_LINK_LIBS})

# build jsrt tests using individual jsrt runs
enable_testing()
# testing js files - include both .js and .mjs files
file(GLOB_RECURSE JSRT_TEST_JS_FILES "test/test_*.js" "test/test_*.mjs")

# Create individual test cases for each test file
foreach(test_file ${JSRT_TEST_JS_FILES})
    # Get relative path for test name
    file(RELATIVE_PATH test_name ${CMAKE_SOURCE_DIR} ${test_file})
    # Replace path separators with underscores to create valid test names
    string(REPLACE "/" "_" test_name ${test_name})
    string(REPLACE "\\" "_" test_name ${test_name})
    
    # Add test that runs jsrt on individual test file
    add_test(NAME ${test_name} COMMAND jsrt ${test_file})
    set_tests_properties(${test_name} PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_ON_FAILURE ON)
endforeach()
