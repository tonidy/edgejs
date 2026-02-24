#include <string>
#include <memory>

#include <gtest/gtest.h>
#include <libplatform/libplatform.h>
#include <v8.h>

#include "napi_v8_platform.h"

extern "C" napi_value Init_2_function_arguments(napi_env env, napi_value exports);
extern "C" napi_value Init_3_callbacks(napi_env env, napi_value exports);
extern "C" napi_value Init_4_object_factory(napi_env env, napi_value exports);
extern "C" napi_value Init_5_function_factory(napi_env env, napi_value exports);
extern "C" napi_value Init_7_factory_wrap(napi_env env, napi_value exports);
extern "C" napi_value Init_8_passing_wrapped(napi_env env, napi_value exports);

namespace {

struct CallbackState {
  bool saw_hello = false;
};

class V8Runtime {
 public:
  V8Runtime() {
    // Homebrew V8 can dereference null argv/data paths here.
    v8::V8::InitializeICUDefaultLocation("");
    v8::V8::InitializeExternalStartupData("");
    platform_ = v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(platform_.get());
    v8::V8::Initialize();

    params_.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    isolate_ = v8::Isolate::New(params_);
  }

  ~V8Runtime() {
    isolate_->Dispose();
    delete params_.array_buffer_allocator;
    v8::V8::Dispose();
    v8::V8::DisposePlatform();
  }

  v8::Isolate* isolate() const { return isolate_; }

