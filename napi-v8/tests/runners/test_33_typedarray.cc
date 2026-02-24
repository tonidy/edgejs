#include <string>

#include "test_env.h"

extern "C" napi_value Init(napi_env env, napi_value exports);

class Test33TypedArray : public FixtureTestBase {};

TEST_F(Test33TypedArray, PortedCoreFlow) {
  EnvScope s(runtime_.get());
  napi_value exports = nullptr;
  ASSERT_EQ(napi_create_object(s.env, &exports), napi_ok);
  ASSERT_NE(Init(s.env, exports), nullptr);

  napi_value global = nullptr;
  ASSERT_EQ(napi_get_global(s.env, &global), napi_ok);
  ASSERT_EQ(napi_set_named_property(s.env, global, "__tta", exports), napi_ok);

  auto run_js = [&](const char* source_text) {
    v8::TryCatch tc(s.isolate);
    std::string wrapped = std::string("(() => { 'use strict'; ") + source_text + " })();";
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
{
  const byteArray = new Uint8Array(3);
  byteArray[0] = 0;
  byteArray[1] = 1;
  byteArray[2] = 2;
  const byteResult = __tta.Multiply(byteArray, 3);
  if (!(byteResult instanceof Uint8Array)) throw new Error('mulU8Type');
  if (byteResult.length !== 3) throw new Error('mulU8Len');
  if (byteResult[0] !== 0 || byteResult[1] !== 3 || byteResult[2] !== 6) throw new Error('mulU8Data');
}
{
  const doubleArray = new Float64Array(3);
  doubleArray[0] = 0.0;
  doubleArray[1] = 1.1;
  doubleArray[2] = 2.2;
  const doubleResult = __tta.Multiply(doubleArray, -3);
  if (!(doubleResult instanceof Float64Array)) throw new Error('mulF64Type');
  if (doubleResult.length !== 3) throw new Error('mulF64Len');
  if (doubleResult[0] !== -0) throw new Error('mulF640');
  if (Math.round(10 * doubleResult[1]) / 10 !== -3.3) throw new Error('mulF641');
  if (Math.round(10 * doubleResult[2]) / 10 !== -6.6) throw new Error('mulF642');
}
{
  const externalResult = __tta.External();
  if (!(externalResult instanceof Int8Array)) throw new Error('extType');
  if (externalResult.length !== 3) throw new Error('extLen');
  if (externalResult[0] !== 0 || externalResult[1] !== 1 || externalResult[2] !== 2) throw new Error('extData');
}
{
  const buffer = new ArrayBuffer(128);
  const arrayTypes = [Int8Array, Uint8Array, Uint8ClampedArray, Int16Array,
    Uint16Array, Int32Array, Uint32Array, Float16Array, Float32Array, Float64Array,
    BigInt64Array, BigUint64Array];
  for (const currentType of arrayTypes) {
    const template = Reflect.construct(currentType, [buffer]);
    const theArray = __tta.CreateTypedArray(template, buffer);
    if (!(theArray instanceof currentType)) throw new Error('createType');
    if (theArray === template) throw new Error('createDistinct');
    if (theArray.buffer !== buffer) throw new Error('createBuffer');
  }
}
{
  const buffer = new ArrayBuffer(128);
  const arrayTypes = [Int8Array, Uint8Array, Uint8ClampedArray, Int16Array,
    Uint16Array, Int32Array, Uint32Array, Float16Array, Float32Array, Float64Array,
    BigInt64Array, BigUint64Array];
  for (const currentType of arrayTypes) {
    const template = Reflect.construct(currentType, [buffer]);
    let ok = false;
    try { __tta.CreateTypedArray(template, buffer, 0, 136); } catch (e) { ok = e instanceof RangeError; }
    if (!ok) throw new Error('rangeLen');
  }
}
{
  const buffer = new ArrayBuffer(128);
  const nonByteArrayTypes = [Int16Array, Uint16Array, Int32Array, Uint32Array,
    Float16Array, Float32Array, Float64Array, BigInt64Array, BigUint64Array];
  for (const currentType of nonByteArrayTypes) {
    const template = Reflect.construct(currentType, [buffer]);
    let ok = false;
    try { __tta.CreateTypedArray(template, buffer, currentType.BYTES_PER_ELEMENT + 1, 1); }
    catch (e) { ok = e instanceof RangeError; }
    if (!ok) throw new Error('rangeAlign');
  }
}
{
  const arrayTypes = [Int8Array, Uint8Array, Uint8ClampedArray, Int16Array,
    Uint16Array, Int32Array, Uint32Array, Float16Array, Float32Array, Float64Array,
    BigInt64Array, BigUint64Array];
  for (const currentType of arrayTypes) {
    const buffer = Reflect.construct(currentType, [8]);
    if (buffer.length !== 8) throw new Error('detachLenInit');
    if (__tta.IsDetached(buffer.buffer)) throw new Error('detachInitiallyFalse');
    __tta.Detach(buffer);
    if (!__tta.IsDetached(buffer.buffer)) throw new Error('detachTrue');
    if (buffer.length !== 0) throw new Error('detachLenZero');
  }
}
{
  const buffer = __tta.External();
  if (!(buffer instanceof Int8Array)) throw new Error('detachExtType');
  if (buffer.length !== 3) throw new Error('detachExtLen');
  if (buffer.byteLength !== 3) throw new Error('detachExtByteLen');
  if (__tta.IsDetached(buffer.buffer)) throw new Error('detachExtInitiallyFalse');
  __tta.Detach(buffer);
  if (!__tta.IsDetached(buffer.buffer)) throw new Error('detachExtTrue');
  if (buffer.length !== 0) throw new Error('detachExtLenZero');
  if (buffer.byteLength !== 0) throw new Error('detachExtByteLenZero');
}
{
  const buffer = new ArrayBuffer(128);
  if (__tta.IsDetached(buffer)) throw new Error('isDetachedFalse');
}
{
  const buffer = __tta.NullArrayBuffer();
  if (!(buffer instanceof ArrayBuffer)) throw new Error('nullAbType');
  if (!__tta.IsDetached(buffer)) throw new Error('nullAbDetached');
}
)JS"));
}
