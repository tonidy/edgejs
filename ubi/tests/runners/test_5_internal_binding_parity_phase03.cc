#include <string>

#include "test_env.h"
#include "ubi_runtime.h"

class Test5InternalBindingParityPhase03 : public FixtureTestBase {};

namespace {

constexpr const char* kParityWaveScript = R"JS(
const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');

const constants = internalBinding('constants');
assert.ok(constants && typeof constants === 'object');
assert.ok(constants.os && typeof constants.os === 'object');
assert.ok(constants.fs && typeof constants.fs === 'object');
assert.ok(constants.crypto && typeof constants.crypto === 'object');
assert.ok(constants.zlib && typeof constants.zlib === 'object');
assert.ok(constants.internal && typeof constants.internal === 'object');
assert.strictEqual(constants.internal.EXTENSIONLESS_FORMAT_JAVASCRIPT, 0);
assert.strictEqual(constants.internal.EXTENSIONLESS_FORMAT_WASM, 1);

const config = internalBinding('config');
assert.ok(config && typeof config === 'object');
assert.strictEqual(typeof config.getDefaultLocale, 'function');
assert.strictEqual(config.hasOpenSSL, !!(process.versions && process.versions.openssl));
const locale = config.getDefaultLocale();
assert.ok(locale === undefined || typeof locale === 'string');

const utilBinding = internalBinding('util');
assert.ok(utilBinding && typeof utilBinding === 'object');
assert.strictEqual(utilBinding.constants.kPending, 0);
assert.strictEqual(utilBinding.constants.kFulfilled, 1);
assert.strictEqual(utilBinding.constants.kRejected, 2);
assert.strictEqual(typeof utilBinding.getCallerLocation, 'function');
const loc = utilBinding.getCallerLocation();
assert.ok(Array.isArray(loc) && loc.length === 3);
assert.strictEqual(typeof loc[0], 'number');
assert.strictEqual(typeof loc[1], 'number');
assert.strictEqual(typeof loc[2], 'string');
assert.strictEqual(typeof utilBinding.privateSymbols.arrow_message_private_symbol, 'symbol');
assert.strictEqual(typeof utilBinding.privateSymbols.decorated_private_symbol, 'symbol');
const callSites = utilBinding.getCallSites(3);
assert.ok(Array.isArray(callSites));
assert.ok(callSites.length >= 1 && callSites.length <= 3);
assert.strictEqual(typeof callSites[0].scriptId, 'string');
assert.ok(!String(callSites[0].scriptName).includes('node:util'));

const proxyTarget = { a: 1 };
const proxyHandler = {
  get(target, prop) {
    return target[prop];
  },
};
const proxy = new Proxy(proxyTarget, proxyHandler);
const proxyDetails = utilBinding.getProxyDetails(proxy, true);
assert.strictEqual(proxyDetails[0], proxyTarget);
assert.strictEqual(proxyDetails[1], proxyHandler);
assert.strictEqual(utilBinding.getProxyDetails(proxy, false), proxyTarget);

const revocable = Proxy.revocable({}, {});
revocable.revoke();
const revokedDetails = utilBinding.getProxyDetails(revocable.proxy, true);
assert.strictEqual(revokedDetails[0], null);
assert.strictEqual(revokedDetails[1], null);
assert.strictEqual(utilBinding.getProxyDetails(revocable.proxy, false), null);

const errorsBinding = internalBinding('errors');
errorsBinding.setSourceMapsEnabled(true);
errorsBinding.setGetSourceMapErrorSource((file, line, column) =>
  `mapped:${file}:${line}:${column}`);
const syntheticError = { stack: 'Error: boom\n    at fn (/tmp/example.js:7:9)' };
const sourcePos = errorsBinding.getErrorSourcePositions(syntheticError);
assert.strictEqual(sourcePos.sourceLine, 'mapped:/tmp/example.js:7:9');
errorsBinding.setSourceMapsEnabled(false);

const traceEvents = internalBinding('trace_events');
assert.ok(traceEvents && typeof traceEvents === 'object');
assert.strictEqual(typeof traceEvents.getEnabledCategories, 'function');
assert.strictEqual(typeof traceEvents.getCategoryEnabledBuffer, 'function');
assert.strictEqual(typeof traceEvents.isTraceCategoryEnabled, 'function');
assert.strictEqual(typeof traceEvents.setTraceCategoryStateUpdateHandler, 'function');
assert.strictEqual(typeof traceEvents.CategorySet, 'function');
const nodeHttpBuffer = traceEvents.getCategoryEnabledBuffer('node.http');
assert.ok(nodeHttpBuffer instanceof Uint8Array);
assert.strictEqual(nodeHttpBuffer.length, 1);
assert.strictEqual(traceEvents.getCategoryEnabledBuffer('node.http'), nodeHttpBuffer);
assert.strictEqual(nodeHttpBuffer[0], 0);
let traceStateCalls = 0;
traceEvents.setTraceCategoryStateUpdateHandler((enabled) => {
  traceStateCalls++;
  assert.strictEqual(typeof enabled, 'boolean');
});
const categorySet = new traceEvents.CategorySet(['node.http', 'node.async_hooks']);
categorySet.enable();
assert.strictEqual(traceEvents.isTraceCategoryEnabled('node.http'), true);
assert.strictEqual(nodeHttpBuffer[0], 1);
assert.ok(traceEvents.getEnabledCategories().includes('node.http'));
const asyncHooksBuffer = traceEvents.getCategoryEnabledBuffer('node.async_hooks');
assert.strictEqual(asyncHooksBuffer[0], 1);
categorySet.disable();
assert.strictEqual(traceEvents.isTraceCategoryEnabled('node.http'), false);
assert.strictEqual(nodeHttpBuffer[0], 0);
assert.ok(traceStateCalls >= 2);

const uv = internalBinding('uv');
assert.strictEqual(typeof uv.UV_UNKNOWN, 'number');
assert.strictEqual(typeof uv.UV_EAI_MEMORY, 'number');
const uvMap = uv.getErrorMap();
assert.ok(uvMap instanceof Map);
assert.ok(uvMap.has(uv.UV_EINVAL));
assert.strictEqual(typeof uv.getErrorMessage(uv.UV_EINVAL), 'string');

const fsBinding = internalBinding('fs');
assert.strictEqual(typeof fsBinding.getFormatOfExtensionlessFile, 'function');
const tmpRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'ubi-fmt-'));
const wasmPath = path.join(tmpRoot, 'mod');
const jsPath = path.join(tmpRoot, 'main');
const wasmFd = fsBinding.open(
  wasmPath,
  fsBinding.O_WRONLY | fsBinding.O_CREAT | fsBinding.O_TRUNC,
  0o666,
);
fsBinding.writeBuffer(wasmFd, new Uint8Array([0x00, 0x61, 0x73, 0x6d]), 0, 4, 0);
fsBinding.close(wasmFd);
fs.writeFileSync(jsPath, 'console.log(1);');
assert.strictEqual(
  fsBinding.getFormatOfExtensionlessFile(wasmPath),
  constants.internal.EXTENSIONLESS_FORMAT_WASM,
);
assert.strictEqual(
  fsBinding.getFormatOfExtensionlessFile(jsPath),
  constants.internal.EXTENSIONLESS_FORMAT_JAVASCRIPT,
);
fs.rmSync(tmpRoot, { recursive: true, force: true });

