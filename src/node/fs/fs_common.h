#ifndef JSRT_NODE_FS_COMMON_H
#define JSRT_NODE_FS_COMMON_H

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>  // For _mkdir on Windows
#include <io.h>      // For access function
#else
#include <unistd.h>
#endif
#include "../../util/debug.h"
#include "../node_modules.h"

// Cross-platform mkdir wrapper
#ifdef _WIN32
#define JSRT_MKDIR(path, mode) _mkdir(path)
#else
#define JSRT_MKDIR(path, mode) mkdir(path, mode)
#endif

// Common helper functions
int mkdir_recursive(const char* path, mode_t mode);
const char* errno_to_node_code(int err);
JSValue create_buffer_from_data(JSContext* ctx, const uint8_t* data, size_t size);
JSValue create_fs_error(JSContext* ctx, int err, const char* syscall, const char* path);

// Synchronous file operations
JSValue js_fs_read_file_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_write_file_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_exists_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_stat_is_file(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_stat_is_directory(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_stat_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_readdir_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_mkdir_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_unlink_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_append_file_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_copy_file_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_rename_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_rmdir_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_access_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Asynchronous file operations (declared in fs_async.c as static)
// These functions are not exposed in the header as they are only used internally

// Module initialization
JSValue JSRT_InitNodeFs(JSContext* ctx);
int js_node_fs_init(JSContext* ctx, JSModuleDef* m);

#endif  // JSRT_NODE_FS_COMMON_H