 private:
  std::unique_ptr<v8::Platform> platform_;
  v8::Isolate::CreateParams params_{};
  v8::Isolate* isolate_ = nullptr;
};

class NapiV8Test : public ::testing::Test {
 protected:
  static void SetUpTestSuite() { runtime_ = std::make_unique<V8Runtime>(); }
  static void TearDownTestSuite() { runtime_.reset(); }
  static std::unique_ptr<V8Runtime> runtime_;
};

std::unique_ptr<V8Runtime> NapiV8Test::runtime_;

napi_value VerifyHello(napi_env env, napi_callback_info info) {
  void* data = nullptr;
  size_t argc = 1;
  napi_value args[1];
  EXPECT_EQ(napi_get_cb_info(env, info, &argc, args, nullptr, &data), napi_ok);
  auto* state = static_cast<CallbackState*>(data);
  EXPECT_EQ(argc, 1u);

  char buffer[64];
  size_t copied = 0;
  EXPECT_EQ(napi_get_value_string_utf8(env, args[0], buffer, sizeof(buffer), &copied), napi_ok);
  state->saw_hello = (std::string(buffer) == "hello world");
  return nullptr;
}

TEST_F(NapiV8Test, Ported2FunctionArguments) {
  v8::Isolate* isolate = runtime_->isolate();
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  napi_env env = nullptr;
  ASSERT_EQ(napi_v8_create_env(context, 8, &env), napi_ok);
  ASSERT_NE(env, nullptr);

  napi_value exports = nullptr;
  ASSERT_EQ(napi_create_object(env, &exports), napi_ok);
  ASSERT_NE(exports, nullptr);

  ASSERT_NE(Init_2_function_arguments(env, exports), nullptr);

  napi_value add_fn = nullptr;
  ASSERT_EQ(napi_get_named_property(env, exports, "add", &add_fn), napi_ok);
  ASSERT_NE(add_fn, nullptr);

  napi_value arg0 = nullptr;
  napi_value arg1 = nullptr;
  ASSERT_EQ(napi_create_double(env, 3, &arg0), napi_ok);
  ASSERT_EQ(napi_create_double(env, 5, &arg1), napi_ok);

  napi_value argv[2] = {arg0, arg1};
  napi_value out = nullptr;
  ASSERT_EQ(napi_call_function(env, exports, add_fn, 2, argv, &out), napi_ok);
  ASSERT_NE(out, nullptr);

  double value = 0;
  ASSERT_EQ(napi_get_value_double(env, out, &value), napi_ok);
  EXPECT_DOUBLE_EQ(value, 8);

  ASSERT_EQ(napi_v8_destroy_env(env), napi_ok);
}

TEST_F(NapiV8Test, Ported3Callbacks) {
  v8::Isolate* isolate = runtime_->isolate();
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  napi_env env = nullptr;
  ASSERT_EQ(napi_v8_create_env(context, 8, &env), napi_ok);
  ASSERT_NE(env, nullptr);

  napi_value exports = nullptr;
  ASSERT_EQ(napi_create_object(env, &exports), napi_ok);
  ASSERT_NE(exports, nullptr);
  ASSERT_NE(Init_3_callbacks(env, exports), nullptr);

  napi_value run_callback = nullptr;
  napi_value run_callback_with_recv = nullptr;
  ASSERT_EQ(napi_get_named_property(env, exports, "RunCallback", &run_callback), napi_ok);
  ASSERT_EQ(
      napi_get_named_property(env, exports, "RunCallbackWithRecv", &run_callback_with_recv),
      napi_ok);

  CallbackState state;
  napi_value hello_cb = nullptr;
  ASSERT_EQ(
      napi_create_function(env, "verifyHello", NAPI_AUTO_LENGTH, VerifyHello, &state, &hello_cb),
      napi_ok);

  napi_value cb_argv[1] = {hello_cb};
  ASSERT_EQ(napi_call_function(env, exports, run_callback, 1, cb_argv, nullptr), napi_ok);
  EXPECT_TRUE(state.saw_hello);

  v8::Local<v8::String> script_source = v8::String::NewFromUtf8Literal(
      isolate,
      "(function(){ 'use strict'; globalThis.__recvSeen = this; })");
  v8::Local<v8::Script> script =
      v8::Script::Compile(context, script_source).ToLocalChecked();
  v8::Local<v8::Value> callback_value = script->Run(context).ToLocalChecked();
  ASSERT_TRUE(callback_value->IsFunction());
  napi_value recv_cb = nullptr;
  ASSERT_EQ(napi_v8_wrap_existing_value(env, callback_value, &recv_cb), napi_ok);

  std::vector<v8::Local<v8::Value>> recv_values = {
      v8::Undefined(isolate),
      v8::Null(isolate),
      v8::Number::New(isolate, 5),
      v8::Boolean::New(isolate, true),
      v8::String::NewFromUtf8Literal(isolate, "Hello"),
      v8::Array::New(isolate),
      v8::Object::New(isolate)};

  for (auto recv_v8 : recv_values) {
    napi_value recv = nullptr;
    ASSERT_EQ(napi_v8_wrap_existing_value(env, recv_v8, &recv), napi_ok);
    napi_value args[2] = {recv_cb, recv};
    ASSERT_EQ(napi_call_function(env, exports, run_callback_with_recv, 2, args, nullptr), napi_ok);

    v8::Local<v8::Value> seen =
        context->Global()
            ->Get(context, v8::String::NewFromUtf8Literal(isolate, "__recvSeen"))
            .ToLocalChecked();
    EXPECT_TRUE(seen->StrictEquals(recv_v8));
  }

  ASSERT_EQ(napi_v8_destroy_env(env), napi_ok);
}

TEST_F(NapiV8Test, Ported4ObjectFactory) {
  v8::Isolate* isolate = runtime_->isolate();
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  napi_env env = nullptr;
  ASSERT_EQ(napi_v8_create_env(context, 8, &env), napi_ok);
  ASSERT_NE(env, nullptr);

  napi_value exports = nullptr;
  ASSERT_EQ(napi_create_object(env, &exports), napi_ok);
  napi_value addon = Init_4_object_factory(env, exports);
  ASSERT_NE(addon, nullptr);

  napi_value arg_hello = nullptr;
  napi_value arg_world = nullptr;
  ASSERT_EQ(napi_create_string_utf8(env, "hello", NAPI_AUTO_LENGTH, &arg_hello), napi_ok);
  ASSERT_EQ(napi_create_string_utf8(env, "world", NAPI_AUTO_LENGTH, &arg_world), napi_ok);

  napi_value obj1 = nullptr;
  napi_value obj2 = nullptr;
  napi_value argv1[1] = {arg_hello};
  napi_value argv2[1] = {arg_world};
  ASSERT_EQ(napi_call_function(env, exports, addon, 1, argv1, &obj1), napi_ok);
  ASSERT_EQ(napi_call_function(env, exports, addon, 1, argv2, &obj2), napi_ok);

  napi_value msg1 = nullptr;
  napi_value msg2 = nullptr;
  ASSERT_EQ(napi_get_named_property(env, obj1, "msg", &msg1), napi_ok);
  ASSERT_EQ(napi_get_named_property(env, obj2, "msg", &msg2), napi_ok);

  char buf1[16];
  char buf2[16];
  ASSERT_EQ(napi_get_value_string_utf8(env, msg1, buf1, sizeof(buf1), nullptr), napi_ok);
  ASSERT_EQ(napi_get_value_string_utf8(env, msg2, buf2, sizeof(buf2), nullptr), napi_ok);
  EXPECT_EQ(std::string(buf1), "hello");
  EXPECT_EQ(std::string(buf2), "world");

  ASSERT_EQ(napi_v8_destroy_env(env), napi_ok);
}

TEST_F(NapiV8Test, Ported5FunctionFactory) {
  v8::Isolate* isolate = runtime_->isolate();
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  napi_env env = nullptr;
  ASSERT_EQ(napi_v8_create_env(context, 8, &env), napi_ok);
  ASSERT_NE(env, nullptr);

  napi_value exports = nullptr;
  ASSERT_EQ(napi_create_object(env, &exports), napi_ok);
  napi_value addon = Init_5_function_factory(env, exports);
  ASSERT_NE(addon, nullptr);

  napi_value fn = nullptr;
  ASSERT_EQ(napi_call_function(env, exports, addon, 0, nullptr, &fn), napi_ok);
  ASSERT_NE(fn, nullptr);

  napi_value out = nullptr;
  ASSERT_EQ(napi_call_function(env, exports, fn, 0, nullptr, &out), napi_ok);
  ASSERT_NE(out, nullptr);

  char buf[32];
  ASSERT_EQ(napi_get_value_string_utf8(env, out, buf, sizeof(buf), nullptr), napi_ok);
  EXPECT_EQ(std::string(buf), "hello world");

  ASSERT_EQ(napi_v8_destroy_env(env), napi_ok);
}

TEST_F(NapiV8Test, Ported7FactoryWrap) {
  v8::Isolate* isolate = runtime_->isolate();
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  napi_env env = nullptr;
  ASSERT_EQ(napi_v8_create_env(context, 8, &env), napi_ok);
  ASSERT_NE(env, nullptr);

  napi_value exports = nullptr;
  ASSERT_EQ(napi_create_object(env, &exports), napi_ok);
  ASSERT_NE(Init_7_factory_wrap(env, exports), nullptr);

  napi_value create_object = nullptr;
  ASSERT_EQ(napi_get_named_property(env, exports, "createObject", &create_object), napi_ok);
  napi_value ten = nullptr;
  ASSERT_EQ(napi_create_uint32(env, 10, &ten), napi_ok);

  napi_value argv[1] = {ten};
  napi_value obj = nullptr;
  ASSERT_EQ(napi_call_function(env, exports, create_object, 1, argv, &obj), napi_ok);

  napi_value plus_one = nullptr;
  ASSERT_EQ(napi_get_named_property(env, obj, "plusOne", &plus_one), napi_ok);

  napi_value out = nullptr;
  ASSERT_EQ(napi_call_function(env, obj, plus_one, 0, nullptr, &out), napi_ok);
  uint32_t v = 0;
  ASSERT_EQ(napi_get_value_uint32(env, out, &v), napi_ok);
  EXPECT_EQ(v, 11u);

  ASSERT_EQ(napi_call_function(env, obj, plus_one, 0, nullptr, &out), napi_ok);
  ASSERT_EQ(napi_get_value_uint32(env, out, &v), napi_ok);
  EXPECT_EQ(v, 12u);

  ASSERT_EQ(napi_v8_destroy_env(env), napi_ok);
}

TEST_F(NapiV8Test, Ported8PassingWrapped) {
  v8::Isolate* isolate = runtime_->isolate();
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  napi_env env = nullptr;
  ASSERT_EQ(napi_v8_create_env(context, 8, &env), napi_ok);
  ASSERT_NE(env, nullptr);

  napi_value exports = nullptr;
  ASSERT_EQ(napi_create_object(env, &exports), napi_ok);
  ASSERT_NE(Init_8_passing_wrapped(env, exports), nullptr);

  napi_value create_object = nullptr;
  napi_value add = nullptr;
  ASSERT_EQ(napi_get_named_property(env, exports, "createObject", &create_object), napi_ok);
  ASSERT_EQ(napi_get_named_property(env, exports, "add", &add), napi_ok);

  napi_value ten = nullptr;
  napi_value twenty = nullptr;
  ASSERT_EQ(napi_create_double(env, 10, &ten), napi_ok);
  ASSERT_EQ(napi_create_double(env, 20, &twenty), napi_ok);

  napi_value obj1 = nullptr;
  napi_value obj2 = nullptr;
  napi_value a1[1] = {ten};
  napi_value a2[1] = {twenty};
  ASSERT_EQ(napi_call_function(env, exports, create_object, 1, a1, &obj1), napi_ok);
  ASSERT_EQ(napi_call_function(env, exports, create_object, 1, a2, &obj2), napi_ok);

  napi_value add_args[2] = {obj1, obj2};
  napi_value sum = nullptr;
  ASSERT_EQ(napi_call_function(env, exports, add, 2, add_args, &sum), napi_ok);
  double dv = 0;
  ASSERT_EQ(napi_get_value_double(env, sum, &dv), napi_ok);
  EXPECT_DOUBLE_EQ(dv, 30);

  ASSERT_EQ(napi_v8_destroy_env(env), napi_ok);
}

}  // namespace
