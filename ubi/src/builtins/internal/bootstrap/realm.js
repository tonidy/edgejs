'use strict';

const {
  internalBinding,
  primordials,
} = require('internal/bootstrap/internal_binding');

const kInternalPrefix = 'internal/';
const kSchemeOnlyBuiltinIds = new Set();

const { builtinIds = [] } = internalBinding('builtins') || {};
const allBuiltinSet = new Set(builtinIds);
const publicBuiltinIds = builtinIds.filter((id) =>
  !id.startsWith(kInternalPrefix) && id !== 'internal_test_binding'
);
const canBeRequiredByUsersList = new Set(publicBuiltinIds);
const canBeRequiredByUsersWithoutSchemeList = new Set(
  publicBuiltinIds.filter((id) => !kSchemeOnlyBuiltinIds.has(id))
);

const BuiltinModule = {
  exists(id) {
    const value = String(id || '');
    const normalized = value.startsWith('node:') ? value.slice(5) : value;
    return allBuiltinSet.has(normalized);
  },

  normalizeRequirableId(id) {
    const value = String(id || '');
    if (value.startsWith('node:')) {
      const normalized = value.slice(5);
      if (canBeRequiredByUsersList.has(normalized)) return normalized;
      return undefined;
    }
    if (canBeRequiredByUsersWithoutSchemeList.has(value)) return value;
    return undefined;
  },

  isBuiltin(id) {
    return this.normalizeRequirableId(id) !== undefined;
  },

  getAllBuiltinModuleIds() {
    const ids = Array.from(canBeRequiredByUsersWithoutSchemeList);
    for (const id of kSchemeOnlyBuiltinIds) {
      ids.push(`node:${id}`);
    }
    ids.sort();
    return ids;
  },
};

function setupPrepareStackTrace() {
  try {
    const errorsBinding = internalBinding('errors');
    if (!errorsBinding || typeof errorsBinding !== 'object') return;

    const {
      setEnhanceStackForFatalException,
      setPrepareStackTraceCallback,
    } = errorsBinding;
    if (typeof setPrepareStackTraceCallback !== 'function' ||
        typeof setEnhanceStackForFatalException !== 'function') {
      return;
    }

    const {
      prepareStackTraceCallback,
      ErrorPrepareStackTrace,
      fatalExceptionStackEnhancers,
    } = require('internal/errors');

    if (typeof prepareStackTraceCallback === 'function') {
      setPrepareStackTraceCallback(prepareStackTraceCallback);
    }

    const beforeInspector =
      fatalExceptionStackEnhancers &&
      fatalExceptionStackEnhancers.beforeInspector;
    const afterInspector =
      fatalExceptionStackEnhancers &&
      fatalExceptionStackEnhancers.afterInspector;
    if (typeof beforeInspector === 'function' &&
        typeof afterInspector === 'function') {
      setEnhanceStackForFatalException(beforeInspector, afterInspector);
    }

    if (typeof ErrorPrepareStackTrace === 'function') {
      Object.defineProperty(Error, 'prepareStackTrace', {
        __proto__: null,
        writable: true,
        enumerable: false,
        configurable: true,
        value: ErrorPrepareStackTrace,
      });
    }
  } catch {
    // Keep bootstrap resilient in subprocess/minimal startup modes.
  }
}

setupPrepareStackTrace();

module.exports = { internalBinding, primordials, BuiltinModule };
