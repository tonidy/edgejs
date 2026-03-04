#include "internal_binding/dispatch.h"

#include "internal_binding/helpers.h"

namespace internal_binding {

namespace {

napi_value RunScript(napi_env env, const char* source) {
  napi_value script = nullptr;
  if (napi_create_string_utf8(env, source, NAPI_AUTO_LENGTH, &script) != napi_ok || script == nullptr) {
    return nullptr;
  }
  napi_value out = nullptr;
  if (napi_run_script(env, script, &out) != napi_ok || out == nullptr) {
    bool pending = false;
    if (napi_is_exception_pending(env, &pending) == napi_ok && pending) {
      napi_value ignored = nullptr;
      napi_get_and_clear_last_exception(env, &ignored);
    }
    return nullptr;
  }
  return out;
}

bool ValueToBool(napi_env env, napi_value value, bool fallback = false) {
  if (value == nullptr) return fallback;
  bool out = fallback;
  if (napi_get_value_bool(env, value, &out) != napi_ok) return fallback;
  return out;
}

bool HasNamedProperty(napi_env env, napi_value obj, const char* key) {
  if (obj == nullptr) return false;
  bool has_prop = false;
  return napi_has_named_property(env, obj, key, &has_prop) == napi_ok && has_prop;
}

napi_value GetNamedProperty(napi_env env, napi_value obj, const char* key) {
  napi_value out = nullptr;
  if (obj == nullptr || napi_get_named_property(env, obj, key, &out) != napi_ok || out == nullptr) {
    return nullptr;
  }
  return out;
}

bool HasIntl(napi_env env) {
  napi_value global = GetGlobal(env);
  return HasNamedProperty(env, global, "Intl");
}

bool HasProcessNested(napi_env env, const char* first_key, const char* second_key) {
  napi_value global = GetGlobal(env);
  napi_value process = GetNamedProperty(env, global, "process");
  if (process == nullptr) return false;
  napi_value first = GetNamedProperty(env, process, first_key);
  if (first == nullptr) return false;
  return HasNamedProperty(env, first, second_key);
}

bool HasDebugBinding(napi_env env) {
  static constexpr const char* kDebugBindingScript = R"JS((function() {
    try {
      if (typeof internalBinding !== 'function') return false;
      const debug = internalBinding('debug');
      return !!(debug && typeof debug.getV8FastApiCallCount === 'function');
    } catch {
      return false;
    }
  })())JS";
  return ValueToBool(env, RunScript(env, kDebugBindingScript), false);
}

napi_value ConfigGetDefaultLocale(napi_env env, napi_callback_info /*info*/) {
  static constexpr const char* kScript = R"JS((function() {
    try {
      if (typeof Intl !== 'object' || Intl === null ||
          typeof Intl.DateTimeFormat !== 'function') {
        return undefined;
      }
      const locale = new Intl.DateTimeFormat().resolvedOptions().locale;
      return typeof locale === 'string' ? locale : undefined;
    } catch {
      return undefined;
    }
  })())JS";
  napi_value out = RunScript(env, kScript);
  return out != nullptr ? out : Undefined(env);
}

}  // namespace

napi_value ResolveConfig(napi_env env, const ResolveOptions& /*options*/) {
  napi_value out = nullptr;
  if (napi_create_object(env, &out) != napi_ok || out == nullptr) return Undefined(env);

  const bool has_intl = HasIntl(env);
  const bool has_inspector = HasProcessNested(env, "features", "inspector");
  const bool has_tracing = HasProcessNested(env, "features", "tracing");
  const bool has_openssl = HasProcessNested(env, "versions", "openssl");
  const bool openssl_is_boringssl = HasProcessNested(env, "versions", "boringssl");
  const bool is_debug_build = HasDebugBinding(env);

  bool has_small_icu = false;
  if (has_intl) {
    static constexpr const char* kSmallIcuScript = R"JS((function() {
      try {
        const locales = Intl.DateTimeFormat.supportedLocalesOf(['es', 'fr', 'zh-CN']);
        return locales.length < 3;
      } catch {
        return false;
      }
    })())JS";
    has_small_icu = ValueToBool(env, RunScript(env, kSmallIcuScript), false);
  }

  bool fips_mode = false;
  if (has_openssl) {
    static constexpr const char* kFipsScript = R"JS((function() {
      try {
        if (typeof require !== 'function') return false;
        const crypto = require('crypto');
        return typeof crypto.getFips === 'function' && !!crypto.getFips();
      } catch {
        return false;
      }
    })())JS";
    fips_mode = ValueToBool(env, RunScript(env, kFipsScript), false);
  }

  SetBool(env, out, "hasIntl", has_intl);
  SetBool(env, out, "hasSmallICU", has_small_icu);
  SetBool(env, out, "hasInspector", has_inspector);
  SetBool(env, out, "hasTracing", has_tracing);
  SetBool(env, out, "hasOpenSSL", has_openssl);
  SetBool(env, out, "openSSLIsBoringSSL", openssl_is_boringssl);
  SetBool(env, out, "fipsMode", fips_mode);
  SetBool(env, out, "hasNodeOptions", true);
  SetBool(env, out, "noBrowserGlobals", false);
  SetBool(env, out, "isDebugBuild", is_debug_build);

  napi_value get_default_locale = nullptr;
  if (napi_create_function(env,
                           "getDefaultLocale",
                           NAPI_AUTO_LENGTH,
                           ConfigGetDefaultLocale,
                           nullptr,
                           &get_default_locale) == napi_ok &&
      get_default_locale != nullptr) {
    napi_set_named_property(env, out, "getDefaultLocale", get_default_locale);
  }

  return out;
}

}  // namespace internal_binding
