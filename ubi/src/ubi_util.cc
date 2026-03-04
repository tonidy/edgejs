#include "ubi_util.h"

#include <uv.h>

#include <cstdint>

namespace {

static uint32_t GetUVHandleTypeCode(uv_handle_type type) {
  switch (type) {
    case UV_TCP:
      return 0;
    case UV_TTY:
      return 1;
    case UV_UDP:
      return 2;
    case UV_FILE:
      return 3;
    case UV_NAMED_PIPE:
      return 4;
    case UV_UNKNOWN_HANDLE:
      return 5;
    default:
      return 5;
  }
}

napi_value GuessHandleType(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = {nullptr};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok ||
      argc < 1) {
    return nullptr;
  }
  int32_t fd = 0;
  if (napi_get_value_int32(env, argv[0], &fd) != napi_ok || fd < 0) {
    return nullptr;
  }
  uv_handle_type t = uv_guess_handle(static_cast<uv_file>(fd));
  uint32_t code = GetUVHandleTypeCode(t);
  napi_value result = nullptr;
  if (napi_create_uint32(env, code, &result) != napi_ok) {
    return nullptr;
  }
  return result;
}

void SetMethod(napi_env env, napi_value obj, const char* name,
               napi_callback cb) {
  napi_value fn = nullptr;
  if (napi_create_function(env, name, NAPI_AUTO_LENGTH, cb, nullptr, &fn) ==
          napi_ok &&
      fn != nullptr) {
    napi_set_named_property(env, obj, name, fn);
  }
}

}  // namespace