const processMethods = internalBinding('process_methods');
assert.strictEqual(typeof processMethods.patchProcessObject, 'function');
assert.strictEqual(typeof processMethods.loadEnvFile, 'function');
const patchedProcess = {};
processMethods.patchProcessObject(patchedProcess);
assert.ok(Array.isArray(patchedProcess.argv));
assert.ok(Array.isArray(patchedProcess.execArgv));
assert.strictEqual(typeof patchedProcess.pid, 'number');
assert.strictEqual(typeof patchedProcess.execPath, 'string');

const envRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'ubi-env-'));
const envPath = path.join(envRoot, '.env');
const envVar = 'UBI_INTERNAL_BINDING_PARITY_ENV';
delete process.env[envVar];
fs.writeFileSync(envPath, `${envVar}=fresh\n`);
processMethods.loadEnvFile(envPath);
assert.strictEqual(process.env[envVar], 'fresh');
fs.writeFileSync(envPath, `${envVar}=override\n`);
processMethods.loadEnvFile(envPath);
assert.strictEqual(process.env[envVar], 'fresh');
assert.throws(
  () => processMethods.loadEnvFile(path.join(envRoot, 'missing.env')),
  (err) => err && err.code === 'ENOENT',
);
fs.rmSync(envRoot, { recursive: true, force: true });
delete process.env[envVar];

assert.throws(
  () => processMethods._debugProcess(0x7fffffff),
  (err) => err && typeof err.code === 'string',
);

const builtinsBinding = internalBinding('builtins');
assert.strictEqual(typeof builtinsBinding.compileFunction, 'function');
assert.strictEqual(typeof builtinsBinding.setInternalLoaders, 'function');
const compiledBuiltin = builtinsBinding.compileFunction('internal/test/binding');
assert.strictEqual(typeof compiledBuiltin, 'function');
builtinsBinding.setInternalLoaders(
  (name) => internalBinding(name),
  (name) => require(name),
);

const udpWrap = internalBinding('udp_wrap');
const udpHandle = new udpWrap.UDP();
const recvBufferSize = udpHandle.getRecvBufferSize();
const sendBufferSize = udpHandle.getSendBufferSize();
assert.ok(recvBufferSize === undefined || typeof recvBufferSize === 'number');
assert.ok(sendBufferSize === undefined || typeof sendBufferSize === 'number');
const recvSetResult = udpHandle.setRecvBufferSize(
  typeof recvBufferSize === 'number' ? recvBufferSize : 0,
);
const sendSetResult = udpHandle.setSendBufferSize(
  typeof sendBufferSize === 'number' ? sendBufferSize : 0,
);
assert.ok(recvSetResult === undefined || typeof recvSetResult === 'number');
assert.ok(sendSetResult === undefined || typeof sendSetResult === 'number');
assert.strictEqual(typeof udpHandle.setMulticastAll(true), 'number');
udpHandle.close(() => {});

globalThis.__ubi_internal_binding_parity_ok = 1;
)JS";

}  // namespace

TEST_F(Test5InternalBindingParityPhase03, WaveOneAndTwoBindingsHaveCriticalParitySurface) {
  EnvScope s(runtime_.get());

  std::string error;
  const int exit_code = UbiRunScriptSource(s.env, kParityWaveScript, &error);
  EXPECT_EQ(exit_code, 0) << "error=" << error;
  EXPECT_TRUE(error.empty());

  napi_value global = nullptr;
  ASSERT_EQ(napi_get_global(s.env, &global), napi_ok);

  napi_value ok_value = nullptr;
  ASSERT_EQ(napi_get_named_property(s.env, global, "__ubi_internal_binding_parity_ok", &ok_value), napi_ok);

  int32_t ok = 0;
  ASSERT_EQ(napi_get_value_int32(s.env, ok_value, &ok), napi_ok);
  EXPECT_EQ(ok, 1);
}
