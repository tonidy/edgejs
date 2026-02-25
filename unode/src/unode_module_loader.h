#ifndef UNODE_MODULE_LOADER_H_
#define UNODE_MODULE_LOADER_H_

#include "js_native_api.h"

napi_status UnodeInstallModuleLoader(napi_env env, const char* entry_script_path);

#endif  // UNODE_MODULE_LOADER_H_
