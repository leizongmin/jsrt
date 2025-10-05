#ifndef JSRT_NODE_FS_COMMON_H
#define JSRT_NODE_FS_COMMON_H

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>  // For open flags
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <utime.h>  // For utime function
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

// Basic file I/O operations
JSValue js_fs_read_file_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_write_file_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_append_file_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_exists_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_unlink_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// File operations
JSValue js_fs_copy_file_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_rename_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_access_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Directory operations (defined in fs_sync_dir.c)
JSValue js_fs_stat_is_file(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_stat_is_directory(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_stat_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_readdir_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_mkdir_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_rmdir_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// File descriptor operations
JSValue js_fs_open_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_close_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_read_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_write_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// File permissions and attributes
JSValue js_fs_chmod_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_chown_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_utimes_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Link operations
JSValue js_fs_link_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_symlink_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_readlink_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_realpath_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Advanced file operations
JSValue js_fs_truncate_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_ftruncate_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_mkdtemp_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_fsync_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_fdatasync_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_statfs_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Phase 1: New stat variants
JSValue js_fs_fstat_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_lstat_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Phase 1: FD-based permissions and times
// Platform Support:
//   - fchmodSync: Unix only (Windows: not supported)
//   - fchownSync/lchownSync: Unix only (Windows: ownership not available)
//   - futimesSync: Cross-platform (Unix: futimes, Windows: _futime with 1s precision)
//   - lutimesSync: Unix only (requires utimensat with AT_SYMLINK_NOFOLLOW)
JSValue js_fs_fchmod_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_fchown_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_lchown_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_futimes_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_lutimes_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Phase 1: Recursive operations
JSValue js_fs_rm_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_cp_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Phase 1: Directory operations
JSValue js_fs_opendir_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Phase 1: Vectored I/O
JSValue js_fs_readv_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_writev_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Asynchronous file operations
// Phase 2: Buffer I/O async APIs
JSValue js_fs_read_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_write_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_readv_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_writev_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Phase 2: Recursive operations async APIs
JSValue js_fs_rm_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_cp_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Phase 2.1: Additional async APIs
JSValue js_fs_truncate_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_ftruncate_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_fsync_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_fdatasync_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_mkdtemp_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_statfs_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Phase 3: Promise API
JSValue JSRT_InitNodeFsPromises(JSContext* ctx);
void fs_promises_init(JSContext* ctx);

// Phase 3: Promise API function declarations
JSValue js_fs_promises_open(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_promises_stat(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_promises_lstat(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_promises_unlink(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_promises_rename(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_promises_mkdir(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_promises_rmdir(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_promises_readdir(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_promises_readlink(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_promises_readFile(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_promises_writeFile(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_promises_appendFile(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_promises_link(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_promises_symlink(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_promises_realpath(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_promises_rm(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_promises_cp(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_promises_chmod(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_promises_lchmod(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_promises_chown(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_promises_lchown(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_promises_utimes(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_promises_lutimes(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_promises_access(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_promises_truncate(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Module initialization
JSValue JSRT_InitNodeFs(JSContext* ctx);
int js_node_fs_init(JSContext* ctx, JSModuleDef* m);

#endif  // JSRT_NODE_FS_COMMON_H