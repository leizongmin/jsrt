#include <errno.h>
#include <quickjs.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include "../../util/debug.h"
#include "process.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

// Forward declaration from quickjs-libc
void js_std_dump_error(JSContext* ctx);

// Signal name to number mapping
typedef struct {
  const char* name;
  int signum;
} SignalMapping;

// Standard Unix signals (cross-platform where possible)
static const SignalMapping signal_map[] = {
    {"SIGHUP", SIGHUP},   {"SIGINT", SIGINT},     {"SIGQUIT", SIGQUIT}, {"SIGILL", SIGILL},   {"SIGTRAP", SIGTRAP},
    {"SIGABRT", SIGABRT}, {"SIGBUS", SIGBUS},     {"SIGFPE", SIGFPE},   {"SIGKILL", SIGKILL}, {"SIGUSR1", SIGUSR1},
    {"SIGSEGV", SIGSEGV}, {"SIGUSR2", SIGUSR2},   {"SIGPIPE", SIGPIPE}, {"SIGALRM", SIGALRM}, {"SIGTERM", SIGTERM},
    {"SIGCHLD", SIGCHLD}, {"SIGCONT", SIGCONT},   {"SIGSTOP", SIGSTOP}, {"SIGTSTP", SIGTSTP}, {"SIGTTIN", SIGTTIN},
    {"SIGTTOU", SIGTTOU}, {"SIGURG", SIGURG},     {"SIGXCPU", SIGXCPU}, {"SIGXFSZ", SIGXFSZ}, {"SIGVTALRM", SIGVTALRM},
    {"SIGPROF", SIGPROF}, {"SIGWINCH", SIGWINCH}, {"SIGIO", SIGIO},     {"SIGSYS", SIGSYS},   {NULL, 0}};

// Global signal handler state
typedef struct SignalHandler {
  int signum;
  uv_signal_t* uv_signal;
  JSValue callback;  // JS callback function
  JSContext* ctx;
  struct SignalHandler* next;
} SignalHandler;

static SignalHandler* g_signal_handlers = NULL;
static JSValue g_process_obj = {0};  // Reference to process object for emitting events

// Convert signal name to number
static int signal_name_to_num(const char* name) {
  // Handle numeric strings
  char* endptr;
  long num = strtol(name, &endptr, 10);
  if (*endptr == '\0' && num >= 0) {
    return (int)num;
  }

  // Handle "SIG" prefix and without prefix
  const char* search_name = name;
  if (strncmp(name, "SIG", 3) == 0) {
    search_name = name;
  }

  for (int i = 0; signal_map[i].name != NULL; i++) {
    if (strcmp(signal_map[i].name, search_name) == 0) {
      return signal_map[i].signum;
    }
    // Try without SIG prefix
    if (strncmp(signal_map[i].name, "SIG", 3) == 0 && strcmp(signal_map[i].name + 3, search_name) == 0) {
      return signal_map[i].signum;
    }
  }

  return -1;  // Unknown signal
}

// Convert signal number to name
static const char* signal_num_to_name(int signum) {
  for (int i = 0; signal_map[i].name != NULL; i++) {
    if (signal_map[i].signum == signum) {
      return signal_map[i].name;
    }
  }
  return "UNKNOWN";
}

// libuv signal callback
static void on_signal_received(uv_signal_t* handle, int signum) {
  SignalHandler* handler = (SignalHandler*)handle->data;
  if (!handler || !handler->ctx) {
    return;
  }

  JSRT_Debug("Received signal %d (%s)", signum, signal_num_to_name(signum));

  // Call all registered callbacks for this signal
  SignalHandler* h = g_signal_handlers;
  while (h) {
    if (h->signum == signum && JS_IsFunction(handler->ctx, h->callback)) {
      JSValue result = JS_Call(handler->ctx, h->callback, g_process_obj, 0, NULL);
      if (JS_IsException(result)) {
        js_std_dump_error(handler->ctx);
      }
      JS_FreeValue(handler->ctx, result);
    }
    h = h->next;
  }
}

// process.kill(pid, signal)
JSValue js_process_kill(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "pid argument is required");
  }

  // Parse pid
  int32_t pid;
  if (JS_ToInt32(ctx, &pid, argv[0]) < 0) {
    return JS_EXCEPTION;
  }

  // Parse signal (default: SIGTERM)
  int signum = SIGTERM;
  if (argc >= 2) {
    if (JS_IsString(argv[1])) {
      const char* sig_name = JS_ToCString(ctx, argv[1]);
      if (!sig_name) {
        return JS_EXCEPTION;
      }
      signum = signal_name_to_num(sig_name);
      JS_FreeCString(ctx, sig_name);
      if (signum < 0) {
        return JS_ThrowTypeError(ctx, "Unknown signal");
      }
    } else {
      int32_t sig_int;
      if (JS_ToInt32(ctx, &sig_int, argv[1]) < 0) {
        return JS_EXCEPTION;
      }
      signum = sig_int;
    }
  }

  JSRT_Debug("Sending signal %d (%s) to pid %d", signum, signal_num_to_name(signum), pid);

  // Send signal
