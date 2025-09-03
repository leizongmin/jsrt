cmake_minimum_required(VERSION 3.10)
project(jsrt C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CFLAGS "-Wall -Wextra")

# optimization flags
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
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

include_directories(src deps/quickjs deps/libuv/include deps/libuv/src deps/readline)

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
    target_link_libraries(qjsc quickjs ${JSRT_PLATFORM_LIBS})
    # build qjsrepl.c
    add_custom_command(OUTPUT qjsrepl.c
    COMMAND qjsc -c -o qjsrepl.c -m ${CMAKE_CURRENT_SOURCE_DIR}/deps/quickjs/repl.js
    DEPENDS qjsc)
    set_source_files_properties(qjsrepl.c PROPERTIES GENERATED TRUE)
    # build qjs
    add_executable(qjs deps/quickjs/qjs.c qjsrepl.c deps/quickjs/quickjs-libc.c)
    target_compile_definitions(qjs PRIVATE CONFIG_VERSION="${QUICKJS_VERSION}" _GNU_SOURCE)
    target_link_libraries(qjs quickjs ${JSRT_PLATFORM_LIBS})
endif()

# build libuv
add_subdirectory(deps/libuv)

# Try to find system readline first
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(READLINE QUIET readline)
endif()

# Check if we can use system readline
if(NOT READLINE_FOUND)
    # Try to find readline manually
    find_library(READLINE_LIBRARY NAMES readline)
    find_path(READLINE_INCLUDE_DIR readline/readline.h)
    if(READLINE_LIBRARY AND READLINE_INCLUDE_DIR)
        set(READLINE_FOUND TRUE)
        set(READLINE_LIBRARIES ${READLINE_LIBRARY})
        set(READLINE_INCLUDE_DIRS ${READLINE_INCLUDE_DIR})
        # Also try to find termcap/ncurses which readline usually depends on
        find_library(TERMCAP_LIBRARY NAMES termcap ncurses)
        if(TERMCAP_LIBRARY)
            list(APPEND READLINE_LIBRARIES ${TERMCAP_LIBRARY})
        endif()
    endif()
endif()

if(READLINE_FOUND)
    # Use system readline
    message(STATUS "Using system readline")
    add_library(readline INTERFACE)
    target_link_libraries(readline INTERFACE ${READLINE_LIBRARIES})
    target_include_directories(readline INTERFACE ${READLINE_INCLUDE_DIRS})
    target_compile_definitions(readline INTERFACE HAVE_READLINE=1)
else()
    # Provide a minimal readline replacement
    message(STATUS "System readline not found, using minimal fallback")
    add_library(readline STATIC src/minimal_readline.c)
    target_include_directories(readline PUBLIC src)
    target_compile_definitions(readline PUBLIC MINIMAL_READLINE=1)
    # Also need to define this for dependent targets
    add_compile_definitions(MINIMAL_READLINE=1)
endif()

# Platform-specific libraries
if(WIN32)
    # Windows doesn't need dl, m is built into MSVCRT, pthread is handled by libuv
    set(JSRT_PLATFORM_LIBS)
else()
    # Unix/Linux systems need these libraries (readline now built statically)
    set(JSRT_PLATFORM_LIBS dl m pthread termcap)
endif()

# build jsrt
file(GLOB_RECURSE JSRT_STD_FILES "src/std/*.c")
file(GLOB_RECURSE JSRT_UTIL_FILES "src/util/*.c")
set(JSRT_LIB_FILES
    ${JSRT_STD_FILES}
    ${JSRT_UTIL_FILES}
    src/jsrt.c
    src/repl.c
    src/runtime.c)
set(JSRT_LINK_LIBS
    jsrtcore
    quickjs
    uv_a
    readline
    ${JSRT_PLATFORM_LIBS})
add_library(jsrtcore STATIC ${JSRT_LIB_FILES})
target_compile_definitions(jsrtcore PRIVATE READLINE_LIBRARY)
target_include_directories(jsrtcore PRIVATE deps/readline)
target_compile_definitions(jsrtcore PRIVATE JSRT_VERSION="${JSRT_VERSION}")
add_executable(jsrt src/main.c src/build.c)
target_compile_definitions(jsrt PRIVATE JSRT_VERSION="${JSRT_VERSION}")
target_link_libraries(jsrt ${JSRT_LINK_LIBS})

# build jsrt test
enable_testing()
file(GLOB_RECURSE JSRT_TEST_LIB_FILES "test/*.c")
add_executable(jsrt_test_js ${JSRT_TEST_LIB_FILES})
# testing js files - include both .js and .mjs files
file(GLOB_RECURSE JSRT_TEST_JS_FILES "test/test_*.js" "test/test_*.mjs")
target_compile_definitions(jsrt_test_js PRIVATE JSRT_VERSION="${JSRT_VERSION}")
target_link_libraries(jsrt_test_js PRIVATE ${JSRT_LINK_LIBS})
add_test(NAME jsrt_test_js COMMAND jsrt_test_js ${JSRT_TEST_JS_FILES})
set_tests_properties(jsrt_test_js PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_ON_FAILURE ON)
