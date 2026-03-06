#include "ubi_task_queue.h"

#include <unordered_map>
#include <unordered_set>

#include "internal_binding/helpers.h"
#include "unofficial_napi.h"

namespace {

struct TaskQueueBindingState {
  napi_ref binding_ref = nullptr;
  napi_ref tick_callback_ref = nullptr;
  napi_ref promise_reject_callback_ref = nullptr;
  napi_ref tick_info_ref = nullptr;
};

std::unordered_map<napi_env, TaskQueueBindingState> g_task_queue_states;
std::unordered_set<napi_env> g_task_queue_cleanup_hook_registered;

void DeleteRefIfAny(napi_env env, napi_ref* ref_slot) {
  if (env == nullptr || ref_slot == nullptr || *ref_slot == nullptr) return;
  napi_delete_reference(env, *ref_slot);
  *ref_slot = nullptr;
}

void OnTaskQueueEnvCleanup(void* arg) {
  napi_env env = static_cast<napi_env>(arg);
  g_task_queue_cleanup_hook_registered.erase(env);

  auto it = g_task_queue_states.find(env);
  if (it == g_task_queue_states.end()) return;

  DeleteRefIfAny(env, &it->second.binding_ref);
  DeleteRefIfAny(env, &it->second.tick_callback_ref);
  DeleteRefIfAny(env, &it->second.promise_reject_callback_ref);
  DeleteRefIfAny(env, &it->second.tick_info_ref);
  g_task_queue_states.erase(it);
}

void EnsureTaskQueueCleanupHook(napi_env env) {
  if (env == nullptr) return;
  auto [it, inserted] = g_task_queue_cleanup_hook_registered.emplace(env);
  if (!inserted) return;
  if (napi_add_env_cleanup_hook(env, OnTaskQueueEnvCleanup, env) != napi_ok) {
    g_task_queue_cleanup_hook_registered.erase(it);
  }
}

TaskQueueBindingState& GetTaskQueueState(napi_env env) {
  EnsureTaskQueueCleanupHook(env);
  return g_task_queue_states[env];
}

static napi_value TaskQueueEnqueueMicrotask(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = {nullptr};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok || argc < 1) return nullptr;

#if defined(UBI_BUNDLED_NAPI_V8)
  if (unofficial_napi_enqueue_microtask(env, argv[0]) == napi_ok) {
    return internal_binding::Undefined(env);
  }
#endif

  napi_value global = nullptr;
  if (napi_get_global(env, &global) != napi_ok || global == nullptr) return nullptr;

  napi_value queue_microtask = nullptr;
  if (napi_get_named_property(env, global, "queueMicrotask", &queue_microtask) == napi_ok &&
      queue_microtask != nullptr) {
    napi_valuetype t = napi_undefined;
    if (napi_typeof(env, queue_microtask, &t) == napi_ok && t == napi_function) {
      napi_value ignored = nullptr;
      napi_call_function(env, global, queue_microtask, 1, argv, &ignored);
    }
  }

  return internal_binding::Undefined(env);
}

static napi_value TaskQueueRunMicrotasks(napi_env env, napi_callback_info /*info*/) {
  (void)unofficial_napi_process_microtasks(env);
  return internal_binding::Undefined(env);
}

static napi_value TaskQueueSetTickCallback(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = {nullptr};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok || argc < 1) return nullptr;

  auto& st = GetTaskQueueState(env);
  DeleteRefIfAny(env, &st.tick_callback_ref);

  napi_valuetype t = napi_undefined;
  if (napi_typeof(env, argv[0], &t) == napi_ok && t == napi_function) {
    napi_create_reference(env, argv[0], 1, &st.tick_callback_ref);
  }

  return internal_binding::Undefined(env);
}

static napi_value TaskQueueSetPromiseRejectCallback(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = {nullptr};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok || argc < 1) return nullptr;

#if defined(UBI_BUNDLED_NAPI_V8)
  (void)unofficial_napi_set_promise_reject_callback(env, argv[0]);
#endif

  auto& st = GetTaskQueueState(env);
  DeleteRefIfAny(env, &st.promise_reject_callback_ref);

  napi_valuetype t = napi_undefined;
  if (napi_typeof(env, argv[0], &t) == napi_ok && t == napi_function) {
    napi_create_reference(env, argv[0], 1, &st.promise_reject_callback_ref);
  }

  return internal_binding::Undefined(env);
}

}  // namespace

