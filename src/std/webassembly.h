#ifndef __JSRT_STD_WEBASSEMBLY_H__
#define __JSRT_STD_WEBASSEMBLY_H__

#include <wasm_export.h>
#include "../runtime.h"

void JSRT_RuntimeSetupStdWebAssembly(JSRT_Runtime* rt);

/**
 * Extract WAMR instance from WebAssembly.Instance JavaScript object
 * @param ctx JavaScript context
 * @param instance_obj WebAssembly.Instance object
 * @return WAMR instance handle or NULL if invalid
 */
wasm_module_inst_t jsrt_webassembly_get_instance(JSContext* ctx, JSValue instance_obj);

#endif