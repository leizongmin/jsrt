#include "runtime.h"
#include "../util/debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wasm_c_api.h>
#include <wasm_export.h>

// Default configuration values
#define JSRT_WASM_DEFAULT_HEAP_SIZE (1 * 1024 * 1024)  // 1MB
#define JSRT_WASM_DEFAULT_STACK_SIZE (64 * 1024)       // 64KB
#define JSRT_WASM_DEFAULT_MAX_MODULES 16

// Configurable limits
#define JSRT_WASM_MIN_HEAP_SIZE (64 * 1024)         // 64KB
#define JSRT_WASM_MAX_HEAP_SIZE (16 * 1024 * 1024)  // 16MB
#define JSRT_WASM_MIN_STACK_SIZE (16 * 1024)        // 16KB
#define JSRT_WASM_MAX_STACK_SIZE (256 * 1024)       // 256KB

static bool wamr_initialized = false;
static jsrt_wasm_config_t current_config;

// WASM C API objects (for Memory/Table/Global)
static wasm_engine_t* wasm_engine = NULL;
static wasm_store_t* wasm_store = NULL;

jsrt_wasm_config_t jsrt_wasm_default_config(void) {
  jsrt_wasm_config_t config;
  config.heap_size = JSRT_WASM_DEFAULT_HEAP_SIZE;
  config.stack_size = JSRT_WASM_DEFAULT_STACK_SIZE;
  config.max_modules = JSRT_WASM_DEFAULT_MAX_MODULES;
  return config;
}

int jsrt_wasm_init(void) {
  if (wamr_initialized) {
    JSRT_Debug("WAMR already initialized");
    return 0;
  }

  JSRT_Debug("Initializing WAMR runtime");

  // Use default configuration for initialization
  current_config = jsrt_wasm_default_config();

  // Initialize WAMR runtime with system allocator (simplest)
  RuntimeInitArgs init_args;
  memset(&init_args, 0, sizeof(RuntimeInitArgs));

  // Use system allocator instead of pool allocator to avoid heap buffer issues
  init_args.mem_alloc_type = Alloc_With_System_Allocator;

  if (!wasm_runtime_full_init(&init_args)) {
    JSRT_Debug("Failed to initialize WAMR runtime");
    return -1;
  }

  // Initialize WASM C API engine and store for Memory/Table/Global objects
  wasm_engine = wasm_engine_new();
  if (!wasm_engine) {
    JSRT_Debug("Failed to create WASM C API engine");
    wasm_runtime_destroy();
    return -1;
  }

  wasm_store = wasm_store_new(wasm_engine);
  if (!wasm_store) {
    JSRT_Debug("Failed to create WASM C API store");
    wasm_engine_delete(wasm_engine);
    wasm_engine = NULL;
    wasm_runtime_destroy();
    return -1;
  }

  wamr_initialized = true;
  JSRT_Debug("WAMR runtime initialized successfully (with C API store)");
  return 0;
}

void jsrt_wasm_cleanup(void) {
  if (!wamr_initialized) {
    return;
  }

  JSRT_Debug("Cleaning up WAMR runtime");

  // Clean up C API objects first
  if (wasm_store) {
    wasm_store_delete(wasm_store);
    wasm_store = NULL;
  }
  if (wasm_engine) {
    wasm_engine_delete(wasm_engine);
    wasm_engine = NULL;
  }

  wasm_runtime_destroy();
  wamr_initialized = false;
  JSRT_Debug("WAMR runtime cleanup completed");
}

int jsrt_wasm_configure(const jsrt_wasm_config_t* config) {
  if (!config) {
    return -1;
  }

  // Validate configuration values
  if (config->heap_size < JSRT_WASM_MIN_HEAP_SIZE || config->heap_size > JSRT_WASM_MAX_HEAP_SIZE) {
    JSRT_Debug("Invalid heap size: %u (must be between %u and %u)", config->heap_size, JSRT_WASM_MIN_HEAP_SIZE,
               JSRT_WASM_MAX_HEAP_SIZE);
    return -1;
  }

  if (config->stack_size < JSRT_WASM_MIN_STACK_SIZE || config->stack_size > JSRT_WASM_MAX_STACK_SIZE) {
    JSRT_Debug("Invalid stack size: %u (must be between %u and %u)", config->stack_size, JSRT_WASM_MIN_STACK_SIZE,
               JSRT_WASM_MAX_STACK_SIZE);
    return -1;
  }

  if (wamr_initialized) {
    JSRT_Debug("Cannot configure WAMR after initialization");
    return -1;
  }

  current_config = *config;
  JSRT_Debug("WAMR configuration updated: heap=%u stack=%u max_modules=%u", config->heap_size, config->stack_size,
             config->max_modules);
  return 0;
}

wasm_store_t* jsrt_wasm_get_store(void) {
  if (!wamr_initialized) {
    JSRT_Debug("WARNING: Accessing WASM store before initialization");
    return NULL;
  }
  return wasm_store;
}