#ifdef _WIN32
  // Windows: Limited signal support
  if (signum == SIGTERM || signum == SIGKILL) {
    HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!process) {
      return JS_ThrowInternalError(ctx, "Failed to open process");
    }
    BOOL result = TerminateProcess(process, 1);
    CloseHandle(process);
    if (!result) {
      return JS_ThrowInternalError(ctx, "Failed to terminate process");
    }
  } else if (signum == SIGINT) {
    // Windows: Generate Ctrl+C event
    if (!GenerateConsoleCtrlEvent(CTRL_C_EVENT, pid)) {
      return JS_ThrowInternalError(ctx, "Failed to send SIGINT");
    }
  } else {
    return JS_ThrowTypeError(ctx, "Signal not supported on Windows");
  }
#else
  // Unix: Use kill() syscall
  if (kill(pid, signum) < 0) {
    return JS_ThrowInternalError(ctx, "Failed to send signal: %s", strerror(errno));
  }
#endif

  return JS_TRUE;
}

// Register a signal handler
static SignalHandler* register_signal_handler(JSContext* ctx, int signum, JSValue callback, uv_loop_t* loop) {
  // Check if handler already exists for this signal
  SignalHandler* existing = g_signal_handlers;
  while (existing) {
    if (existing->signum == signum && existing->ctx == ctx) {
      // Update callback
      JS_FreeValue(ctx, existing->callback);
      existing->callback = JS_DupValue(ctx, callback);
      return existing;
    }
    existing = existing->next;
  }

  // Create new handler
  SignalHandler* handler = malloc(sizeof(SignalHandler));
  if (!handler) {
    return NULL;
  }

  handler->signum = signum;
  handler->ctx = ctx;
  handler->callback = JS_DupValue(ctx, callback);
  handler->uv_signal = malloc(sizeof(uv_signal_t));

  // Initialize libuv signal handler
  uv_signal_init(loop, handler->uv_signal);
  handler->uv_signal->data = handler;
  uv_signal_start(handler->uv_signal, on_signal_received, signum);

  // Add to list
  handler->next = g_signal_handlers;
  g_signal_handlers = handler;

  JSRT_Debug("Registered handler for signal %d (%s)", signum, signal_num_to_name(signum));

  return handler;
}

// Setup signal handling for process object
void jsrt_process_setup_signals(JSContext* ctx, JSValue process_obj, uv_loop_t* loop) {
  g_process_obj = JS_DupValue(ctx, process_obj);

  // Common signals are registered on-demand via process.on()
  JSRT_Debug("Signal handling system initialized");
}

// Enhanced process.on() with signal support
JSValue js_process_on_with_signals(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv,
                                   uv_loop_t* loop) {
  if (argc < 2) {
    return JS_UNDEFINED;
  }

  const char* event_name = JS_ToCString(ctx, argv[0]);
  if (!event_name) {
    return JS_EXCEPTION;
  }

  JSValue callback = argv[1];
  if (!JS_IsFunction(ctx, callback)) {
    JS_FreeCString(ctx, event_name);
    return JS_ThrowTypeError(ctx, "Callback must be a function");
  }

  // Check if this is a signal event
  int signum = signal_name_to_num(event_name);
  if (signum > 0) {
    // Register signal handler
    SignalHandler* handler = register_signal_handler(ctx, signum, callback, loop);
    JS_FreeCString(ctx, event_name);
    if (!handler) {
      return JS_ThrowOutOfMemory(ctx);
    }
    return this_val;
  }

  JS_FreeCString(ctx, event_name);

  // Fall back to regular event handling (will be extended in later tasks)
  return JS_UNDEFINED;
}

// Cleanup function
void jsrt_process_cleanup_signals(JSContext* ctx) {
  SignalHandler* handler = g_signal_handlers;
  while (handler) {
    SignalHandler* next = handler->next;

    // Stop and close libuv signal
    if (handler->uv_signal) {
      uv_signal_stop(handler->uv_signal);
      uv_close((uv_handle_t*)handler->uv_signal, NULL);
      free(handler->uv_signal);
    }

    // Free callback
    if (handler->ctx) {
      JS_FreeValue(handler->ctx, handler->callback);
    }

    free(handler);
    handler = next;
  }
  g_signal_handlers = NULL;

  if (!JS_IsUndefined(g_process_obj)) {
    JS_FreeValue(ctx, g_process_obj);
    g_process_obj = (JSValue){0};
  }

  JSRT_Debug("Signal handling system cleaned up");
}

// Initialize signal handling module
void jsrt_process_init_signals(void) {
  // Nothing to initialize at startup
  JSRT_Debug("Signal handling module initialized");
}
