#include <filesystem>
#include <fstream>
#include <functional>
#include <string>

#include "test_env.h"
#include "unode_runtime.h"

class Test0RuntimePhase01 : public FixtureTestBase {};

namespace {

std::string WriteTempScript(const std::string& stem, const std::string& contents) {
  const auto temp_dir = std::filesystem::temp_directory_path();
  const auto unique_name =
      stem + "_" + std::to_string(static_cast<unsigned long long>(std::hash<std::string>{}(contents))) + ".js";
  const auto script_path = temp_dir / unique_name;
  std::ofstream out(script_path);
  out << contents;
  out.close();
  return script_path.string();
}

void RemoveTempScript(const std::string& path) {
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

}  // namespace

TEST_F(Test0RuntimePhase01, ValidFixtureScriptReturnsZero) {
  EnvScope s(runtime_.get());
  const std::string script_path = std::string(NAPI_V8_ROOT_PATH) + "/tests/fixtures/phase0_hello.js";
  ASSERT_TRUE(std::filesystem::exists(script_path)) << "Missing fixture: " << script_path;

  std::string error;
  const int exit_code = UnodeRunScriptFile(s.env, script_path.c_str(), &error);
  EXPECT_EQ(exit_code, 0) << "error=" << error << ", fixture=" << script_path;
  EXPECT_TRUE(error.empty()) << "error=" << error;

  napi_value global = nullptr;
  ASSERT_EQ(napi_get_global(s.env, &global), napi_ok);
  napi_value value = nullptr;
  ASSERT_EQ(napi_get_named_property(s.env, global, "__phase01_value", &value), napi_ok);

  int32_t extracted = 0;
  ASSERT_EQ(napi_get_value_int32(s.env, value, &extracted), napi_ok);
  EXPECT_EQ(extracted, 7);

}

TEST_F(Test0RuntimePhase01, ThrownErrorReturnsNonZero) {
  EnvScope s(runtime_.get());
  const std::string script_path = WriteTempScript("unode_phase01_throw", "throw new Error('boom from unode');");

  std::string error;
  const int exit_code = UnodeRunScriptFile(s.env, script_path.c_str(), &error);
  EXPECT_EQ(exit_code, 1);
  EXPECT_NE(error.find("boom from unode"), std::string::npos);

  RemoveTempScript(script_path);
}

TEST_F(Test0RuntimePhase01, SyntaxErrorReturnsNonZero) {
  EnvScope s(runtime_.get());
  const std::string script_path = WriteTempScript("unode_phase01_syntax", "function (");

  std::string error;
  const int exit_code = UnodeRunScriptFile(s.env, script_path.c_str(), &error);
  EXPECT_EQ(exit_code, 1);
  EXPECT_FALSE(error.empty());

  RemoveTempScript(script_path);
}
