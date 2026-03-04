#include "internal_binding/dispatch.h"

#include "internal_binding/helpers.h"

namespace internal_binding {

namespace {

bool IsObjectLike(napi_env env, napi_value value) {
  if (value == nullptr) return false;
  napi_valuetype type = napi_undefined;
  if (napi_typeof(env, value, &type) != napi_ok) return false;
  return type == napi_object || type == napi_function;
}

napi_value TryRequireModule(napi_env env, const char* module_name) {
  const napi_value undefined = Undefined(env);
  const napi_value global = GetGlobal(env);
  if (global == nullptr) return undefined;

  napi_value require_fn = nullptr;
  if (napi_get_named_property(env, global, "require", &require_fn) != napi_ok || require_fn == nullptr) {
    return undefined;
  }
  napi_valuetype require_type = napi_undefined;
  if (napi_typeof(env, require_fn, &require_type) != napi_ok || require_type != napi_function) {
    return undefined;
  }

  napi_value module_name_v = nullptr;
  if (napi_create_string_utf8(env, module_name, NAPI_AUTO_LENGTH, &module_name_v) != napi_ok ||
      module_name_v == nullptr) {
    return undefined;
  }

  napi_value argv[1] = {module_name_v};
  napi_value module = nullptr;
  if (napi_call_function(env, global, require_fn, 1, argv, &module) != napi_ok || module == nullptr) {
    bool pending = false;
    if (napi_is_exception_pending(env, &pending) == napi_ok && pending) {
      napi_value ignored = nullptr;
      napi_get_and_clear_last_exception(env, &ignored);
    }
    return undefined;
  }
  return module;
}

napi_value TryGetModuleConstants(napi_env env, const char* module_name) {
  const napi_value undefined = Undefined(env);
  const napi_value module = TryRequireModule(env, module_name);
  if (IsUndefined(env, module) || !IsObjectLike(env, module)) return undefined;

  napi_value constants = nullptr;
  if (napi_get_named_property(env, module, "constants", &constants) != napi_ok ||
      constants == nullptr || !IsObjectLike(env, constants)) {
    return undefined;
  }
  return constants;
}

bool CopyNumericOwnProperties(napi_env env, napi_value src, napi_value dst) {
  napi_value keys = nullptr;
  if (napi_get_property_names(env, src, &keys) != napi_ok || keys == nullptr) return false;
  uint32_t key_count = 0;
  if (napi_get_array_length(env, keys, &key_count) != napi_ok) return false;
  for (uint32_t i = 0; i < key_count; i++) {
    napi_value key = nullptr;
    if (napi_get_element(env, keys, i, &key) != napi_ok || key == nullptr) continue;

    napi_value value = nullptr;
    if (napi_get_property(env, src, key, &value) != napi_ok || value == nullptr) continue;

    napi_valuetype t = napi_undefined;
    if (napi_typeof(env, value, &t) != napi_ok || t != napi_number) continue;

    napi_set_property(env, dst, key, value);
  }
  return true;
}

napi_value CreateInternalConstants(napi_env env) {
  napi_value internal = nullptr;
  if (napi_create_object(env, &internal) != napi_ok || internal == nullptr) return Undefined(env);
  SetInt32(env, internal, "EXTENSIONLESS_FORMAT_JAVASCRIPT", 0);
  SetInt32(env, internal, "EXTENSIONLESS_FORMAT_WASM", 1);
  return internal;
}

napi_value CreateTraceConstants(napi_env env) {
  napi_value trace = nullptr;
  if (napi_create_object(env, &trace) != napi_ok || trace == nullptr) return Undefined(env);
  SetInt32(env, trace, "TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN", 'b');
  SetInt32(env, trace, "TRACE_EVENT_PHASE_NESTABLE_ASYNC_END", 'e');
  return trace;
}

napi_value CreateDefaultFsConstants(napi_env env) {
  napi_value fs_obj = nullptr;
  if (napi_create_object(env, &fs_obj) != napi_ok || fs_obj == nullptr) return Undefined(env);
  SetInt32(env, fs_obj, "F_OK", 0);
  SetInt32(env, fs_obj, "R_OK", 4);
  SetInt32(env, fs_obj, "W_OK", 2);
  SetInt32(env, fs_obj, "X_OK", 1);
  return fs_obj;
}

napi_value CreateEmptyObject(napi_env env) {
  napi_value out = nullptr;
  if (napi_create_object(env, &out) != napi_ok || out == nullptr) return Undefined(env);
  return out;
}

napi_value CreateDefaultOsConstants(napi_env env) {
  napi_value os_obj = nullptr;
  if (napi_create_object(env, &os_obj) != napi_ok || os_obj == nullptr) return Undefined(env);
  napi_value signals = nullptr;
  if (napi_create_object(env, &signals) == napi_ok && signals != nullptr) {
    napi_set_named_property(env, os_obj, "signals", signals);
  }
  return os_obj;
}

void SetNamedObjectIfValid(napi_env env, napi_value target, const char* key, napi_value value) {
  if (value != nullptr && !IsUndefined(env, value) && IsObjectLike(env, value)) {
    napi_set_named_property(env, target, key, value);
  }
}

}  // namespace

napi_value ResolveConstants(napi_env env, const ResolveOptions& /*options*/) {
  const napi_value undefined = Undefined(env);
  napi_value out = nullptr;
  if (napi_create_object(env, &out) != napi_ok || out == nullptr) return undefined;

  SetNamedObjectIfValid(env, out, "os", CreateDefaultOsConstants(env));
  SetNamedObjectIfValid(env, out, "fs", CreateDefaultFsConstants(env));
  SetNamedObjectIfValid(env, out, "crypto", CreateEmptyObject(env));
  SetNamedObjectIfValid(env, out, "zlib", CreateEmptyObject(env));

  // Prefer native ubi constants when present.
  const napi_value os_constants = GetGlobalNamed(env, "__ubi_os_constants");
  SetNamedObjectIfValid(env, out, "os", os_constants);

  const napi_value fs_binding = GetGlobalNamed(env, "__ubi_fs");
  if (!IsUndefined(env, fs_binding) && IsObjectLike(env, fs_binding)) {
    napi_value fs_constants_obj = nullptr;
    if (napi_create_object(env, &fs_constants_obj) == napi_ok && fs_constants_obj != nullptr) {
      CopyNumericOwnProperties(env, fs_binding, fs_constants_obj);
      SetNamedObjectIfValid(env, out, "fs", fs_constants_obj);
    }
  }

  // Fill high-impact constant surfaces from public module constants when available.
  SetNamedObjectIfValid(env, out, "zlib", TryGetModuleConstants(env, "zlib"));
  SetNamedObjectIfValid(env, out, "crypto", TryGetModuleConstants(env, "crypto"));

  napi_value os_module_constants = TryGetModuleConstants(env, "os");
  if (!IsUndefined(env, os_module_constants) && IsObjectLike(env, os_module_constants)) {
    SetNamedObjectIfValid(env, out, "os", os_module_constants);
  }
  napi_value fs_module_constants = TryGetModuleConstants(env, "fs");
  if (!IsUndefined(env, fs_module_constants) && IsObjectLike(env, fs_module_constants)) {
    SetNamedObjectIfValid(env, out, "fs", fs_module_constants);
  }

  SetNamedObjectIfValid(env, out, "internal", CreateInternalConstants(env));
  SetNamedObjectIfValid(env, out, "trace", CreateTraceConstants(env));

  return out;
}

}  // namespace internal_binding
