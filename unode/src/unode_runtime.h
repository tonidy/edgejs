#ifndef UNODE_RUNTIME_H_
#define UNODE_RUNTIME_H_

#include <string>

#include "js_native_api.h"

int UnodeRunScriptFile(napi_env env, const char* script_path, std::string* error_out);

#endif  // UNODE_RUNTIME_H_
