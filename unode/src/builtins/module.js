'use strict';

const { BuiltinModule } = require('internal/bootstrap/realm');

// Minimal shim for require('module'). _initPaths() is called by tests like
// test-require-dot.js; when NODE_PATH is used we rely on the loader reading
// process.env.NODE_PATH (via setenv sync) and getenv in the loader.
function _initPaths() {
  // No-op: loader reads NODE_PATH from environment when present.
}

const builtinModules = Object.freeze(BuiltinModule.getAllBuiltinModuleIds());

module.exports = {
  _initPaths,
  builtinModules,
  isBuiltin(id) {
    return BuiltinModule.isBuiltin(id);
  },
};
