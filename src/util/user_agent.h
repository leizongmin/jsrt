#ifndef JSRT_USER_AGENT_H
#define JSRT_USER_AGENT_H

#include <quickjs.h>

/**
 * Generate a dynamic user-agent string using real version numbers from process.versions
 *
 * @param ctx JavaScript context to access process.versions
 * @return Dynamically allocated user-agent string (caller must free)
 *         Returns "jsrt/1.0" as fallback if process.versions is not available
 */
char* jsrt_generate_user_agent(JSContext* ctx);

/**
 * Generate a simple user-agent string without JavaScript context
 * Uses compile-time version information when available
 *
 * @return Static user-agent string (no need to free)
 */
const char* jsrt_get_static_user_agent(void);

#endif  // JSRT_USER_AGENT_H