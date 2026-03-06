#ifndef UBI_ENV_LOOP_H_
#define UBI_ENV_LOOP_H_

#include "node_api.h"

#include <uv.h>

napi_status UbiEnsureEnvLoop(napi_env env, uv_loop_t** loop_out);
uv_loop_t* UbiGetEnvLoop(napi_env env);
uv_loop_t* UbiGetExistingEnvLoop(napi_env env);

#endif  // UBI_ENV_LOOP_H_
