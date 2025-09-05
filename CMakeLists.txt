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
option(ENABLE_LIBFFI "Enable libffi integration for robust FFI calling conventions" ${LIBFFI_FOUND})

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
add_subdirectory(deps/libuv)

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
add_subdirectory(deps/wamr)

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
    set(JSRT_PLATFORM_LIBS)
else()
    # Unix/Linux/macOS need these libraries
    set(JSRT_PLATFORM_LIBS dl m pthread)
endif()

# build jsrt
file(GLOB_RECURSE JSRT_STD_FILES "src/std/*.c")
file(GLOB_RECURSE JSRT_UTIL_FILES "src/util/*.c")
file(GLOB_RECURSE JSRT_HTTP_FILES "src/http/*.c")
file(GLOB_RECURSE JSRT_WASM_FILES "src/wasm/*.c")
set(JSRT_LIB_FILES
    ${JSRT_STD_FILES}
    ${JSRT_UTIL_FILES}
    ${JSRT_HTTP_FILES}
    ${JSRT_WASM_FILES}
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
    ${FFI_LIBRARIES})
add_library(jsrtcore STATIC ${JSRT_LIB_FILES})
target_compile_definitions(jsrtcore PRIVATE JSRT_VERSION="${JSRT_VERSION}")
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
