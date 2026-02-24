#include <string>

#include "test_env.h"

extern "C" napi_value Init(napi_env env, napi_value exports);

class Test32SharedArrayBuffer : public FixtureTestBase {};

TEST_F(Test32SharedArrayBuffer, PortedCoreFlow) {
  EnvScope s(runtime_.get());
  napi_value exports = nullptr;
  ASSERT_EQ(napi_create_object(s.env, &exports), napi_ok);
  ASSERT_NE(Init(s.env, exports), nullptr);

  napi_value global = nullptr;
  ASSERT_EQ(napi_get_global(s.env, &global), napi_ok);
  ASSERT_EQ(napi_set_named_property(s.env, global, "__tsab", exports), napi_ok);

  auto run_js = [&](const char* src) {
    v8::TryCatch tc(s.isolate);
    std::string wrapped = std::string("(() => { 'use strict'; ") + src + " })();";
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
                      << " while running: " << src;
      }
      return false;
    }
    return true;
  };

  ASSERT_TRUE(run_js(R"JS(
    (() => {
      const sab = new SharedArrayBuffer(16);
      const ab = new ArrayBuffer(16);
      const obj = {};
      const arr = [];
      if (__tsab.TestIsSharedArrayBuffer(sab) !== true) throw new Error("isSab1");
      if (__tsab.TestIsSharedArrayBuffer(ab) !== false) throw new Error("isSab2");
      if (__tsab.TestIsSharedArrayBuffer(obj) !== false) throw new Error("isSab3");
      if (__tsab.TestIsSharedArrayBuffer(arr) !== false) throw new Error("isSab4");
      if (__tsab.TestIsSharedArrayBuffer(null) !== false) throw new Error("isSab5");
      if (__tsab.TestIsSharedArrayBuffer(undefined) !== false) throw new Error("isSab6");
    })();
  )JS"));

  ASSERT_TRUE(run_js(R"JS(
    (() => {
      const sab = __tsab.TestCreateSharedArrayBuffer(16);
      if (!(sab instanceof SharedArrayBuffer)) throw new Error("createSabType");
      if (sab.byteLength !== 16) throw new Error("createSabLength");
    })();
  )JS"));

  ASSERT_TRUE(run_js(R"JS(
    (() => {
      const sab = new SharedArrayBuffer(32);
      const byteLength = __tsab.TestGetSharedArrayBufferInfo(sab);
      if (byteLength !== 32) throw new Error("getSabInfo");
    })();
  )JS"));

  ASSERT_TRUE(run_js(R"JS(
    (() => {
      const sab = new SharedArrayBuffer(8);
      const result = __tsab.TestSharedArrayBufferData(sab);
      if (result !== true) throw new Error("dataWriteResult");
      const view = new Uint8Array(sab);
      for (let i = 0; i < 8; i++) {
        if (view[i] !== (i % 256)) throw new Error("dataWritePattern");
      }
    })();
  )JS"));

  ASSERT_TRUE(run_js(R"JS(
    (() => {
      const sab = new SharedArrayBuffer(16);
      const result = __tsab.TestSharedArrayBufferData(sab);
      if (result !== true) throw new Error("dataExisting");
    })();
  )JS"));

  ASSERT_TRUE(run_js(R"JS(
    (() => {
      const sab = __tsab.TestCreateSharedArrayBuffer(0);
      if (!(sab instanceof SharedArrayBuffer)) throw new Error("zeroSabType");
      if (sab.byteLength !== 0) throw new Error("zeroSabLength");
    })();
  )JS"));

  ASSERT_TRUE(run_js(R"JS(
    (() => {
      let threw = false;
      try {
        __tsab.TestGetSharedArrayBufferInfo({});
      } catch (e) {
        threw = (e && e.message === "Invalid argument");
      }
      if (!threw) throw new Error("invalidArgs");
    })();
  )JS"));
}
