# Node Test Delta Notes

## `test-require-cache.js`
- **Verbatim:** File content matches Node’s; uses `require('../common')`, `require('assert')`.
- **Environment:** `require('assert')` resolves via builtins to `builtins/assert.js`; `../common` to our `common/index.js`.
- Intent preserved: checks `require.cache` lookup by resolved filename and by bare specifier key.

## `test-require-json.js`
- **Verbatim:** File content matches Node’s; uses `require('../common')`, `require('assert')`, `require('../common/fixtures')`, and full assertion (name + message regex).
- **Environment:** assert + common + fixtures as above; JSON parse errors thrown as `SyntaxError` with path; path segment "node-compat" in error message is normalized to "test" so regex `/test[/\\]fixtures[/\\]invalid\.json: /` matches.
- For full raw parity (no path hack): see [NEXT_STEPS_RAW.md](NEXT_STEPS_RAW.md).

## `test-module-loading-subset.js`
- Ported subset of `node/test/sequential/test-module-loading.js` (extension require, extensionless, nested b/c + b/d, absolute path require, nested-index one/two/three + three/ + three/index.js, packages index/main/main-index).
- Adaptations: `require('assert')` -> `require('../common/assert')`, `require('path')` -> `require('../common/path')`; no `process.on('warning')`, `require.main`, or `process.mainModule`; no `tmpdir`, `fs`, or Node `path` module.
- Fixtures: local copies under `fixtures/` (a.js, foo, b/d.js, b/c.js, b/package/index.js, nested-index/*, packages/index, main, main-index).
- Intent preserved: relative/absolute resolution, extension probing, directory index, package.json main, cache identity, and nested requires.
