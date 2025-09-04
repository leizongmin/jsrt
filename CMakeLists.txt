cmake_minimum_required(VERSION 3.10)
project(jsrt C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CFLAGS "-Wall -Wextra")

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

include_directories(src deps/quickjs deps/libuv/include deps/libuv/src deps/readline deps/llhttp/build)

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

# build llhttp (minimal implementation)
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
set(JSRT_LIB_FILES
    ${JSRT_STD_FILES}
    ${JSRT_UTIL_FILES}
    ${JSRT_HTTP_FILES}
    src/jsrt.c
    src/repl.c
    src/runtime.c)
set(JSRT_LINK_LIBS
    jsrtcore
    quickjs
    uv_a
    llhttp
    readline
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
