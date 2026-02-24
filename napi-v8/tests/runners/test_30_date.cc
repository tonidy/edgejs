#include <string>

#include "test_env.h"

extern "C" napi_value Init(napi_env env, napi_value exports);

class Test30Date : public FixtureTestBase {};

TEST_F(Test30Date, PortedCoreFlow) {
  EnvScope s(runtime_.get());
  napi_value exports = nullptr;
  ASSERT_EQ(napi_create_object(s.env, &exports), napi_ok);
  ASSERT_NE(Init(s.env, exports), nullptr);

  napi_value global = nullptr;
  ASSERT_EQ(napi_get_global(s.env, &global), napi_ok);
  ASSERT_EQ(napi_set_named_property(s.env, global, "__td", exports), napi_ok);

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
const dateTypeTestDate = __td.createDate(1549183351);
if (__td.isDate(dateTypeTestDate) !== true) throw new Error('isDate1');
if (__td.isDate(new Date(1549183351)) !== true) throw new Error('isDate2');
if (__td.isDate(2.4) !== false) throw new Error('isDate3');
if (__td.isDate('not a date') !== false) throw new Error('isDate4');
if (__td.isDate(undefined) !== false) throw new Error('isDate5');
if (__td.isDate(null) !== false) throw new Error('isDate6');
if (__td.isDate({}) !== false) throw new Error('isDate7');
if (__td.getDateValue(new Date(1549183351)) !== 1549183351) throw new Error('getDateValue');
)JS"));
}
