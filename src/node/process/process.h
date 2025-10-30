#ifndef __JSRT_NODE_PROCESS_H__
#define __JSRT_NODE_PROCESS_H__

#include <quickjs.h>
#include "../../runtime.h"

// Platform-specific includes and definitions
#ifdef _WIN32
#include <process.h>
#include <tlhelp32.h>
#include <windows.h>
#include <winsock2.h>
#else
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>
#endif

// Forward declarations
typedef struct JSRTProcessModule JSRTProcessModule;

// Global variables for command line arguments
extern int jsrt_argc;
extern char** jsrt_argv;
extern double g_jsrt_start_time;

// Platform abstraction layer (platform.c)
int jsrt_process_getpid(void);
int jsrt_process_getppid(void);
int jsrt_process_gettimeofday(struct timeval* tv, void* tz);
char* jsrt_process_getcwd(char* buf, size_t size);
int jsrt_process_chdir(const char* path);
const char* jsrt_process_platform(void);
const char* jsrt_process_arch(void);

// Basic process information (basic.c)
JSValue js_process_get_pid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_get_ppid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_get_argv(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_get_argv0(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_get_platform(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_get_arch(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Version information (versions.c)
JSValue js_process_get_version(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_get_versions(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Environment variables (env.c)
JSValue js_process_get_env(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Timing functions (timing.c)
JSValue js_process_uptime(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_hrtime(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Process control (control.c)
JSValue js_process_exit(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_cwd(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_chdir(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Node.js specific features (nodejs.c)
JSValue js_process_nextTick(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_memoryUsage(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Properties (properties.c)
JSValue js_process_get_execPath(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_get_execArgv(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_get_exitCode(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_set_exitCode(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_get_title(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_set_title(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_get_config(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_get_release(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_get_features(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
int jsrt_process_get_exit_code_internal(void);

// Module initialization and export (module.c)
JSValue jsrt_init_unified_process_module(JSContext* ctx);
JSValue jsrt_get_process_module(JSContext* ctx);
JSValue node_get_process_module(JSContext* ctx);
JSValue jsrt_init_module_process(JSContext* ctx);
JSValue node_init_module_process(JSContext* ctx);
void jsrt_process_cleanup(JSContext* ctx);
JSValue JSRT_CreateProcessModule(JSContext* ctx);
JSValue JSRT_InitNodeProcess(JSContext* ctx);
int js_unified_process_init(JSContext* ctx, JSModuleDef* m);

// stdio (stdout, stderr, stdin) - stdio.c
JSValue jsrt_create_stdout(JSContext* ctx);
JSValue jsrt_create_stderr(JSContext* ctx);
JSValue jsrt_create_stdin(JSContext* ctx);

// Runtime setup function (replaces JSRT_RuntimeSetupStdProcess)
void JSRT_RuntimeSetupStdProcess(JSRT_Runtime* rt);

// Initialization functions for each component
void jsrt_process_init_platform(void);
void jsrt_process_init_basic(void);
void jsrt_process_init_versions(void);
void jsrt_process_init_env(void);
void jsrt_process_init_timing(void);
void jsrt_process_init_control(void);
void jsrt_process_init_nodejs(void);
void jsrt_process_init_properties(void);
void jsrt_process_cleanup_properties(void);

// IPC functions (process_ipc.c)
void jsrt_process_setup_ipc(JSContext* ctx, JSValue process_obj, JSRT_Runtime* rt);
void jsrt_process_cleanup_ipc(JSContext* ctx);

#endif  // __JSRT_NODE_PROCESS_H__