#ifndef __JSRT_WASM_RUNTIME_H__
#define __JSRT_WASM_RUNTIME_H__

#include <stdbool.h>
#include <stdint.h>

// Runtime configuration
typedef struct {
  uint32_t heap_size;    // WASM linear memory heap size
  uint32_t stack_size;   // WASM execution stack size
  uint32_t max_modules;  // Maximum concurrent modules
} jsrt_wasm_config_t;

// Runtime lifecycle
int jsrt_wasm_init(void);
void jsrt_wasm_cleanup(void);

// Runtime configuration
int jsrt_wasm_configure(const jsrt_wasm_config_t* config);

// Get default configuration
jsrt_wasm_config_t jsrt_wasm_default_config(void);

#endif