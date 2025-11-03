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
JSValue js_process_hrtime_bigint(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

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
const char* jsrt_process_get_exec_path_internal(void);

// Signals (signals.c)
JSValue js_process_kill(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_on_with_signals(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv,
                                   uv_loop_t* loop);
void jsrt_process_setup_signals(JSContext* ctx, JSValue process_obj, uv_loop_t* loop);
void jsrt_process_cleanup_signals(JSContext* ctx);

// Events (events.c)
JSValue js_process_on_events(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_emit_events(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_emit_warning(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_set_uncaught_exception_capture(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_has_uncaught_exception_capture(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
void jsrt_process_emit_exit(JSContext* ctx, int exit_code);
void jsrt_process_emit_before_exit(JSContext* ctx, int exit_code);
bool jsrt_process_emit_uncaught_exception(JSContext* ctx, JSValue error);
bool jsrt_process_emit_unhandled_rejection(JSContext* ctx, JSValue reason, JSValue promise);
void jsrt_process_emit_rejection_handled(JSContext* ctx, JSValue promise);
void jsrt_process_setup_events(JSContext* ctx, JSValue process_obj);
void jsrt_process_cleanup_events(JSContext* ctx);

// Resources (resources.c)
JSValue js_process_cpu_usage(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_resource_usage(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_memory_usage_rss(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_available_memory(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_constrained_memory(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

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

// stdio (stdout, stderr, stdin) - stdio.c (legacy)
JSValue jsrt_create_stdout(JSContext* ctx);
JSValue jsrt_create_stderr(JSContext* ctx);
JSValue jsrt_create_stdin(JSContext* ctx);

// stdio streams - process_streams.c (new: real stream objects)
JSValue jsrt_create_stdin_stream(JSContext* ctx);
JSValue jsrt_create_stdout_stream(JSContext* ctx);
JSValue jsrt_create_stderr_stream(JSContext* ctx);

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
void jsrt_process_init_signals(void);
void jsrt_process_init_events(void);
void jsrt_process_init_resources(void);

// Unix permissions (unix_permissions.c - Unix only)
#ifndef _WIN32
JSValue js_process_getuid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_geteuid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_getgid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_getegid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_setuid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_seteuid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_setgid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_setegid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_getgroups(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_setgroups(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_initgroups(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_umask(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
#endif

// IPC functions (process_ipc.c)
void jsrt_process_setup_ipc(JSContext* ctx, JSValue process_obj, JSRT_Runtime* rt);
void jsrt_process_cleanup_ipc(JSContext* ctx);

// Advanced features (advanced.c)
JSValue js_process_load_env_file(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_get_active_resources_info(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_set_source_maps_enabled(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_get_source_maps_enabled(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_ref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_unref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
void jsrt_process_init_advanced(void);
void jsrt_process_cleanup_advanced(JSContext* ctx);

// Stub implementations for future features (stubs.c)
JSValue js_process_get_report(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_get_permission(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_get_finalization(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_dlopen(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_process_get_builtin_module(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
void jsrt_process_init_stubs(void);

#endif  // __JSRT_NODE_PROCESS_H__
