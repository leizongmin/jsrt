#include "net_internal.h"
#include <uv.h>

// Class IDs for networking classes (exported, not static)
JSClassID js_server_class_id;
JSClassID js_socket_class_id;

// Deferred cleanup list for handles with embedded structs
// These will be freed after uv_loop_close to avoid use-after-free during uv_walk
typedef struct cleanup_item {
  void* ptr;
  struct cleanup_item* next;
} cleanup_item_t;

static cleanup_item_t* cleanup_list_head = NULL;
static uv_mutex_t cleanup_mutex;
static bool cleanup_mutex_initialized = false;

void net_add_to_cleanup_list(void* ptr) {
  if (!cleanup_mutex_initialized) {
    uv_mutex_init(&cleanup_mutex);
    cleanup_mutex_initialized = true;
  }

  cleanup_item_t* item = (cleanup_item_t*)malloc(sizeof(cleanup_item_t));
  item->ptr = ptr;

  uv_mutex_lock(&cleanup_mutex);
  item->next = cleanup_list_head;
  cleanup_list_head = item;
  uv_mutex_unlock(&cleanup_mutex);
}

void net_cleanup_deferred() {
  if (!cleanup_mutex_initialized) {
    return;
  }

  uv_mutex_lock(&cleanup_mutex);
  cleanup_item_t* current = cleanup_list_head;
  while (current) {
    cleanup_item_t* next = current->next;
    free(current->ptr);  // Free the actual socket/server struct
    free(current);       // Free the list item
    current = next;
  }
  cleanup_list_head = NULL;
  uv_mutex_unlock(&cleanup_mutex);

  uv_mutex_destroy(&cleanup_mutex);
  cleanup_mutex_initialized = false;
}

// Class definitions
static JSClassDef js_socket_class = {
    "Socket",
    .finalizer = js_socket_finalizer,
};

static JSClassDef js_server_class = {
    "Server",
    .finalizer = js_server_finalizer,
};

// Main module functions
JSValue js_net_create_server(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return js_server_constructor(ctx, JS_UNDEFINED, argc, argv);
}

JSValue js_net_connect(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue socket = js_socket_constructor(ctx, JS_UNDEFINED, 0, NULL);
  if (JS_IsException(socket)) {
    return socket;
  }

  // Call connect on the socket
  JSValue connect_method = JS_GetPropertyStr(ctx, socket, "connect");
  JSValue result = JS_Call(ctx, connect_method, socket, argc, argv);
  JS_FreeValue(ctx, connect_method);

  if (JS_IsException(result)) {
    JS_FreeValue(ctx, socket);
    return result;
  }

  JS_FreeValue(ctx, result);
  return socket;
}

// Module initialization
JSValue JSRT_InitNodeNet(JSContext* ctx) {
  JSValue net_module = JS_NewObject(ctx);

  // Register class IDs
  JS_NewClassID(&js_socket_class_id);
  JS_NewClassID(&js_server_class_id);

  // Create class definitions
  JS_NewClass(JS_GetRuntime(ctx), js_socket_class_id, &js_socket_class);
  JS_NewClass(JS_GetRuntime(ctx), js_server_class_id, &js_server_class);

  // Create constructors
  JSValue socket_ctor = JS_NewCFunction2(ctx, js_socket_constructor, "Socket", 0, JS_CFUNC_constructor, 0);
  JSValue server_ctor = JS_NewCFunction2(ctx, js_server_constructor, "Server", 0, JS_CFUNC_constructor, 0);

  // Module functions
  JS_SetPropertyStr(ctx, net_module, "createServer", JS_NewCFunction(ctx, js_net_create_server, "createServer", 1));
  JS_SetPropertyStr(ctx, net_module, "connect", JS_NewCFunction(ctx, js_net_connect, "connect", 2));

  // Export constructors
  JS_SetPropertyStr(ctx, net_module, "Socket", socket_ctor);
  JS_SetPropertyStr(ctx, net_module, "Server", server_ctor);

  return net_module;
}

// ES Module support
int js_node_net_init(JSContext* ctx, JSModuleDef* m) {
  JSValue net_module = JSRT_InitNodeNet(ctx);

  // Export individual functions with proper memory management
  JSValue createServer = JS_GetPropertyStr(ctx, net_module, "createServer");
  JS_SetModuleExport(ctx, m, "createServer", JS_DupValue(ctx, createServer));
  JS_FreeValue(ctx, createServer);

  JSValue connect = JS_GetPropertyStr(ctx, net_module, "connect");
  JS_SetModuleExport(ctx, m, "connect", JS_DupValue(ctx, connect));
  JS_FreeValue(ctx, connect);

  JSValue Socket = JS_GetPropertyStr(ctx, net_module, "Socket");
  JS_SetModuleExport(ctx, m, "Socket", JS_DupValue(ctx, Socket));
  JS_FreeValue(ctx, Socket);

  JSValue Server = JS_GetPropertyStr(ctx, net_module, "Server");
  JS_SetModuleExport(ctx, m, "Server", JS_DupValue(ctx, Server));
  JS_FreeValue(ctx, Server);

  // Also export the whole module as default
  JS_SetModuleExport(ctx, m, "default", JS_DupValue(ctx, net_module));

  JS_FreeValue(ctx, net_module);
  return 0;
}
