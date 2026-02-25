#ifndef NAPI_V8_JS_NATIVE_API_H_
#define NAPI_V8_JS_NATIVE_API_H_

#include "../../node/src/js_native_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// Buffer API (implemented in napi-v8; declared in Node's node_api.h but not
// in js_native_api.h). Exposed here so napi-v8 consumers can use it.
NAPI_EXTERN napi_status NAPI_CDECL napi_is_buffer(napi_env env,
                                                  napi_value value,
                                                  bool* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_get_buffer_info(napi_env env,
                                                        napi_value value,
                                                        void** data,
                                                        size_t* length);

#ifdef __cplusplus
}
#endif

#endif  // NAPI_V8_JS_NATIVE_API_H_