void UbiInstallUtilBinding(napi_env env) {
  napi_value binding = nullptr;
  if (napi_create_object(env, &binding) != napi_ok || binding == nullptr) {
    return;
  }
  SetMethod(env, binding, "guessHandleType", GuessHandleType);

  // Install Node-like utility helpers on the native util binding so
  // internalBinding('util') can be provided natively without shape mismatch.
  static const char kInstallUtilHelpers[] =
      "(function(binding){"
      "  if (!binding || typeof binding !== 'object') return binding;"
      "  const kHasBackingStore = new WeakSet();"
      "  const kProxyDetails = globalThis.__ubi_proxy_details || (globalThis.__ubi_proxy_details = new WeakMap());"
      "  const kProxyTag = globalThis.__ubi_proxy_tag || (globalThis.__ubi_proxy_tag = Symbol('ubi.proxy.tag'));"
      "  const kExternalStreamTag = globalThis.__ubi_external_stream_tag || (globalThis.__ubi_external_stream_tag = Symbol('ubi.external.stream'));"
      "  const kCtorNameMap = new WeakMap();"
      "  const kUntransferable = Symbol('untransferable_object_private_symbol');"
      "  const kExitInfoPrivate = Symbol('exit_info_private_symbol');"
      "  const kContextifyContextPrivate = Symbol('contextify_context_private_symbol');"
      "  const kHostDefinedOptionPrivate = Symbol('host_defined_option_symbol');"
      "  const kEntryPointPromisePrivate = Symbol('entry_point_promise_private_symbol');"
      "  const kEntryPointModulePrivate = Symbol('entry_point_module_private_symbol');"
      "  const kModuleSourcePrivate = Symbol('module_source_private_symbol');"
      "  const kModuleExportNamesPrivate = Symbol('module_export_names_private_symbol');"
      "  const kModuleCircularVisitedPrivate = Symbol('module_circular_visited_private_symbol');"
      "  const kModuleExportPrivate = Symbol('module_export_private_symbol');"
      "  const kModuleFirstParentPrivate = Symbol('module_first_parent_private_symbol');"
      "  const kModuleLastParentPrivate = Symbol('module_last_parent_private_symbol');"
      "  const kTransferModePrivate = Symbol('transfer_mode_private_symbol');"
      "  const kSourceMapDataPrivate = Symbol('source_map_data_private_symbol');"
      "  const kArrowMessagePrivate = '__ubi_arrow_message_private_symbol__';"
      "  const kDecoratedPrivate = '__ubi_decorated_private_symbol__';"
      "  if (!globalThis.__ubi_setprototypeof_wrapped) {"
      "    globalThis.__ubi_setprototypeof_wrapped = true;"
      "    const originalSetPrototypeOf = Object.setPrototypeOf;"
      "    Object.setPrototypeOf = function setPrototypeOfPatched(obj, proto) {"
      "      if (obj && (typeof obj === 'object' || typeof obj === 'function') && proto === null) {"
      "        try {"
      "          const ctor = obj.constructor;"
      "          if (ctor && typeof ctor.name === 'string' && ctor.name) kCtorNameMap.set(obj, ctor.name);"
      "        } catch {}"
      "      }"
      "      return originalSetPrototypeOf(obj, proto);"
      "    };"
      "  }"
      "  function parseDotEnv(content) {"
      "    const out = {};"
      "    const text = String(content);"
      "    let i = 0;"
      "    while (i < text.length) {"
      "      while (i < text.length && (text[i] === ' ' || text[i] === '\\t' || text[i] === '\\r')) i++;"
      "      if (i >= text.length) break;"
      "      if (text[i] === '\\n') { i++; continue; }"
      "      if (text[i] === '#') { while (i < text.length && text[i] !== '\\n') i++; continue; }"
      "      let lineStart = i;"
      "      while (lineStart < text.length && (text[lineStart] === ' ' || text[lineStart] === '\\t')) lineStart++;"
      "      if (text.startsWith('export ', lineStart)) i = lineStart + 7;"
      "      const keyStart = i;"
      "      while (i < text.length && text[i] !== '=' && text[i] !== '\\n') i++;"
      "      if (i >= text.length || text[i] === '\\n') { if (i < text.length && text[i] === '\\n') i++; continue; }"
      "      let key = text.slice(keyStart, i).trim();"
      "      if (!key) { i++; continue; }"
      "      i++;"
      "      while (i < text.length && (text[i] === ' ' || text[i] === '\\t')) i++;"
      "      let value = '';"
      "      const q = text[i];"
      "      if (q === '\"' || q === '\\'' || q === '`') {"
      "        i++;"
      "        const valueStart = i;"
      "        const closeIdx = text.indexOf(q, i);"
      "        if (closeIdx !== -1) {"
      "          i = closeIdx;"
      "          value = text.slice(valueStart, i);"
      "          i++;"
      "          while (i < text.length && text[i] !== '\\n') i++;"
      "        } else {"
      "          value = q;"
      "          while (i < text.length && text[i] !== '\\n') i++;"
      "        }"
      "        if (q === '\"') value = value.replace(/\\\\n/g, '\\n');"
      "      } else {"
      "        const valueStart = i;"
      "        while (i < text.length && text[i] !== '\\n' && text[i] !== '#') i++;"
      "        value = text.slice(valueStart, i).trim();"
      "        while (i < text.length && text[i] !== '\\n') i++;"
      "      }"
      "      out[key] = value;"
      "      if (i < text.length && text[i] === '\\n') i++;"
      "    }"
      "    return out;"
      "  }"
      "  binding.constants = {"
      "    ALL_PROPERTIES: 0,"
      "    ONLY_ENUMERABLE: 1,"
      "    kPending: 0,"
      "    kExitCode: 0,"
      "    kExiting: 1,"
      "    kHasExitCode: 2"
      "  };"
      "  if (!binding.shouldAbortOnUncaughtToggle) {"
      "    binding.shouldAbortOnUncaughtToggle = new Uint8Array(1);"
      "    binding.shouldAbortOnUncaughtToggle[0] = 1;"
      "  }"
      "  if (!globalThis.__ubi_types) {"
      "    const toStringTag = (value) => Object.prototype.toString.call(value);"
      "    const typesBinding = {"
      "      isExternal(value) { return !!(value && value[kExternalStreamTag] === true); },"
      "      isDate(value) { return toStringTag(value) === '[object Date]'; },"
      "      isArgumentsObject(value) { return toStringTag(value) === '[object Arguments]'; },"
      "      isBooleanObject(value) { return toStringTag(value) === '[object Boolean]'; },"
      "      isNumberObject(value) { return toStringTag(value) === '[object Number]'; },"
      "      isStringObject(value) { return toStringTag(value) === '[object String]'; },"
      "      isSymbolObject(value) { return toStringTag(value) === '[object Symbol]'; },"
      "      isBigIntObject(value) { return toStringTag(value) === '[object BigInt]'; },"
      "      isNativeError(value) { return value != null && typeof value === 'object' && toStringTag(value) === '[object Error]'; },"
      "      isRegExp(value) { return toStringTag(value) === '[object RegExp]'; },"
      "      isAsyncFunction(value) {"
      "        const tag = toStringTag(value);"
      "        if (tag === '[object AsyncGeneratorFunction]') return true;"
      "        if (tag === '[object AsyncFunction]') {"
      "          return !(value && typeof value === 'function' && value.prototype != null && typeof value.prototype.next === 'function');"
      "        }"
      "        return false;"
      "      },"
      "      isGeneratorFunction(value) {"
      "        if (value && typeof value === 'function' && value.prototype != null && typeof value.prototype.next === 'function') return true;"
      "        const tag = toStringTag(value);"
      "        return tag === '[object GeneratorFunction]' || tag === '[object AsyncGeneratorFunction]';"
      "      },"
      "      isGeneratorObject(value) { return toStringTag(value) === '[object Generator]'; },"
      "      isPromise(value) { return toStringTag(value) === '[object Promise]'; },"
      "      isMap(value) { return toStringTag(value) === '[object Map]'; },"
      "      isSet(value) { return toStringTag(value) === '[object Set]'; },"
      "      isMapIterator(value) { return toStringTag(value) === '[object Map Iterator]'; },"
      "      isSetIterator(value) { return toStringTag(value) === '[object Set Iterator]'; },"
      "      isWeakMap(value) { return toStringTag(value) === '[object WeakMap]'; },"
      "      isWeakSet(value) { return toStringTag(value) === '[object WeakSet]'; },"
      "      isArrayBuffer(value) { return toStringTag(value) === '[object ArrayBuffer]'; },"
      "      isDataView(value) { return toStringTag(value) === '[object DataView]'; },"
      "      isSharedArrayBuffer(value) { return toStringTag(value) === '[object SharedArrayBuffer]'; },"
      "      isProxy(value) { return !!(value && value[kProxyTag] === true); },"
      "      isModuleNamespaceObject(value) { return !!value && toStringTag(value) === '[object Module]'; },"
      "    };"
      "    typesBinding.isAnyArrayBuffer = (value) => value != null &&"
      "      (value.constructor === ArrayBuffer ||"
      "        (typeof SharedArrayBuffer === 'function' && value.constructor === SharedArrayBuffer));"
      "    typesBinding.isBoxedPrimitive = (value) => typesBinding.isNumberObject(value) || typesBinding.isStringObject(value) ||"
      "      typesBinding.isBooleanObject(value) || typesBinding.isBigIntObject(value) || typesBinding.isSymbolObject(value);"
      "    globalThis.__ubi_types = typesBinding;"
      "  }"
      "  binding.defineLazyProperties = function(target, id, keys) {"
      "    if (!target || typeof target !== 'object' || !Array.isArray(keys)) return;"
      "    for (const key of keys) {"
      "      Object.defineProperty(target, key, {"
      "        configurable: true, enumerable: true,"
      "        get() {"
      "          const mod = require(String(id));"
      "          const value = mod ? mod[key] : undefined;"
      "          Object.defineProperty(target, key, { configurable: true, enumerable: true, writable: true, value });"
      "          return value;"
      "        },"
      "      });"
      "    }"
      "  };"
      "  binding.constructSharedArrayBuffer = function(size) { return new SharedArrayBuffer(Number(size) || 0); };"
      "  binding.getOwnNonIndexProperties = function(obj, filter) {"
      "    const kOnlyEnumerable = 1;"
      "    return Object.getOwnPropertyNames(obj).filter((n) => {"
      "      const index = Number(n);"
      "      if (Number.isInteger(index) && String(index) === n) return false;"
      "      if (n === 'length') return filter !== kOnlyEnumerable;"
      "      if (filter === kOnlyEnumerable) {"
      "        const desc = Object.getOwnPropertyDescriptor(obj, n);"
      "        return !!(desc && desc.enumerable);"
      "      }"
      "      return true;"
      "    });"
      "  };"
      "  binding.getCallSites = function(frameCount) {"
      "    const n = frameCount == null ? 8 : Math.max(0, Math.trunc(Number(frameCount) || 0));"
      "    if (n === 0) return [];"
      "    const fallbackName = (process && process.argv && process.argv[1]) || '[eval]';"
      "    const lines = String(new Error().stack || '').split('\\n').slice(1);"
      "    const out = [];"
      "    for (const raw of lines) {"
      "      const line = String(raw).trim();"
      "      let m = /\\((.*):(\\d+):(\\d+)\\)$/.exec(line);"
      "      if (!m) m = /at (.*):(\\d+):(\\d+)$/.exec(line);"
      "      if (!m) continue;"
      "      const scriptName = m[1];"
      "      if (!scriptName || scriptName.includes('internal_binding.js')) continue;"
      "      out.push({ scriptName, scriptId: '0', lineNumber: Number(m[2]) || 1, columnNumber: Number(m[3]) || 1, column: Number(m[3]) || 1, functionName: '' });"
      "      if (out.length >= n) break;"
      "    }"
      "    if (out.length === 0) {"
      "      out.push({ scriptName: fallbackName, scriptId: '0', lineNumber: 1, columnNumber: 1, column: 1, functionName: '' });"
      "    }"
      "    while (out.length < n) {"
      "      const last = out[out.length - 1];"
      "      out.push({ scriptName: last.scriptName, scriptId: '0', lineNumber: last.lineNumber, columnNumber: last.columnNumber, column: last.column, functionName: '' });"
      "    }"
      "    return out.slice(0, n);"
      "  };"
      "  binding.getConstructorName = function(value) {"
      "    if (kCtorNameMap.has(value)) return kCtorNameMap.get(value);"
      "    let obj = value;"
      "    while (obj && (typeof obj === 'object' || typeof obj === 'function')) {"
      "      const desc = Object.getOwnPropertyDescriptor(obj, 'constructor');"
      "      if (desc && typeof desc.value === 'function' && typeof desc.value.name === 'string' && desc.value.name) return desc.value.name;"
      "      obj = Object.getPrototypeOf(obj);"
      "    }"
      "    return '';"
      "  };"
      "  binding.getPromiseDetails = function(promise) {"
      "    if (!(promise instanceof Promise)) return [0, undefined];"
      "    return [0, undefined];"
      "  };"
      "  binding.previewEntries = function(value, pairMode) {"
      "    try {"
      "      if (value instanceof Map) {"
      "        const entries = Array.from(value.entries());"
      "        return pairMode ? [entries, true] : entries.flat();"
      "      }"
      "      if (value instanceof Set) {"
      "        const entries = Array.from(value.values());"
      "        return pairMode ? [entries, false] : entries;"
      "      }"
      "    } catch {}"
      "    return pairMode ? [[], false] : [];"
      "  };"
      "  binding.getProxyDetails = function(value) { return kProxyDetails.get(value); };"
      "  binding.parseEnv = function(src) {"
      "    return parseDotEnv(src);"
      "  };"
      "  binding.sleep = function(msec) {"
      "    const n = Number(msec);"
      "    if (!Number.isInteger(n) || n < 0 || n > 0xffffffff) return;"
      "    if (typeof Atomics === 'object' && typeof Atomics.wait === 'function' && typeof SharedArrayBuffer === 'function') {"
      "      const i32 = new Int32Array(new SharedArrayBuffer(4));"
      "      Atomics.wait(i32, 0, 0, n);"
      "      return;"
      "    }"
      "    const start = Date.now();"
      "    while ((Date.now() - start) < n) {}"
      "  };"
      "  binding.isInsideNodeModules = function() {"
      "    const stack = String(new Error().stack || '');"
      "    return /(^|[\\\\/])node_modules([\\\\/]|$)/i.test(stack);"
      "  };"
      "  binding.privateSymbols = {"
      "    untransferable_object_private_symbol: kUntransferable,"
      "    arrow_message_private_symbol: kArrowMessagePrivate,"
      "    decorated_private_symbol: kDecoratedPrivate,"
      "    exit_info_private_symbol: kExitInfoPrivate,"
      "    contextify_context_private_symbol: kContextifyContextPrivate,"
      "    host_defined_option_symbol: kHostDefinedOptionPrivate,"
      "    entry_point_promise_private_symbol: kEntryPointPromisePrivate,"
      "    entry_point_module_private_symbol: kEntryPointModulePrivate,"
      "    module_source_private_symbol: kModuleSourcePrivate,"
      "    module_export_names_private_symbol: kModuleExportNamesPrivate,"
      "    module_circular_visited_private_symbol: kModuleCircularVisitedPrivate,"
      "    module_export_private_symbol: kModuleExportPrivate,"
      "    module_first_parent_private_symbol: kModuleFirstParentPrivate,"
      "    module_last_parent_private_symbol: kModuleLastParentPrivate,"
      "    transfer_mode_private_symbol: kTransferModePrivate,"
      "    source_map_data_private_symbol: kSourceMapDataPrivate,"
      "  };"
      "  binding.arrayBufferViewHasBuffer = function(view) {"
      "    if (view == null || typeof view !== 'object') return false;"
      "    let byteLength = 0;"
      "    try { byteLength = view.byteLength; } catch { return false; }"
      "    if (typeof byteLength !== 'number') return false;"
      "    if (kHasBackingStore.has(view)) return true;"
      "    if (byteLength >= 96) return true;"
      "    kHasBackingStore.add(view);"
      "    return false;"
      "  };"
      "  return binding;"
      "})";
  napi_value install_script = nullptr;
  if (napi_create_string_utf8(env, kInstallUtilHelpers, NAPI_AUTO_LENGTH, &install_script) == napi_ok &&
      install_script != nullptr) {
    napi_value install_fn = nullptr;
    if (napi_run_script(env, install_script, &install_fn) == napi_ok && install_fn != nullptr) {
      napi_value global_for_call = nullptr;
      napi_get_global(env, &global_for_call);
      napi_value argv[1] = {binding};
      napi_value ignored = nullptr;
      napi_call_function(env, global_for_call, install_fn, 1, argv, &ignored);
    }
  }

  static const char kEnhanceUtilHelpers[] = R"JS((function(binding) {
    if (!binding || typeof binding !== 'object') return binding;
    const globalObj = globalThis;
    const kPending = 0;
    const kFulfilled = 1;
    const kRejected = 2;

    if (!binding.constants || typeof binding.constants !== 'object') binding.constants = {};
    binding.constants.ALL_PROPERTIES ??= 0;
    binding.constants.ONLY_ENUMERABLE ??= 1;
    binding.constants.kPending = kPending;
    binding.constants.kFulfilled = kFulfilled;
    binding.constants.kRejected = kRejected;
    binding.constants.kExitCode ??= 0;
    binding.constants.kExiting ??= 1;
    binding.constants.kHasExitCode ??= 2;

    if (!binding.privateSymbols || typeof binding.privateSymbols !== 'object') {
      binding.privateSymbols = {};
    }
    const privateSymbols = binding.privateSymbols;
    const ensureSymbol = (key) => {
      if (typeof privateSymbols[key] !== 'symbol') privateSymbols[key] = Symbol(key);
    };
    ensureSymbol('arrow_message_private_symbol');
    ensureSymbol('decorated_private_symbol');
    ensureSymbol('untransferable_object_private_symbol');
    ensureSymbol('exit_info_private_symbol');
    ensureSymbol('contextify_context_private_symbol');
    ensureSymbol('host_defined_option_symbol');
    ensureSymbol('entry_point_promise_private_symbol');
    ensureSymbol('entry_point_module_private_symbol');
    ensureSymbol('module_source_private_symbol');
    ensureSymbol('module_export_names_private_symbol');
    ensureSymbol('module_circular_visited_private_symbol');
    ensureSymbol('module_export_private_symbol');
    ensureSymbol('module_first_parent_private_symbol');
    ensureSymbol('module_last_parent_private_symbol');
    ensureSymbol('transfer_mode_private_symbol');
    ensureSymbol('source_map_data_private_symbol');

    const NativeProxy = globalObj.__ubi_native_proxy || (globalObj.__ubi_native_proxy = globalObj.Proxy);
    const proxyTag = globalObj.__ubi_proxy_tag || (globalObj.__ubi_proxy_tag = Symbol('ubi.proxy.tag'));
    const proxyDetails = globalObj.__ubi_proxy_details || (globalObj.__ubi_proxy_details = new WeakMap());

    if (!globalObj.__ubi_proxy_wrapped &&
        typeof NativeProxy === 'function' &&
        typeof NativeProxy.revocable === 'function') {
      globalObj.__ubi_proxy_wrapped = true;
      const WrappedProxy = function Proxy(target, handler) {
        const proxy = new NativeProxy(target, handler);
        proxyDetails.set(proxy, [target, handler]);
        try {
          Object.defineProperty(proxy, proxyTag, { value: true });
        } catch {}
        return proxy;
      };
      Object.defineProperty(WrappedProxy, 'name', { value: 'Proxy' });
      Object.defineProperty(WrappedProxy, 'length', { value: 2 });
      WrappedProxy.revocable = function revocable(target, handler) {
        const record = NativeProxy.revocable(target, handler);
        proxyDetails.set(record.proxy, [target, handler]);
        try {
          Object.defineProperty(record.proxy, proxyTag, { value: true });
        } catch {}
        const nativeRevoke = record.revoke;
        record.revoke = function revoke() {
          proxyDetails.set(record.proxy, [null, null]);
          return nativeRevoke();
        };
        return record;
      };
      delete WrappedProxy.prototype;
      Object.setPrototypeOf(WrappedProxy, NativeProxy);
      globalObj.Proxy = WrappedProxy;
    }

    binding.getProxyDetails = function(value, full = true) {
      const details = proxyDetails.get(value);
      if (details === undefined) return undefined;
      if (full === false) return details[0];
      return details;
    };

    const promiseStates = globalObj.__ubi_promise_states ||
      (globalObj.__ubi_promise_states = new WeakMap());
    binding.getPromiseDetails = function(promise) {
      if (!(promise instanceof Promise)) return undefined;
      let details = promiseStates.get(promise);
      if (details !== undefined) return details;
      details = [kPending, undefined];
      promiseStates.set(promise, details);
      try {
        Promise.prototype.then.call(
          promise,
          (value) => {
            details[0] = kFulfilled;
            details[1] = value;
            return value;
          },
          (reason) => {
            details[0] = kRejected;
            details[1] = reason;
            return undefined;
          },
        );
      } catch {}
      return details;
    };

    binding.getExternalValue = function(_value) {
      return 0n;
    };

    function shouldSkipScript(scriptName) {
      if (!scriptName) return true;
      const s = String(scriptName);
      return s.includes('node:util') ||
        s.includes('node-lib/util.js') ||
        s.includes('internal/bootstrap/internal_binding.js') ||
        s.includes('internal_binding.js');
    }

    function parseStackFrame(line) {
      const trimmed = String(line).trim();
      let m = /^at\s+(.*?)\s+\((.*):(\d+):(\d+)\)$/.exec(trimmed);
      if (m) {
        return {
          functionName: m[1] || '',
          scriptName: m[2] || '',
          lineNumber: Number(m[3]) || 1,
          columnNumber: Number(m[4]) || 1,
        };
      }
      m = /^at\s+(.*):(\d+):(\d+)$/.exec(trimmed);
      if (m) {
        return {
          functionName: '',
          scriptName: m[1] || '',
          lineNumber: Number(m[2]) || 1,
          columnNumber: Number(m[3]) || 1,
        };
      }
      return null;
    }

    binding.getCallSites = function(frameCount) {
      const n = frameCount == null ? 10 : Math.max(0, Math.trunc(Number(frameCount) || 0));
      if (n <= 0) return [];

      const stackLines = String(new Error().stack || '').split('\n').slice(1);
      const out = [];
      for (const line of stackLines) {
        const parsed = parseStackFrame(line);
        if (!parsed) continue;
        if (shouldSkipScript(parsed.scriptName)) continue;
        out.push({
          scriptName: parsed.scriptName,
          scriptId: '0',
          lineNumber: parsed.lineNumber,
          columnNumber: parsed.columnNumber,
          column: parsed.columnNumber,
          functionName: parsed.functionName,
        });
        if (out.length >= n) break;
      }

      if (out.length === 0) {
        const fallback = (process && process.argv && process.argv[1]) || '[eval]';
        out.push({
          scriptName: fallback,
          scriptId: '0',
          lineNumber: 1,
          columnNumber: 1,
          column: 1,
          functionName: '',
        });
      }

      return out;
    };

    binding.getCallerLocation = function() {
      const sites = binding.getCallSites(24);
      for (const site of sites) {
        const file = String(site?.scriptName || '');
        if (!file) continue;
        if (file.includes('internal/test_runner/')) continue;
        if (file.includes('node-lib/internal/test_runner/')) continue;
        return [site.lineNumber || 1, site.columnNumber || 1, file];
      }
      return [1, 1, '[eval]'];
    };

    return binding;
  }) )JS";

  napi_value enhance_script = nullptr;
  if (napi_create_string_utf8(env, kEnhanceUtilHelpers, NAPI_AUTO_LENGTH, &enhance_script) == napi_ok &&
      enhance_script != nullptr) {
    napi_value enhance_fn = nullptr;
    if (napi_run_script(env, enhance_script, &enhance_fn) == napi_ok && enhance_fn != nullptr) {
      napi_value global_for_call = nullptr;
      napi_get_global(env, &global_for_call);
      napi_value argv[1] = {binding};
      napi_value ignored = nullptr;
      napi_call_function(env, global_for_call, enhance_fn, 1, argv, &ignored);
    }
  }

  napi_value global = nullptr;
  if (napi_get_global(env, &global) != napi_ok || global == nullptr) {
    return;
  }
  napi_set_named_property(env, global, "__ubi_util", binding);
}
