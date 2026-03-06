#include "ubi_async_wrap.h"

#include <unordered_map>

#include "internal_binding/helpers.h"
#include "ubi_module_loader.h"

namespace {

struct AsyncWrapCache {
  napi_ref binding_ref = nullptr;
  napi_ref async_id_fields_ref = nullptr;
  napi_ref queue_destroy_ref = nullptr;
};

std::unordered_map<napi_env, AsyncWrapCache> g_async_wrap_cache;

napi_value GetRefValue(napi_env env, napi_ref ref) {
  if (env == nullptr || ref == nullptr) return nullptr;
  napi_value value = nullptr;
  if (napi_get_reference_value(env, ref, &value) != napi_ok || value == nullptr) {
    return nullptr;
  }
  return value;
}

napi_value ResolveInternalBinding(napi_env env, const char* name) {
  if (env == nullptr || name == nullptr) return nullptr;

  napi_value global = internal_binding::GetGlobal(env);
  if (global == nullptr) return nullptr;

  napi_value internal_binding = UbiGetInternalBinding(env);
  if (internal_binding == nullptr) {
    if (napi_get_named_property(env, global, "internalBinding", &internal_binding) != napi_ok ||
        internal_binding == nullptr) {
      return nullptr;
    }
  }

  napi_valuetype type = napi_undefined;
  if (napi_typeof(env, internal_binding, &type) != napi_ok || type != napi_function) {
    return nullptr;
  }

  napi_value binding_name = nullptr;
  if (napi_create_string_utf8(env, name, NAPI_AUTO_LENGTH, &binding_name) != napi_ok ||
      binding_name == nullptr) {
    return nullptr;
  }

  napi_value binding = nullptr;
  napi_value argv[1] = {binding_name};
  if (napi_call_function(env, global, internal_binding, 1, argv, &binding) != napi_ok ||
      binding == nullptr) {
    bool pending = false;
    if (napi_is_exception_pending(env, &pending) == napi_ok && pending) {
      napi_value ignored = nullptr;
      napi_get_and_clear_last_exception(env, &ignored);
    }
    return nullptr;
  }
  return binding;
}

AsyncWrapCache& GetCache(napi_env env) {
  return g_async_wrap_cache[env];
}

napi_value GetAsyncWrapBinding(napi_env env) {
  AsyncWrapCache& cache = GetCache(env);
  napi_value binding = GetRefValue(env, cache.binding_ref);
  if (binding != nullptr) return binding;

  binding = ResolveInternalBinding(env, "async_wrap");
  if (binding == nullptr) return nullptr;

  if (cache.binding_ref != nullptr) {
    napi_delete_reference(env, cache.binding_ref);
    cache.binding_ref = nullptr;
  }
  napi_create_reference(env, binding, 1, &cache.binding_ref);
  return binding;
}

double* GetAsyncIdFields(napi_env env) {
  AsyncWrapCache& cache = GetCache(env);
  napi_value fields = GetRefValue(env, cache.async_id_fields_ref);
  if (fields == nullptr) {
    napi_value binding = GetAsyncWrapBinding(env);
    if (binding == nullptr) return nullptr;
    if (napi_get_named_property(env, binding, "async_id_fields", &fields) != napi_ok ||
        fields == nullptr) {
      return nullptr;
    }
    if (cache.async_id_fields_ref != nullptr) {
      napi_delete_reference(env, cache.async_id_fields_ref);
      cache.async_id_fields_ref = nullptr;
    }
    napi_create_reference(env, fields, 1, &cache.async_id_fields_ref);
  }

  bool is_typedarray = false;
  if (napi_is_typedarray(env, fields, &is_typedarray) != napi_ok || !is_typedarray) {
    return nullptr;
  }

  napi_typedarray_type type = napi_uint8_array;
  size_t length = 0;
  void* data = nullptr;
  napi_value arraybuffer = nullptr;
  size_t byte_offset = 0;
  if (napi_get_typedarray_info(env,
                               fields,
                               &type,
                               &length,
                               &data,
                               &arraybuffer,
                               &byte_offset) != napi_ok ||
      data == nullptr ||
      type != napi_float64_array ||
      length < 4) {
    return nullptr;
  }

  return static_cast<double*>(data);
}

napi_value GetQueueDestroyFunction(napi_env env) {
  AsyncWrapCache& cache = GetCache(env);
  napi_value fn = GetRefValue(env, cache.queue_destroy_ref);
  if (fn != nullptr) return fn;

  napi_value binding = GetAsyncWrapBinding(env);
  if (binding == nullptr) return nullptr;
  if (napi_get_named_property(env, binding, "queueDestroyAsyncId", &fn) != napi_ok ||
      fn == nullptr) {
    return nullptr;
  }

  napi_valuetype type = napi_undefined;
  if (napi_typeof(env, fn, &type) != napi_ok || type != napi_function) {
    return nullptr;
  }

  if (cache.queue_destroy_ref != nullptr) {
    napi_delete_reference(env, cache.queue_destroy_ref);
    cache.queue_destroy_ref = nullptr;
  }
  napi_create_reference(env, fn, 1, &cache.queue_destroy_ref);
  return fn;
}

}  // namespace

int64_t UbiAsyncWrapNextId(napi_env env) {
  double* fields = GetAsyncIdFields(env);
  if (fields == nullptr) return 1;

  constexpr size_t kAsyncIdCounter = 2;
  fields[kAsyncIdCounter] += 1;
  return static_cast<int64_t>(fields[kAsyncIdCounter]);
}

void UbiAsyncWrapQueueDestroyId(napi_env env, int64_t async_id) {
  if (env == nullptr || async_id <= 0) return;

  napi_value binding = GetAsyncWrapBinding(env);
  napi_value queue_destroy = GetQueueDestroyFunction(env);
  if (binding == nullptr || queue_destroy == nullptr) return;

  napi_value async_id_value = nullptr;
  if (napi_create_int64(env, async_id, &async_id_value) != napi_ok || async_id_value == nullptr) {
    return;
  }

  napi_value ignored = nullptr;
  napi_value argv[1] = {async_id_value};
  if (napi_call_function(env, binding, queue_destroy, 1, argv, &ignored) != napi_ok) {
    bool pending = false;
    if (napi_is_exception_pending(env, &pending) == napi_ok && pending) {
      napi_value ignored_error = nullptr;
      napi_get_and_clear_last_exception(env, &ignored_error);
    }
  }
}

void UbiAsyncWrapReset(napi_env env, int64_t* async_id) {
  if (async_id == nullptr) return;
  if (*async_id > 0) UbiAsyncWrapQueueDestroyId(env, *async_id);
  *async_id = UbiAsyncWrapNextId(env);
}
