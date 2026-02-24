#include <string>

#include "test_env.h"

extern "C" napi_value Init(napi_env env, napi_value exports);

class Test18Conversions : public FixtureTestBase {};

TEST_F(Test18Conversions, PortedCoreFlow) {
  EnvScope s(runtime_.get());
  napi_value exports = nullptr;
  ASSERT_EQ(napi_create_object(s.env, &exports), napi_ok);
  ASSERT_NE(Init(s.env, exports), nullptr);

  napi_value global = nullptr;
  ASSERT_EQ(napi_get_global(s.env, &global), napi_ok);
  ASSERT_EQ(napi_set_named_property(s.env, global, "__conv", exports), napi_ok);

  auto run_js = [&](const char* source_text) {
    v8::TryCatch tc(s.isolate);
    std::string wrapped = std::string("(() => { ") + source_text + " })();";
    v8::Local<v8::String> source =
        v8::String::NewFromUtf8(s.isolate, wrapped.c_str(), v8::NewStringType::kNormal)
            .ToLocalChecked();
    v8::Local<v8::Script> script;
    if (!v8::Script::Compile(s.context, source).ToLocal(&script)) return false;
    v8::Local<v8::Value> out;
    if (!script->Run(s.context).ToLocal(&out)) {
      if (tc.HasCaught()) {
        v8::String::Utf8Value msg(s.isolate, tc.Exception());
        ADD_FAILURE() << "JS exception: " << (*msg ? *msg : "<empty>")
                      << " while running: " << source_text;
      }
      return false;
    }
    return true;
  };

  ASSERT_TRUE(run_js(R"JS(
const boolExpected = /boolean was expected/i;
const numberExpected = /number was expected/i;
const stringExpected = /string was expected/i;
const testSym = Symbol('test');
function expectThrow(fn, rx, tag) {
  let ok = false;
  try { fn(); } catch (e) { ok = rx.test(String(e && e.message)); }
  if (!ok) throw new Error(tag);
}
if (__conv.asBool(false) !== false) throw new Error('asBoolFalse');
if (__conv.asBool(true) !== true) throw new Error('asBoolTrue');
expectThrow(() => __conv.asBool(undefined), boolExpected, 'asBoolUndef');
expectThrow(() => __conv.asBool(0), boolExpected, 'asBoolZero');
expectThrow(() => __conv.asBool(testSym), boolExpected, 'asBoolSym');

[__conv.asInt32, __conv.asUInt32, __conv.asInt64].forEach((asInt) => {
  if (asInt(0) !== 0) throw new Error('asInt0');
  if (asInt(1.9) !== 1) throw new Error('asIntTrunc');
  if (asInt(Number.NaN) !== 0) throw new Error('asIntNaN');
  expectThrow(() => asInt(undefined), numberExpected, 'asIntUndef');
  expectThrow(() => asInt(testSym), numberExpected, 'asIntSym');
});
if (__conv.asInt32(-1) !== -1) throw new Error('asInt32Neg');
if (__conv.asInt64(-1) !== -1) throw new Error('asInt64Neg');
if (__conv.asUInt32(-1) !== Math.pow(2, 32) - 1) throw new Error('asUInt32Neg');

if (__conv.asDouble(1.9) !== 1.9) throw new Error('asDouble');
if (!Number.isNaN(__conv.asDouble(Number.NaN))) throw new Error('asDoubleNaN');
expectThrow(() => __conv.asDouble(undefined), numberExpected, 'asDoubleUndef');

if (__conv.asString('test') !== 'test') throw new Error('asString');
expectThrow(() => __conv.asString(1), stringExpected, 'asStringNum');

if (__conv.toBool('') !== false) throw new Error('toBoolEmpty');
if (__conv.toBool('true') !== true) throw new Error('toBoolTrue');
if (__conv.toNumber('1.1') !== 1.1) throw new Error('toNumber');
if (!Number.isNaN(__conv.toNumber({}))) throw new Error('toNumberObj');
{
  let ok = false;
  try { __conv.toNumber(testSym); } catch (e) { ok = e instanceof TypeError; }
  if (!ok) throw new Error('toNumberSym');
}

if (__conv.toString(undefined) !== 'undefined') throw new Error('toStringUndefined');
if (__conv.toString([1,2,3]) !== '1,2,3') throw new Error('toStringArray');
{
  let ok = false;
  try { __conv.toString(testSym); } catch (e) { ok = e instanceof TypeError; }
  if (!ok) throw new Error('toStringSym');
}
)JS"));

  ASSERT_TRUE(run_js(R"JS(
function eq(a, b) { return JSON.stringify(a) === JSON.stringify(b); }
if (!eq(__conv.testNull.getValueBool(), {
  envIsNull: 'Invalid argument',
  valueIsNull: 'Invalid argument',
  resultIsNull: 'Invalid argument',
  inputTypeCheck: 'A boolean was expected',
})) throw new Error('nullBool');

if (!eq(__conv.testNull.getValueInt32(), {
  envIsNull: 'Invalid argument',
  valueIsNull: 'Invalid argument',
  resultIsNull: 'Invalid argument',
  inputTypeCheck: 'A number was expected',
})) throw new Error('nullInt32');

if (!eq(__conv.testNull.getValueUint32(), {
  envIsNull: 'Invalid argument',
  valueIsNull: 'Invalid argument',
  resultIsNull: 'Invalid argument',
  inputTypeCheck: 'A number was expected',
})) throw new Error('nullUint32');

if (!eq(__conv.testNull.getValueInt64(), {
  envIsNull: 'Invalid argument',
  valueIsNull: 'Invalid argument',
  resultIsNull: 'Invalid argument',
  inputTypeCheck: 'A number was expected',
})) throw new Error('nullInt64');

if (!eq(__conv.testNull.getValueDouble(), {
  envIsNull: 'Invalid argument',
  valueIsNull: 'Invalid argument',
  resultIsNull: 'Invalid argument',
  inputTypeCheck: 'A number was expected',
})) throw new Error('nullDouble');

if (!eq(__conv.testNull.coerceToBool(), {
  envIsNull: 'Invalid argument',
  valueIsNull: 'Invalid argument',
  resultIsNull: 'Invalid argument',
  inputTypeCheck: 'napi_ok',
})) throw new Error('nullCoerceBool');

if (!eq(__conv.testNull.coerceToObject(), {
  envIsNull: 'Invalid argument',
  valueIsNull: 'Invalid argument',
  resultIsNull: 'Invalid argument',
  inputTypeCheck: 'napi_ok',
})) throw new Error('nullCoerceObject');

if (!eq(__conv.testNull.coerceToString(), {
  envIsNull: 'Invalid argument',
  valueIsNull: 'Invalid argument',
  resultIsNull: 'Invalid argument',
  inputTypeCheck: 'napi_ok',
})) throw new Error('nullCoerceString');

if (!eq(__conv.testNull.getValueStringUtf8(), {
  envIsNull: 'Invalid argument',
  valueIsNull: 'Invalid argument',
  wrongTypeIn: 'A string was expected',
  bufAndOutLengthIsNull: 'Invalid argument',
})) throw new Error('nullUtf8');

if (!eq(__conv.testNull.getValueStringLatin1(), {
  envIsNull: 'Invalid argument',
  valueIsNull: 'Invalid argument',
  wrongTypeIn: 'A string was expected',
  bufAndOutLengthIsNull: 'Invalid argument',
})) throw new Error('nullLatin1');

if (!eq(__conv.testNull.getValueStringUtf16(), {
  envIsNull: 'Invalid argument',
  valueIsNull: 'Invalid argument',
  wrongTypeIn: 'A string was expected',
  bufAndOutLengthIsNull: 'Invalid argument',
})) throw new Error('nullUtf16');
)JS"));
}
