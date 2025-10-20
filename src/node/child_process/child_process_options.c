#include "../../util/debug.h"
#include "child_process_internal.h"

// TODO: Implement options parsing
int parse_spawn_options(JSContext* ctx, JSValueConst options_obj, JSChildProcessOptions* options) {
  // Placeholder implementation
  memset(options, 0, sizeof(*options));
  options->uid = -1;
  options->gid = -1;
  return 0;
}

// TODO: Implement options cleanup
void free_spawn_options(JSChildProcessOptions* options) {
  // Placeholder - will need to free allocated strings
}
