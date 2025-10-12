/**
 * Protocol Handler Registry
 *
 * Manages registration and lookup of protocol handlers for module loading.
 * Supports protocols like file://, http://, https://, and potentially zip://.
 */

#ifndef __JSRT_MODULE_PROTOCOL_REGISTRY_H__
#define __JSRT_MODULE_PROTOCOL_REGISTRY_H__

#include <stdbool.h>
#include <stddef.h>
#include "../../util/file.h"

/**
 * Protocol Handler Structure
 *
 * Defines the interface for loading content from a specific protocol.
 */
typedef struct {
  const char* protocol_name;  // "file", "http", "https", "zip"

  // Load content from protocol
  // url: Full URL including protocol (e.g., "file:///path/to/file.js")
  // user_data: Custom data passed during registration
  // Returns: JSRT_ReadFileResult with content or error
  JSRT_ReadFileResult (*load)(const char* url, void* user_data);

  // Cleanup handler resources (optional, can be NULL)
  // Called when handler is unregistered or at shutdown
  void (*cleanup)(void* user_data);

  // Custom user data for the handler
  void* user_data;
} JSRT_ProtocolHandler;

/**
 * Initialize protocol handlers system
 *
 * Must be called before any protocol operations.
 * Initializes the internal registry and default handlers.
 */
void jsrt_init_protocol_handlers(void);

/**
 * Cleanup protocol handlers system
 *
 * Should be called at shutdown.
 * Frees all registered handlers and cleans up resources.
 */
void jsrt_cleanup_protocol_handlers(void);

/**
 * Register a protocol handler
 *
 * @param protocol Protocol name (e.g., "file", "http", "https")
 * @param handler Handler structure with load function
 * @return true on success, false if protocol already registered or invalid
 *
 * Note: The handler structure is copied internally, so the caller
 * can free their copy after registration.
 */
bool jsrt_register_protocol_handler(const char* protocol, const JSRT_ProtocolHandler* handler);

/**
 * Get handler for protocol
 *
 * @param protocol Protocol name to look up
 * @return Pointer to handler structure, or NULL if not found
 *
 * Note: The returned pointer is valid until the handler is unregistered
 * or jsrt_cleanup_protocol_handlers() is called.
 */
const JSRT_ProtocolHandler* jsrt_get_protocol_handler(const char* protocol);

/**
 * Unregister handler for protocol
 *
 * @param protocol Protocol name to unregister
 * @return true if handler was found and removed, false otherwise
 *
 * Calls the cleanup function if provided before removing the handler.
 */
bool jsrt_unregister_protocol_handler(const char* protocol);

/**
 * Check if a protocol is registered
 *
 * @param protocol Protocol name to check
 * @return true if registered, false otherwise
 */
bool jsrt_has_protocol_handler(const char* protocol);

/**
 * Get list of registered protocols
 *
 * @param protocols Output array to store protocol names
 * @param max_protocols Maximum number of protocols to return
 * @return Number of protocols written to array
 *
 * Useful for debugging and diagnostics.
 */
size_t jsrt_get_registered_protocols(const char** protocols, size_t max_protocols);

#endif  // __JSRT_MODULE_PROTOCOL_REGISTRY_H__
