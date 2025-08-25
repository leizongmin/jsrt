cmake_minimum_required(VERSION 3.16)
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

include_directories(src deps/quickjs deps/libuv/include deps/libuv/src)

# build libquickjs
set(QUICKJS_LIB_FILES
    deps/quickjs/cutils.c
    deps/quickjs/libbf.c
    deps/quickjs/libregexp.c
    deps/quickjs/libunicode.c
    deps/quickjs/quickjs-libc.c
    deps/quickjs/quickjs.c
    )
add_library(quickjs STATIC ${QUICKJS_LIB_FILES})
file(STRINGS deps/quickjs/VERSION QUICKJS_VERSION)
target_compile_definitions(quickjs PRIVATE CONFIG_VERSION="${QUICKJS_VERSION}" CONFIG_BIGNUM _GNU_SOURCE)

# build qjs and qjsc, use flag -DBUILD_QJS=ON to enable
option(BUILD_QJS "Build qjs and qjsc executable" OFF)
if(BUILD_QJS)
    add_executable(qjsc deps/quickjs/qjsc.c deps/quickjs/quickjs-libc.c)
    target_compile_definitions(qjsc PRIVATE CONFIG_VERSION="${QUICKJS_VERSION}" CONFIG_BIGNUM _GNU_SOURCE)
    target_link_libraries(qjsc quickjs ${JSRT_PLATFORM_LIBS})
    # build qjscalc.c
    add_custom_command(OUTPUT qjscalc.c
    COMMAND qjsc -fbignum -c -o qjscalc.c ${CMAKE_CURRENT_SOURCE_DIR}/deps/quickjs/qjscalc.js
    DEPENDS qjsc)
    set_source_files_properties(qjscalc.c PROPERTIES GENERATED TRUE)
    # build qjsrepl.c
    add_custom_command(OUTPUT qjsrepl.c
    COMMAND qjsc -c -o qjsrepl.c -m ${CMAKE_CURRENT_SOURCE_DIR}/deps/quickjs/repl.js
    DEPENDS qjsc)
    set_source_files_properties(qjsrepl.c PROPERTIES GENERATED TRUE)
    # build qjs
    add_executable(qjs deps/quickjs/qjs.c qjscalc.c qjsrepl.c deps/quickjs/quickjs-libc.c)
    target_compile_definitions(qjs PRIVATE CONFIG_VERSION="${QUICKJS_VERSION}" CONFIG_BIGNUM _GNU_SOURCE)
    target_link_libraries(qjs quickjs ${JSRT_PLATFORM_LIBS})
endif()

# build libuv
add_subdirectory(deps/libuv)

# Platform-specific libraries
if(WIN32)
    # Windows doesn't need dl, m is built into MSVCRT, pthread is handled by libuv
    set(JSRT_PLATFORM_LIBS)
else()
    # Unix/Linux systems need these libraries
    set(JSRT_PLATFORM_LIBS dl m pthread)
endif()

# build jsrt
file(GLOB_RECURSE JSRT_STD_FILES "src/std/*.c")
file(GLOB_RECURSE JSRT_UTIL_FILES "src/util/*.c")
set(JSRT_LIB_FILES
    ${JSRT_STD_FILES}
    ${JSRT_UTIL_FILES}
    src/jsrt.c
    src/runtime.c)
set(JSRT_LINK_LIBS
    jsrtcore
    quickjs
    uv_a
    ${JSRT_PLATFORM_LIBS})
add_library(jsrtcore STATIC ${JSRT_LIB_FILES})
add_executable(jsrt src/main.c)
target_link_libraries(jsrt ${JSRT_LINK_LIBS})

# build jsrt test
enable_testing()
file(GLOB_RECURSE JSRT_TEST_LIB_FILES "test/*.c")
add_executable(jsrt_test_js ${JSRT_TEST_LIB_FILES})
# testing js files - include both .js and .mjs files
file(GLOB_RECURSE JSRT_TEST_JS_FILES "test/test_*.js" "test/test_*.mjs")
target_link_libraries(jsrt_test_js PRIVATE ${JSRT_LINK_LIBS})
add_test(NAME jsrt_test_js COMMAND jsrt_test_js ${JSRT_TEST_JS_FILES})
set_tests_properties(jsrt_test_js PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_ON_FAILURE ON)
