#include "fs_common.h"

// Forward declarations for async functions (defined in fs_async.c)
extern JSValue js_fs_append_file(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
extern JSValue js_fs_copy_file(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
extern JSValue js_fs_rename(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
extern JSValue js_fs_rmdir(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
extern JSValue js_fs_access(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
extern JSValue js_fs_read_file(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
extern JSValue js_fs_write_file(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Module initialization
JSValue JSRT_InitNodeFs(JSContext* ctx) {
  JSValue fs_module = JS_NewObject(ctx);

  // Synchronous file operations
  JS_SetPropertyStr(ctx, fs_module, "readFileSync", JS_NewCFunction(ctx, js_fs_read_file_sync, "readFileSync", 2));
  JS_SetPropertyStr(ctx, fs_module, "writeFileSync", JS_NewCFunction(ctx, js_fs_write_file_sync, "writeFileSync", 3));
  JS_SetPropertyStr(ctx, fs_module, "appendFileSync",
                    JS_NewCFunction(ctx, js_fs_append_file_sync, "appendFileSync", 3));
  JS_SetPropertyStr(ctx, fs_module, "copyFileSync", JS_NewCFunction(ctx, js_fs_copy_file_sync, "copyFileSync", 3));
  JS_SetPropertyStr(ctx, fs_module, "renameSync", JS_NewCFunction(ctx, js_fs_rename_sync, "renameSync", 2));
  JS_SetPropertyStr(ctx, fs_module, "rmdirSync", JS_NewCFunction(ctx, js_fs_rmdir_sync, "rmdirSync", 2));
  JS_SetPropertyStr(ctx, fs_module, "accessSync", JS_NewCFunction(ctx, js_fs_access_sync, "accessSync", 2));
  JS_SetPropertyStr(ctx, fs_module, "existsSync", JS_NewCFunction(ctx, js_fs_exists_sync, "existsSync", 1));
  JS_SetPropertyStr(ctx, fs_module, "statSync", JS_NewCFunction(ctx, js_fs_stat_sync, "statSync", 1));
  JS_SetPropertyStr(ctx, fs_module, "readdirSync", JS_NewCFunction(ctx, js_fs_readdir_sync, "readdirSync", 1));
  JS_SetPropertyStr(ctx, fs_module, "mkdirSync", JS_NewCFunction(ctx, js_fs_mkdir_sync, "mkdirSync", 2));
  JS_SetPropertyStr(ctx, fs_module, "unlinkSync", JS_NewCFunction(ctx, js_fs_unlink_sync, "unlinkSync", 1));

  // File descriptor operations
  JS_SetPropertyStr(ctx, fs_module, "openSync", JS_NewCFunction(ctx, js_fs_open_sync, "openSync", 3));
  JS_SetPropertyStr(ctx, fs_module, "closeSync", JS_NewCFunction(ctx, js_fs_close_sync, "closeSync", 1));
  JS_SetPropertyStr(ctx, fs_module, "readSync", JS_NewCFunction(ctx, js_fs_read_sync, "readSync", 5));
  JS_SetPropertyStr(ctx, fs_module, "writeSync", JS_NewCFunction(ctx, js_fs_write_sync, "writeSync", 5));

  // File permissions and attributes
  JS_SetPropertyStr(ctx, fs_module, "chmodSync", JS_NewCFunction(ctx, js_fs_chmod_sync, "chmodSync", 2));
  JS_SetPropertyStr(ctx, fs_module, "chownSync", JS_NewCFunction(ctx, js_fs_chown_sync, "chownSync", 3));
  JS_SetPropertyStr(ctx, fs_module, "utimesSync", JS_NewCFunction(ctx, js_fs_utimes_sync, "utimesSync", 3));

  // Link operations
  JS_SetPropertyStr(ctx, fs_module, "linkSync", JS_NewCFunction(ctx, js_fs_link_sync, "linkSync", 2));
  JS_SetPropertyStr(ctx, fs_module, "symlinkSync", JS_NewCFunction(ctx, js_fs_symlink_sync, "symlinkSync", 3));
  JS_SetPropertyStr(ctx, fs_module, "readlinkSync", JS_NewCFunction(ctx, js_fs_readlink_sync, "readlinkSync", 2));
  JS_SetPropertyStr(ctx, fs_module, "realpathSync", JS_NewCFunction(ctx, js_fs_realpath_sync, "realpathSync", 2));

  // Advanced file operations
  JS_SetPropertyStr(ctx, fs_module, "truncateSync", JS_NewCFunction(ctx, js_fs_truncate_sync, "truncateSync", 2));
  JS_SetPropertyStr(ctx, fs_module, "ftruncateSync", JS_NewCFunction(ctx, js_fs_ftruncate_sync, "ftruncateSync", 2));
  JS_SetPropertyStr(ctx, fs_module, "mkdtempSync", JS_NewCFunction(ctx, js_fs_mkdtemp_sync, "mkdtempSync", 2));
  JS_SetPropertyStr(ctx, fs_module, "fsyncSync", JS_NewCFunction(ctx, js_fs_fsync_sync, "fsyncSync", 1));
  JS_SetPropertyStr(ctx, fs_module, "fdatasyncSync", JS_NewCFunction(ctx, js_fs_fdatasync_sync, "fdatasyncSync", 1));

  // Asynchronous file operations
  JS_SetPropertyStr(ctx, fs_module, "readFile", JS_NewCFunction(ctx, js_fs_read_file, "readFile", 2));
  JS_SetPropertyStr(ctx, fs_module, "writeFile", JS_NewCFunction(ctx, js_fs_write_file, "writeFile", 3));
  JS_SetPropertyStr(ctx, fs_module, "appendFile", JS_NewCFunction(ctx, js_fs_append_file, "appendFile", 3));
  JS_SetPropertyStr(ctx, fs_module, "copyFile", JS_NewCFunction(ctx, js_fs_copy_file, "copyFile", 3));
  JS_SetPropertyStr(ctx, fs_module, "rename", JS_NewCFunction(ctx, js_fs_rename, "rename", 3));
  JS_SetPropertyStr(ctx, fs_module, "rmdir", JS_NewCFunction(ctx, js_fs_rmdir, "rmdir", 2));
  JS_SetPropertyStr(ctx, fs_module, "access", JS_NewCFunction(ctx, js_fs_access, "access", 3));

  // Constants
  JSValue constants = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, constants, "F_OK", JS_NewInt32(ctx, F_OK));
  JS_SetPropertyStr(ctx, constants, "R_OK", JS_NewInt32(ctx, R_OK));
  JS_SetPropertyStr(ctx, constants, "W_OK", JS_NewInt32(ctx, W_OK));
  JS_SetPropertyStr(ctx, constants, "X_OK", JS_NewInt32(ctx, X_OK));
  JS_SetPropertyStr(ctx, fs_module, "constants", constants);

  return fs_module;
}

// ES Module support
int js_node_fs_init(JSContext* ctx, JSModuleDef* m) {
  JSValue fs_module = JSRT_InitNodeFs(ctx);

  // Export individual functions - synchronous
  JS_SetModuleExport(ctx, m, "readFileSync", JS_GetPropertyStr(ctx, fs_module, "readFileSync"));
  JS_SetModuleExport(ctx, m, "writeFileSync", JS_GetPropertyStr(ctx, fs_module, "writeFileSync"));
  JS_SetModuleExport(ctx, m, "appendFileSync", JS_GetPropertyStr(ctx, fs_module, "appendFileSync"));
  JS_SetModuleExport(ctx, m, "copyFileSync", JS_GetPropertyStr(ctx, fs_module, "copyFileSync"));
  JS_SetModuleExport(ctx, m, "renameSync", JS_GetPropertyStr(ctx, fs_module, "renameSync"));
  JS_SetModuleExport(ctx, m, "rmdirSync", JS_GetPropertyStr(ctx, fs_module, "rmdirSync"));
  JS_SetModuleExport(ctx, m, "accessSync", JS_GetPropertyStr(ctx, fs_module, "accessSync"));
  JS_SetModuleExport(ctx, m, "existsSync", JS_GetPropertyStr(ctx, fs_module, "existsSync"));
  JS_SetModuleExport(ctx, m, "statSync", JS_GetPropertyStr(ctx, fs_module, "statSync"));
  JS_SetModuleExport(ctx, m, "readdirSync", JS_GetPropertyStr(ctx, fs_module, "readdirSync"));
  JS_SetModuleExport(ctx, m, "mkdirSync", JS_GetPropertyStr(ctx, fs_module, "mkdirSync"));
  JS_SetModuleExport(ctx, m, "unlinkSync", JS_GetPropertyStr(ctx, fs_module, "unlinkSync"));

  // Export individual functions - asynchronous
  JS_SetModuleExport(ctx, m, "readFile", JS_GetPropertyStr(ctx, fs_module, "readFile"));
  JS_SetModuleExport(ctx, m, "writeFile", JS_GetPropertyStr(ctx, fs_module, "writeFile"));
  JS_SetModuleExport(ctx, m, "appendFile", JS_GetPropertyStr(ctx, fs_module, "appendFile"));
  JS_SetModuleExport(ctx, m, "copyFile", JS_GetPropertyStr(ctx, fs_module, "copyFile"));
  JS_SetModuleExport(ctx, m, "rename", JS_GetPropertyStr(ctx, fs_module, "rename"));
  JS_SetModuleExport(ctx, m, "rmdir", JS_GetPropertyStr(ctx, fs_module, "rmdir"));
  JS_SetModuleExport(ctx, m, "access", JS_GetPropertyStr(ctx, fs_module, "access"));

  // Export file descriptor operations
  JS_SetModuleExport(ctx, m, "openSync", JS_GetPropertyStr(ctx, fs_module, "openSync"));
  JS_SetModuleExport(ctx, m, "closeSync", JS_GetPropertyStr(ctx, fs_module, "closeSync"));
  JS_SetModuleExport(ctx, m, "readSync", JS_GetPropertyStr(ctx, fs_module, "readSync"));
  JS_SetModuleExport(ctx, m, "writeSync", JS_GetPropertyStr(ctx, fs_module, "writeSync"));

  // Export file permissions and attributes
  JS_SetModuleExport(ctx, m, "chmodSync", JS_GetPropertyStr(ctx, fs_module, "chmodSync"));
  JS_SetModuleExport(ctx, m, "chownSync", JS_GetPropertyStr(ctx, fs_module, "chownSync"));
  JS_SetModuleExport(ctx, m, "utimesSync", JS_GetPropertyStr(ctx, fs_module, "utimesSync"));

  // Export link operations
  JS_SetModuleExport(ctx, m, "linkSync", JS_GetPropertyStr(ctx, fs_module, "linkSync"));
  JS_SetModuleExport(ctx, m, "symlinkSync", JS_GetPropertyStr(ctx, fs_module, "symlinkSync"));
  JS_SetModuleExport(ctx, m, "readlinkSync", JS_GetPropertyStr(ctx, fs_module, "readlinkSync"));
  JS_SetModuleExport(ctx, m, "realpathSync", JS_GetPropertyStr(ctx, fs_module, "realpathSync"));

  // Export advanced operations
  JS_SetModuleExport(ctx, m, "truncateSync", JS_GetPropertyStr(ctx, fs_module, "truncateSync"));
  JS_SetModuleExport(ctx, m, "ftruncateSync", JS_GetPropertyStr(ctx, fs_module, "ftruncateSync"));
  JS_SetModuleExport(ctx, m, "mkdtempSync", JS_GetPropertyStr(ctx, fs_module, "mkdtempSync"));
  JS_SetModuleExport(ctx, m, "fsyncSync", JS_GetPropertyStr(ctx, fs_module, "fsyncSync"));
  JS_SetModuleExport(ctx, m, "fdatasyncSync", JS_GetPropertyStr(ctx, fs_module, "fdatasyncSync"));

  // Export constants
  JS_SetModuleExport(ctx, m, "constants", JS_GetPropertyStr(ctx, fs_module, "constants"));

  // Also export the whole module as default
  JS_SetModuleExport(ctx, m, "default", fs_module);

  return 0;
}