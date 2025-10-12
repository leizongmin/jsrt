/**
 * Protocol Handler Registry Implementation
 */

#include "protocol_registry.h"
#include <stdlib.h>
#include <string.h>
#include "../util/module_debug.h"

// Maximum number of protocols (reasonable limit)
#define MAX_PROTOCOLS 16

// Registry entry structure
typedef struct {
  char* protocol_name;
  JSRT_ProtocolHandler handler;
  bool in_use;
} ProtocolRegistryEntry;

// Global registry
static struct {
  ProtocolRegistryEntry entries[MAX_PROTOCOLS];
  size_t count;
  bool initialized;
} g_protocol_registry = {.count = 0, .initialized = false};

/**
 * Initialize protocol handlers system
 */
void jsrt_init_protocol_handlers(void) {
  if (g_protocol_registry.initialized) {
    MODULE_Debug_Protocol("Protocol registry already initialized");
    return;
  }

  MODULE_Debug_Protocol("Initializing protocol registry");

  // Clear all entries
  memset(&g_protocol_registry, 0, sizeof(g_protocol_registry));
  g_protocol_registry.initialized = true;

  MODULE_Debug_Protocol("Protocol registry initialized successfully");
}

/**
 * Cleanup protocol handlers system
 */
void jsrt_cleanup_protocol_handlers(void) {
  if (!g_protocol_registry.initialized) {
    return;
  }

  MODULE_Debug_Protocol("Cleaning up protocol registry");

  // Cleanup all registered handlers
  for (size_t i = 0; i < MAX_PROTOCOLS; i++) {
    if (g_protocol_registry.entries[i].in_use) {
      ProtocolRegistryEntry* entry = &g_protocol_registry.entries[i];

      MODULE_Debug_Protocol("Cleaning up protocol handler: %s", entry->protocol_name);

      // Call cleanup function if provided
      if (entry->handler.cleanup) {
        entry->handler.cleanup(entry->handler.user_data);
      }

      // Free protocol name
      free(entry->protocol_name);
      entry->protocol_name = NULL;
      entry->in_use = false;
    }
  }

  g_protocol_registry.count = 0;
  g_protocol_registry.initialized = false;

  MODULE_Debug_Protocol("Protocol registry cleaned up");
}

/**
 * Find entry by protocol name
 */
static ProtocolRegistryEntry* find_entry(const char* protocol) {
  if (!protocol) {
    return NULL;
  }

  for (size_t i = 0; i < MAX_PROTOCOLS; i++) {
    if (g_protocol_registry.entries[i].in_use && strcmp(g_protocol_registry.entries[i].protocol_name, protocol) == 0) {
      return &g_protocol_registry.entries[i];
    }
  }

  return NULL;
}

/**
 * Find free entry slot
 */
static ProtocolRegistryEntry* find_free_entry(void) {
  for (size_t i = 0; i < MAX_PROTOCOLS; i++) {
    if (!g_protocol_registry.entries[i].in_use) {
      return &g_protocol_registry.entries[i];
    }
  }
  return NULL;
}

/**
 * Register a protocol handler
 */
bool jsrt_register_protocol_handler(const char* protocol, const JSRT_ProtocolHandler* handler) {
  if (!g_protocol_registry.initialized) {
    MODULE_Debug_Error("Protocol registry not initialized");
    return false;
  }

  if (!protocol || !handler || !handler->load) {
    MODULE_Debug_Error("Invalid arguments to jsrt_register_protocol_handler");
    return false;
  }

  // Check if protocol already registered
  if (find_entry(protocol)) {
    MODULE_Debug_Error("Protocol '%s' already registered", protocol);
    return false;
  }

  // Find free slot
  ProtocolRegistryEntry* entry = find_free_entry();
  if (!entry) {
    MODULE_Debug_Error("Protocol registry full (max %d protocols)", MAX_PROTOCOLS);
    return false;
  }

  // Copy protocol name
  entry->protocol_name = strdup(protocol);
  if (!entry->protocol_name) {
    MODULE_Debug_Error("Failed to allocate memory for protocol name");
    return false;
  }

  // Copy handler
  entry->handler = *handler;
  entry->handler.protocol_name = entry->protocol_name;  // Use our copy
  entry->in_use = true;
  g_protocol_registry.count++;

  MODULE_Debug_Protocol("Registered protocol handler: %s (total: %zu)", protocol, g_protocol_registry.count);

  return true;
}

/**
 * Get handler for protocol
 */
const JSRT_ProtocolHandler* jsrt_get_protocol_handler(const char* protocol) {
  if (!g_protocol_registry.initialized) {
    MODULE_Debug_Error("Protocol registry not initialized");
    return NULL;
  }

  if (!protocol) {
    return NULL;
  }

  ProtocolRegistryEntry* entry = find_entry(protocol);
  if (entry) {
    MODULE_Debug_Protocol("Found handler for protocol: %s", protocol);
    return &entry->handler;
  }

  MODULE_Debug_Protocol("No handler found for protocol: %s", protocol);
  return NULL;
}

/**
 * Unregister handler for protocol
 */
bool jsrt_unregister_protocol_handler(const char* protocol) {
  if (!g_protocol_registry.initialized) {
    MODULE_Debug_Error("Protocol registry not initialized");
    return false;
  }

  if (!protocol) {
    return false;
  }

  ProtocolRegistryEntry* entry = find_entry(protocol);
  if (!entry) {
    MODULE_Debug_Protocol("Protocol '%s' not registered", protocol);
    return false;
  }

  MODULE_Debug_Protocol("Unregistering protocol handler: %s", protocol);

  // Call cleanup function if provided
  if (entry->handler.cleanup) {
    entry->handler.cleanup(entry->handler.user_data);
  }

  // Free resources
  free(entry->protocol_name);
  entry->protocol_name = NULL;
  entry->in_use = false;
  g_protocol_registry.count--;

  MODULE_Debug_Protocol("Unregistered protocol handler: %s (remaining: %zu)", protocol, g_protocol_registry.count);

  return true;
}

/**
 * Check if a protocol is registered
 */
bool jsrt_has_protocol_handler(const char* protocol) {
  if (!g_protocol_registry.initialized) {
    return false;
  }

  return find_entry(protocol) != NULL;
}

/**
 * Get list of registered protocols
 */
size_t jsrt_get_registered_protocols(const char** protocols, size_t max_protocols) {
  if (!g_protocol_registry.initialized || !protocols || max_protocols == 0) {
    return 0;
  }

  size_t count = 0;
  for (size_t i = 0; i < MAX_PROTOCOLS && count < max_protocols; i++) {
    if (g_protocol_registry.entries[i].in_use) {
      protocols[count++] = g_protocol_registry.entries[i].protocol_name;
    }
  }

  return count;
}
