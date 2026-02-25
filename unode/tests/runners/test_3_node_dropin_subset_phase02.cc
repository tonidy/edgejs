#include <cstdlib>
#include <string>

#include "test_env.h"
#include "unode_runtime.h"

class Test3NodeDropinSubsetPhase02 : public FixtureTestBase {};

namespace {

int RunNodeCompatScript(napi_env env, const char* relative_path, std::string* error_out) {
  const std::string script_path = std::string(NAPI_V8_ROOT_PATH) + "/tests/node-compat/" + relative_path;
  return UnodeRunScriptFile(env, script_path.c_str(), error_out);
}

// Run a Node test script from the node repo (raw drop-in). Uses UNODE_FALLBACK_BUILTINS_DIR
// so require('assert'), require('path'), etc. resolve to unode/tests/node-compat/builtins.
// NODE_TEST_DIR points to node/test so common/fixtures.js can resolve fixtures under node/test/fixtures.
int RunRawNodeTestScript(napi_env env, const char* node_test_relative_path, std::string* error_out) {
#ifdef NAPI_V8_NODE_ROOT_PATH
  const std::string node_root(NAPI_V8_NODE_ROOT_PATH);
  const std::string unode_root(NAPI_V8_ROOT_PATH);
  const std::string script_path = node_root + "/test/parallel/" + node_test_relative_path;
  const std::string fallback_builtins = unode_root + "/tests/node-compat/builtins";
  const std::string node_test_dir = node_root + "/test";
  setenv("UNODE_FALLBACK_BUILTINS_DIR", fallback_builtins.c_str(), 1);
  setenv("NODE_TEST_DIR", node_test_dir.c_str(), 1);
  const int exit_code = UnodeRunScriptFile(env, script_path.c_str(), error_out);
  unsetenv("UNODE_FALLBACK_BUILTINS_DIR");
  unsetenv("NODE_TEST_DIR");
  return exit_code;
#else
  (void)env;
  (void)node_test_relative_path;
  (void)error_out;
  return -1;
#endif
}

}  // namespace

TEST_F(Test3NodeDropinSubsetPhase02, RequireCacheSubsetTest) {
  EnvScope s(runtime_.get());
  std::string error;
  const int exit_code = RunNodeCompatScript(s.env, "parallel/test-require-cache.js", &error);
  EXPECT_EQ(exit_code, 0) << "error=" << error;
  EXPECT_TRUE(error.empty()) << "error=" << error;
}

TEST_F(Test3NodeDropinSubsetPhase02, RequireJsonSubsetTest) {
  EnvScope s(runtime_.get());
  std::string error;
  const int exit_code = RunNodeCompatScript(s.env, "parallel/test-require-json.js", &error);
  EXPECT_EQ(exit_code, 0) << "error=" << error;
  EXPECT_TRUE(error.empty()) << "error=" << error;
}

TEST_F(Test3NodeDropinSubsetPhase02, ModuleLoadingSubsetTest) {
  EnvScope s(runtime_.get());
  std::string error;
  const int exit_code = RunNodeCompatScript(s.env, "parallel/test-module-loading-subset.js", &error);
  EXPECT_EQ(exit_code, 0) << "error=" << error;
  EXPECT_TRUE(error.empty()) << "error=" << error;
}

#ifdef NAPI_V8_NODE_ROOT_PATH
TEST_F(Test3NodeDropinSubsetPhase02, RawRequireCacheFromNodeTest) {
  EnvScope s(runtime_.get());
  std::string error;
  const int exit_code = RunRawNodeTestScript(s.env, "test-require-cache.js", &error);
  EXPECT_EQ(exit_code, 0) << "error=" << error;
  EXPECT_TRUE(error.empty()) << "error=" << error;
}

TEST_F(Test3NodeDropinSubsetPhase02, RawRequireJsonFromNodeTest) {
  EnvScope s(runtime_.get());
  std::string error;
  const int exit_code = RunRawNodeTestScript(s.env, "test-require-json.js", &error);
  EXPECT_EQ(exit_code, 0) << "error=" << error;
  EXPECT_TRUE(error.empty()) << "error=" << error;
}

TEST_F(Test3NodeDropinSubsetPhase02, RawModuleCacheFromNodeTest) {
  EnvScope s(runtime_.get());
  std::string error;
  const int exit_code = RunRawNodeTestScript(s.env, "test-module-cache.js", &error);
  EXPECT_EQ(exit_code, 0) << "error=" << error;
  EXPECT_TRUE(error.empty()) << "error=" << error;
}

TEST_F(Test3NodeDropinSubsetPhase02, RawRequireDotFromNodeTest) {
  EnvScope s(runtime_.get());
  std::string error;
  const int exit_code = RunRawNodeTestScript(s.env, "test-require-dot.js", &error);
  EXPECT_EQ(exit_code, 0) << "error=" << error;
  EXPECT_TRUE(error.empty()) << "error=" << error;
}
#endif
