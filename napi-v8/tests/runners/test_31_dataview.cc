#include <string>

#include "test_env.h"

extern "C" napi_value Init(napi_env env, napi_value exports);

class Test31DataView : public FixtureTestBase {};

TEST_F(Test31DataView, PortedCoreFlow) {
  EnvScope s(runtime_.get());
  napi_value exports = nullptr;
  ASSERT_EQ(napi_create_object(s.env, &exports), napi_ok);
  ASSERT_NE(Init(s.env, exports), nullptr);

  napi_value global = nullptr;
  ASSERT_EQ(napi_get_global(s.env, &global), napi_ok);
  ASSERT_EQ(napi_set_named_property(s.env, global, "__tdv", exports), napi_ok);

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
  const buffer = new ArrayBuffer(128);
  const template = Reflect.construct(DataView, [buffer]);
  const theDataview = __tdv.CreateDataViewFromJSDataView(template);
  if (!(theDataview instanceof DataView)) throw new Error('abCreate');
}
{
  const buffer = new SharedArrayBuffer(128);
  const template = new DataView(buffer);
  const theDataview = __tdv.CreateDataViewFromJSDataView(template);
  if (!(theDataview instanceof DataView)) throw new Error('sabCreate');
  if (template.buffer !== theDataview.buffer) throw new Error('sameBuffer');
}
{
  const buffer = new ArrayBuffer(128);
  let ok = false;
  try { __tdv.CreateDataView(buffer, 10, 200); } catch (e) { ok = e instanceof RangeError; }
  if (!ok) throw new Error('abRange');
}
{
  const buffer = new SharedArrayBuffer(128);
  let ok = false;
  try { __tdv.CreateDataView(buffer, 10, 200); } catch (e) { ok = e instanceof RangeError; }
  if (!ok) throw new Error('sabRange');
}
)JS"));
}
