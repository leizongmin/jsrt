/**
 * Node.js TTY module implementation (Enhanced with libuv integration)
 *
 * Provides TTY (terminal) functionality including:
 * - ReadStream with raw mode support via libuv
 * - WriteStream with cursor control and window size via libuv
 * - tty.isatty() utility function with proper TTY detection
 * - Terminal capability detection and resize events
 * - Cross-platform compatibility (Windows, Unix, macOS)
 */

#ifndef JSRT_NODE_TTY_H
#define JSRT_NODE_TTY_H

#include <quickjs.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <uv.h>
#include "../stream/stream_internal.h"

// TTY mode constants (matching libuv and Node.js)
#define UV_TTY_MODE_NORMAL 0
#define UV_TTY_MODE_RAW 1
#define UV_TTY_MODE_IO 2

// Constants for file descriptor validation
#define MIN_FD 0
#define MAX_FD 1024

// Constants for clearLine direction parameter
#define CLEAR_LINE_DIRECTION_TO_END -1
#define CLEAR_LINE_DIRECTION_TO_START 0
#define CLEAR_LINE_DIRECTION_ENTIRE 1

// Enhanced TTY Stream data structures with libuv integration
typedef struct {
  JSStreamData base;        // Base stream data (inherits from stream)
  uv_tty_t handle;          // libuv TTY handle for real terminal operations
  int fd;                   // File descriptor (0=stdin, 1=stdout, 2=stderr)
  bool is_raw;              // Raw mode state for ReadStream
  int columns;              // Terminal width in characters
  int rows;                 // Terminal height in characters
  bool is_tty;              // TTY detection result
  bool handle_initialized;  // Whether libuv handle is initialized
  uv_timer_t resize_timer;  // Timer for detecting resize events
  JSContext* ctx;           // JavaScript context for event emission
} JSTTYStreamData;

// Global TTY resize handler management
typedef struct tty_resize_handler {
  struct tty_resize_handler* next;
  JSContext* ctx;
  JSValue stream_obj;
  int fd;
  JSTTYStreamData* tty_data;
} tty_resize_handler_t;

// Function prototypes
JSValue JSRT_InitNodeTTY(JSContext* ctx);
int js_node_tty_init(JSContext* ctx, JSModuleDef* m);

// TTY utility functions with enhanced libuv integration
JSValue js_tty_isatty(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_tty_guess_handle(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// ReadStream methods with libuv integration
JSValue js_readstream_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);
JSValue js_readstream_set_raw_mode(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_readstream_get_is_raw(JSContext* ctx, JSValueConst this_val);
JSValue js_readstream_set_is_raw(JSContext* ctx, JSValueConst this_val, JSValueConst value);
JSValue js_readstream_get_fd(JSContext* ctx, JSValueConst this_val);

// WriteStream methods with enhanced functionality
JSValue js_writestream_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);
JSValue js_writestream_get_columns(JSContext* ctx, JSValueConst this_val);
JSValue js_writestream_get_rows(JSContext* ctx, JSValueConst this_val);
JSValue js_writestream_clear_line(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic);
JSValue js_writestream_clear_screen_down(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv,
                                         int magic);
JSValue js_writestream_cursor_to(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic);
JSValue js_writestream_move_cursor(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic);
JSValue js_writestream_get_color_depth(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic);
JSValue js_writestream_has_colors(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic);
JSValue js_writestream_get_fd(JSContext* ctx, JSValueConst this_val);
JSValue js_writestream_get_window_size(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Enhanced color detection functions
int js_tty_get_color_depth_enhanced(int fd);
bool js_tty_has_colors_enhanced(int fd, int count);
bool js_tty_is_terminal_supported(const char* term);

// Cross-platform TTY utilities
bool js_tty_init_handle(JSTTYStreamData* stream, int fd, bool readable, JSContext* ctx);
void js_tty_cleanup_handle(JSTTYStreamData* stream);
int js_tty_get_window_size_from_handle(JSTTYStreamData* stream, int* width, int* height);

// Resize event handling
int js_tty_setup_resize_detection(JSTTYStreamData* stream, JSValue stream_obj);
void js_tty_cleanup_resize_detection(JSTTYStreamData* stream);
void js_tty_process_resize_events(void);
void js_tty_resize_signal_handler(int sig);

// Platform-specific implementations
#ifdef _WIN32
int js_tty_enable_virtual_terminal_processing(int fd);
void js_tty_windows_cursor_to(int x, int y);
void js_tty_windows_clear_line(void);
#endif

// Error handling utilities
int js_tty_handle_error(JSContext* ctx, int uv_result, const char* operation);
JSValue js_tty_throw_error(JSContext* ctx, int uv_result, const char* operation);

// TTY stream lifecycle management
void js_tty_stream_finalizer(JSRuntime* rt, JSValue val);
JSClassID js_tty_get_readstream_class_id(void);
JSClassID js_tty_get_writestream_class_id(void);

// Module initialization and cleanup
int js_tty_init_global_state(void);
void js_tty_cleanup_global_state(void);
void js_tty_init_classes(JSContext* ctx);

// Class IDs for TTY streams (extern declarations)
extern JSClassID js_readstream_class_id;
extern JSClassID js_writestream_class_id;

// Global state
extern tty_resize_handler_t* resize_handlers;
extern volatile sig_atomic_t resize_pending;

#endif  // JSRT_NODE_TTY_H