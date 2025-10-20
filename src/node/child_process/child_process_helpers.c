#include "../../util/debug.h"
#include "child_process_internal.h"
#include <signal.h>

// Add EventEmitter methods to an object
void add_event_emitter_methods(JSContext* ctx, JSValue obj) {
  // Get EventEmitter from events module
  JSValue events_module = JSRT_LoadNodeModuleCommonJS(ctx, "events");
  if (JS_IsException(events_module)) {
    JSRT_Debug("Failed to load events module");
    return;
  }

  JSValue EventEmitter = JS_GetPropertyStr(ctx, events_module, "EventEmitter");
  if (!JS_IsException(EventEmitter)) {
    JSValue proto = JS_GetPropertyStr(ctx, EventEmitter, "prototype");
    if (!JS_IsException(proto)) {
      // Copy EventEmitter methods to obj
      JSValue on_func = JS_GetPropertyStr(ctx, proto, "on");
      if (JS_IsFunction(ctx, on_func)) {
        JS_SetPropertyStr(ctx, obj, "on", JS_DupValue(ctx, on_func));
      }
      JS_FreeValue(ctx, on_func);

      JSValue emit_func = JS_GetPropertyStr(ctx, proto, "emit");
      if (JS_IsFunction(ctx, emit_func)) {
        JS_SetPropertyStr(ctx, obj, "emit", JS_DupValue(ctx, emit_func));
      }
      JS_FreeValue(ctx, emit_func);

      JSValue once_func = JS_GetPropertyStr(ctx, proto, "once");
      if (JS_IsFunction(ctx, once_func)) {
        JS_SetPropertyStr(ctx, obj, "once", JS_DupValue(ctx, once_func));
      }
      JS_FreeValue(ctx, once_func);

      JSValue removeListener = JS_GetPropertyStr(ctx, proto, "removeListener");
      if (JS_IsFunction(ctx, removeListener)) {
        JS_SetPropertyStr(ctx, obj, "removeListener", JS_DupValue(ctx, removeListener));
      }
      JS_FreeValue(ctx, removeListener);
    }
    JS_FreeValue(ctx, proto);
  }
  JS_FreeValue(ctx, EventEmitter);
  JS_FreeValue(ctx, events_module);
}

// Get signal name from number
const char* signal_name(int signal_num) {
#ifdef _WIN32
  // Windows has limited signal support
  switch (signal_num) {
    case SIGTERM:
      return "SIGTERM";
    case SIGINT:
      return "SIGINT";
    default:
      return NULL;
  }
#else
  // POSIX signals
  switch (signal_num) {
    case SIGHUP:
      return "SIGHUP";
    case SIGINT:
      return "SIGINT";
    case SIGQUIT:
      return "SIGQUIT";
    case SIGILL:
      return "SIGILL";
    case SIGTRAP:
      return "SIGTRAP";
    case SIGABRT:
      return "SIGABRT";
    case SIGBUS:
      return "SIGBUS";
    case SIGFPE:
      return "SIGFPE";
    case SIGKILL:
      return "SIGKILL";
    case SIGUSR1:
      return "SIGUSR1";
    case SIGSEGV:
      return "SIGSEGV";
    case SIGUSR2:
      return "SIGUSR2";
    case SIGPIPE:
      return "SIGPIPE";
    case SIGALRM:
      return "SIGALRM";
    case SIGTERM:
      return "SIGTERM";
    case SIGCHLD:
      return "SIGCHLD";
    case SIGCONT:
      return "SIGCONT";
    case SIGSTOP:
      return "SIGSTOP";
    case SIGTSTP:
      return "SIGTSTP";
    case SIGTTIN:
      return "SIGTTIN";
    case SIGTTOU:
      return "SIGTTOU";
    default:
      return NULL;
  }
#endif
}

// Get signal number from name
int signal_from_name(const char* name) {
#ifdef _WIN32
  if (strcmp(name, "SIGTERM") == 0)
    return SIGTERM;
  if (strcmp(name, "SIGINT") == 0)
    return SIGINT;
  return -1;
#else
  if (strcmp(name, "SIGHUP") == 0)
    return SIGHUP;
  if (strcmp(name, "SIGINT") == 0)
    return SIGINT;
  if (strcmp(name, "SIGQUIT") == 0)
    return SIGQUIT;
  if (strcmp(name, "SIGILL") == 0)
    return SIGILL;
  if (strcmp(name, "SIGTRAP") == 0)
    return SIGTRAP;
  if (strcmp(name, "SIGABRT") == 0)
    return SIGABRT;
  if (strcmp(name, "SIGBUS") == 0)
    return SIGBUS;
  if (strcmp(name, "SIGFPE") == 0)
    return SIGFPE;
  if (strcmp(name, "SIGKILL") == 0)
    return SIGKILL;
  if (strcmp(name, "SIGUSR1") == 0)
    return SIGUSR1;
  if (strcmp(name, "SIGSEGV") == 0)
    return SIGSEGV;
  if (strcmp(name, "SIGUSR2") == 0)
    return SIGUSR2;
  if (strcmp(name, "SIGPIPE") == 0)
    return SIGPIPE;
  if (strcmp(name, "SIGALRM") == 0)
    return SIGALRM;
  if (strcmp(name, "SIGTERM") == 0)
    return SIGTERM;
  if (strcmp(name, "SIGCHLD") == 0)
    return SIGCHLD;
  if (strcmp(name, "SIGCONT") == 0)
    return SIGCONT;
  if (strcmp(name, "SIGSTOP") == 0)
    return SIGSTOP;
  if (strcmp(name, "SIGTSTP") == 0)
    return SIGTSTP;
  if (strcmp(name, "SIGTTIN") == 0)
    return SIGTTIN;
  if (strcmp(name, "SIGTTOU") == 0)
    return SIGTTOU;
  return -1;
#endif
}

// Emit an event on an object
void emit_event(JSContext* ctx, JSValue obj, const char* event, int argc, JSValue* argv) {
  JSValue emit_func = JS_GetPropertyStr(ctx, obj, "emit");
  if (JS_IsFunction(ctx, emit_func)) {
    JSValue event_name = JS_NewString(ctx, event);

    // Build arguments array: [event_name, ...argv]
    JSValue* emit_argv = js_malloc(ctx, sizeof(JSValue) * (argc + 1));
    if (emit_argv) {
      emit_argv[0] = event_name;
      for (int i = 0; i < argc; i++) {
        emit_argv[i + 1] = argv[i];
      }

      JSValue result = JS_Call(ctx, emit_func, obj, argc + 1, emit_argv);
      JS_FreeValue(ctx, result);

      js_free(ctx, emit_argv);
    }
    JS_FreeValue(ctx, event_name);
  }
  JS_FreeValue(ctx, emit_func);
}

// Free a NULL-terminated string array
void free_string_array(char** arr) {
  if (!arr)
    return;

  for (int i = 0; arr[i]; i++) {
    free(arr[i]);
  }
  free(arr);
}

// Copy a NULL-terminated string array
char** copy_string_array(const char** arr) {
  if (!arr)
    return NULL;

  // Count entries
  int count = 0;
  while (arr[count])
    count++;

  // Allocate array
  char** copy = malloc(sizeof(char*) * (count + 1));
  if (!copy)
    return NULL;

  // Copy strings
  for (int i = 0; i < count; i++) {
    copy[i] = strdup(arr[i]);
    if (!copy[i]) {
      // Cleanup on failure
      for (int j = 0; j < i; j++) {
        free(copy[j]);
      }
      free(copy);
      return NULL;
    }
  }
  copy[count] = NULL;

  return copy;
}