napi_value UbiGetOrCreateTaskQueueBinding(napi_env env) {
  if (env == nullptr) return nullptr;

  auto& st = GetTaskQueueState(env);
  if (st.binding_ref != nullptr) {
    napi_value existing = nullptr;
    if (napi_get_reference_value(env, st.binding_ref, &existing) == napi_ok && existing != nullptr) {
      return existing;
    }
  }

  napi_value binding = nullptr;
  if (napi_create_object(env, &binding) != napi_ok || binding == nullptr) return nullptr;

  auto define_method = [&](const char* name, napi_callback cb) -> bool {
    napi_value fn = nullptr;
    if (napi_create_function(env, name, NAPI_AUTO_LENGTH, cb, nullptr, &fn) != napi_ok || fn == nullptr) {
      return false;
    }
    return napi_set_named_property(env, binding, name, fn) == napi_ok;
  };

  if (!define_method("enqueueMicrotask", TaskQueueEnqueueMicrotask) ||
      !define_method("setTickCallback", TaskQueueSetTickCallback) ||
      !define_method("runMicrotasks", TaskQueueRunMicrotasks) ||
      !define_method("setPromiseRejectCallback", TaskQueueSetPromiseRejectCallback)) {
    return nullptr;
  }

  napi_value global = nullptr;
  if (napi_get_global(env, &global) != napi_ok || global == nullptr) return nullptr;
  napi_value int32_array_ctor = nullptr;
  if (napi_get_named_property(env, global, "Int32Array", &int32_array_ctor) != napi_ok ||
      int32_array_ctor == nullptr) {
    return nullptr;
  }
  napi_value length = nullptr;
  if (napi_create_uint32(env, 2, &length) != napi_ok || length == nullptr) return nullptr;
  napi_value tick_info = nullptr;
  napi_value tick_args[1] = {length};
  if (napi_new_instance(env, int32_array_ctor, 1, tick_args, &tick_info) != napi_ok || tick_info == nullptr) {
    return nullptr;
  }
  if (napi_set_named_property(env, binding, "tickInfo", tick_info) != napi_ok) return nullptr;
  DeleteRefIfAny(env, &st.tick_info_ref);
  if (napi_create_reference(env, tick_info, 1, &st.tick_info_ref) != napi_ok || st.tick_info_ref == nullptr) {
    return nullptr;
  }

  napi_value promise_events = nullptr;
  if (napi_create_object(env, &promise_events) != napi_ok || promise_events == nullptr) return nullptr;
  auto set_event_const = [&](const char* name, int32_t value) -> bool {
    napi_value v = nullptr;
    return napi_create_int32(env, value, &v) == napi_ok && v != nullptr &&
           napi_set_named_property(env, promise_events, name, v) == napi_ok;
  };
  if (!set_event_const("kPromiseRejectWithNoHandler", 0) ||
      !set_event_const("kPromiseHandlerAddedAfterReject", 1) ||
      !set_event_const("kPromiseResolveAfterResolved", 2) ||
      !set_event_const("kPromiseRejectAfterResolved", 3)) {
    return nullptr;
  }
  if (napi_set_named_property(env, binding, "promiseRejectEvents", promise_events) != napi_ok) return nullptr;

  DeleteRefIfAny(env, &st.binding_ref);
  if (napi_create_reference(env, binding, 1, &st.binding_ref) != napi_ok || st.binding_ref == nullptr) {
    return nullptr;
  }

  return binding;
}

napi_status UbiRunTaskQueueTickCallback(napi_env env, bool* called) {
  if (called != nullptr) {
    *called = false;
  }
  if (env == nullptr) {
    return napi_invalid_arg;
  }

  auto it = g_task_queue_states.find(env);
  if (it == g_task_queue_states.end() || it->second.tick_callback_ref == nullptr) {
    return napi_ok;
  }

  napi_value tick_cb = nullptr;
  napi_status status = napi_get_reference_value(env, it->second.tick_callback_ref, &tick_cb);
  if (status != napi_ok || tick_cb == nullptr) {
    return status == napi_ok ? napi_generic_failure : status;
  }

  napi_value global = nullptr;
  status = napi_get_global(env, &global);
  if (status != napi_ok || global == nullptr) {
    return status == napi_ok ? napi_generic_failure : status;
  }

  napi_value process = nullptr;
  if (napi_get_named_property(env, global, "process", &process) != napi_ok || process == nullptr) {
    process = global;
  }

  napi_value ignored = nullptr;
  status = napi_call_function(env, process, tick_cb, 0, nullptr, &ignored);
  if (status == napi_ok && called != nullptr) {
    *called = true;
  }
  return status;
}

bool UbiGetTaskQueueFlags(napi_env env, bool* has_tick_scheduled, bool* has_rejection_to_warn) {
  if (has_tick_scheduled != nullptr) {
    *has_tick_scheduled = false;
  }
  if (has_rejection_to_warn != nullptr) {
    *has_rejection_to_warn = false;
  }
  if (env == nullptr) {
    return false;
  }

  auto it = g_task_queue_states.find(env);
  if (it == g_task_queue_states.end() || it->second.tick_info_ref == nullptr) {
    return false;
  }

  napi_value tick_info = nullptr;
  if (napi_get_reference_value(env, it->second.tick_info_ref, &tick_info) != napi_ok || tick_info == nullptr) {
    return false;
  }
  auto read_flag = [&](uint32_t index, bool* out) -> bool {
    if (out == nullptr) return true;
    napi_value value = nullptr;
    uint32_t raw = 0;
    if (napi_get_element(env, tick_info, index, &value) != napi_ok || value == nullptr ||
        napi_get_value_uint32(env, value, &raw) != napi_ok) {
      return false;
    }
    *out = raw != 0;
    return true;
  };

  return read_flag(0, has_tick_scheduled) && read_flag(1, has_rejection_to_warn);
}
