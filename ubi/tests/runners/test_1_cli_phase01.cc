#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

#include "test_env.h"
#include "ubi_cli.h"

class Test1CliPhase01 : public FixtureTestBase {};

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

std::filesystem::path ResolveBuiltUbiBinary() {
  namespace fs = std::filesystem;
  const fs::path cwd = fs::current_path();
  const std::vector<fs::path> candidates = {
      cwd / "ubi",
      cwd / "build-ubi-rename" / "ubi",
      cwd / "build-ubi" / "ubi",
      cwd / "build" / "ubi",
      cwd.parent_path() / "ubi",
      cwd.parent_path() / "build-ubi-rename" / "ubi",
      cwd.parent_path() / "build-ubi" / "ubi",
      cwd.parent_path() / "build" / "ubi",
  };
  for (const auto& candidate : candidates) {
    std::error_code ec;
    if (!fs::exists(candidate, ec) || ec) continue;
    if (fs::is_directory(candidate, ec) || ec) continue;
    return fs::absolute(candidate).lexically_normal();
  }
  return {};
}

std::string ShellSingleQuoted(const std::string& input) {
  std::string out;
  out.reserve(input.size() + 2);
  out.push_back('\'');
  for (char c : input) {
    if (c == '\'') {
      out += "'\\''";
    } else {
      out.push_back(c);
    }
  }
  out.push_back('\'');
  return out;
}

}  // namespace

TEST_F(Test1CliPhase01, NoArgsWithStdinEofFallsBackToStdinMode) {
#if defined(_WIN32)
  GTEST_SKIP() << "stdin EOF subprocess check is POSIX-only";
#else
  const auto ubi_path = ResolveBuiltUbiBinary();
  ASSERT_FALSE(ubi_path.empty()) << "Failed to resolve built ubi binary";

  const auto temp_root = std::filesystem::temp_directory_path() / "ubi_phase01_cli_no_args";
  const auto stdout_path = temp_root / "stdout.txt";
  const auto stderr_path = temp_root / "stderr.txt";
  std::error_code ec;
  std::filesystem::remove_all(temp_root, ec);
  std::filesystem::create_directories(temp_root, ec);
  ASSERT_FALSE(ec) << "Failed to create temp directory";

  const std::string cmd =
      ShellSingleQuoted(ubi_path.string()) + " </dev/null >" +
      ShellSingleQuoted(stdout_path.string()) + " 2>" +
      ShellSingleQuoted(stderr_path.string());
  const int status = std::system(cmd.c_str());
  ASSERT_NE(status, -1);
  ASSERT_TRUE(WIFEXITED(status)) << "status=" << status;
  EXPECT_EQ(WEXITSTATUS(status), 0);

  std::ifstream stderr_in(stderr_path);
  std::string stderr_output((std::istreambuf_iterator<char>(stderr_in)),
                            std::istreambuf_iterator<char>());
  EXPECT_TRUE(stderr_output.empty()) << "stderr=" << stderr_output;

  std::filesystem::remove_all(temp_root, ec);
#endif
}

TEST_F(Test1CliPhase01, ExtraArgsAreForwardedToScriptArgv) {
  const std::string script_path = WriteTempScript(
      "ubi_phase01_cli_extra_args",
      "console.log(process.argv.slice(2).join(','));\n");
  const char* argv[] = {"ubi", script_path.c_str(), "alpha", "beta", "gamma"};

  testing::internal::CaptureStdout();
  std::string error;
  const int exit_code = UbiRunCli(5, argv, &error);
  const std::string stdout_output = testing::internal::GetCapturedStdout();

  EXPECT_EQ(exit_code, 0) << "error=" << error;
  EXPECT_TRUE(error.empty()) << "error=" << error;
  EXPECT_NE(stdout_output.find("alpha,beta,gamma"), std::string::npos);

  RemoveTempScript(script_path);
}

TEST_F(Test1CliPhase01, MissingScriptFileReturnsNonZero) {
  const std::string script_path =
      std::string(NAPI_V8_ROOT_PATH) + "/tests/fixtures/this_file_should_not_exist_phase01.js";
  const char* argv[] = {"ubi", script_path.c_str()};
  std::string error;
  const int exit_code = UbiRunCli(2, argv, &error);
  EXPECT_EQ(exit_code, 1);
  const std::string prefix = "Failed to read script file";
  ASSERT_GE(error.size(), prefix.size()) << "error=" << error;
  EXPECT_EQ(error.rfind(prefix, 0), 0u) << "error=" << error;
  if (error.size() > prefix.size()) {
    ASSERT_GE(error.size(), prefix.size() + 2) << "error=" << error;
    EXPECT_EQ(error.substr(prefix.size(), 2), ": ") << "error=" << error;
  }
}

TEST_F(Test1CliPhase01, ValidFixtureScriptReturnsZero) {
  const std::string script_path = std::string(NAPI_V8_ROOT_PATH) + "/tests/fixtures/phase0_hello.js";
  ASSERT_TRUE(std::filesystem::exists(script_path)) << "Missing fixture: " << script_path;
  const char* argv[] = {"ubi", script_path.c_str()};

  testing::internal::CaptureStdout();
  std::string error;
  const int exit_code = UbiRunCli(2, argv, &error);
  const std::string stdout_output = testing::internal::GetCapturedStdout();

  EXPECT_EQ(exit_code, 0) << "error=" << error;
  EXPECT_TRUE(error.empty()) << "error=" << error;
  EXPECT_NE(stdout_output.find("hello from ubi"), std::string::npos);
}

TEST_F(Test1CliPhase01, RelativeScriptPathWithoutDotPrefixRunsFromCwd) {
  const auto temp_root = std::filesystem::temp_directory_path() / "ubi_phase01_relative_entry";
  const auto script_dir = temp_root / "examples";
  const auto script_path = script_dir / "relative_entry.js";
  std::error_code ec;
  std::filesystem::remove_all(temp_root, ec);
  std::filesystem::create_directories(script_dir, ec);
  ASSERT_FALSE(ec) << "Failed to create temp test directories";

  std::ofstream out(script_path);
  out << "console.log('relative entry ok');\n";
  out.close();
  ASSERT_TRUE(out.good()) << "Failed to write temp script";

  const auto original_cwd = std::filesystem::current_path();
  std::filesystem::current_path(temp_root, ec);
  ASSERT_FALSE(ec) << "Failed to set cwd to temp root";

  const char* argv[] = {"ubi", "examples/relative_entry.js"};
  testing::internal::CaptureStdout();
  std::string error;
  const int exit_code = UbiRunCli(2, argv, &error);
  const std::string stdout_output = testing::internal::GetCapturedStdout();

  std::filesystem::current_path(original_cwd, ec);
  std::filesystem::remove_all(temp_root, ec);

  EXPECT_EQ(exit_code, 0) << "error=" << error;
  EXPECT_TRUE(error.empty()) << "error=" << error;
  EXPECT_NE(stdout_output.find("relative entry ok"), std::string::npos);
}

TEST_F(Test1CliPhase01, RelativeScriptPathFallsBackToUbiSubdirectory) {
  const auto temp_root = std::filesystem::temp_directory_path() / "ubi_phase01_repo_fallback";
  const auto script_dir = temp_root / "ubi" / "examples";
  const auto script_path = script_dir / "fallback_entry.js";
  std::error_code ec;
  std::filesystem::remove_all(temp_root, ec);
  std::filesystem::create_directories(script_dir, ec);
  ASSERT_FALSE(ec) << "Failed to create temp test directories";

  std::ofstream out(script_path);
  out << "console.log('fallback entry ok');\n";
  out.close();
  ASSERT_TRUE(out.good()) << "Failed to write temp script";

  const auto original_cwd = std::filesystem::current_path();
  std::filesystem::current_path(temp_root, ec);
  ASSERT_FALSE(ec) << "Failed to set cwd to temp root";

  const char* argv[] = {"ubi", "examples/fallback_entry.js"};
  testing::internal::CaptureStdout();
  std::string error;
  const int exit_code = UbiRunCli(2, argv, &error);
  const std::string stdout_output = testing::internal::GetCapturedStdout();

  std::filesystem::current_path(original_cwd, ec);
  std::filesystem::remove_all(temp_root, ec);

  EXPECT_EQ(exit_code, 0) << "error=" << error;
  EXPECT_TRUE(error.empty()) << "error=" << error;
  EXPECT_NE(stdout_output.find("fallback entry ok"), std::string::npos);
}

TEST_F(Test1CliPhase01, RuntimeThrownErrorReturnsNonZero) {
  const std::string script_path = WriteTempScript("ubi_phase01_cli_throw", "throw new Error('boom from cli');");
  const char* argv[] = {"ubi", script_path.c_str()};
  std::string error;

  const int exit_code = UbiRunCli(2, argv, &error);
  EXPECT_EQ(exit_code, 1);
  EXPECT_NE(error.find("boom from cli"), std::string::npos);

  RemoveTempScript(script_path);
}

TEST_F(Test1CliPhase01, RuntimeSyntaxErrorReturnsNonZero) {
  const std::string script_path = WriteTempScript("ubi_phase01_cli_syntax", "function (");
  const char* argv[] = {"ubi", script_path.c_str()};
  std::string error;

  const int exit_code = UbiRunCli(2, argv, &error);
  EXPECT_EQ(exit_code, 1);
  EXPECT_FALSE(error.empty());

  RemoveTempScript(script_path);
}

TEST_F(Test1CliPhase01, EvalFlagExecutesSource) {
  const char* argv[] = {"ubi", "-e", "console.log('eval-ok')"};
  testing::internal::CaptureStdout();
  std::string error;
  const int exit_code = UbiRunCli(3, argv, &error);
  const std::string stdout_output = testing::internal::GetCapturedStdout();

  EXPECT_EQ(exit_code, 0) << "error=" << error;
  EXPECT_TRUE(error.empty()) << "error=" << error;
  EXPECT_NE(stdout_output.find("eval-ok"), std::string::npos);
}

TEST_F(Test1CliPhase01, PrintFlagEvaluatesExpression) {
  const char* argv[] = {"ubi", "-p", "40 + 2"};
  testing::internal::CaptureStdout();
  std::string error;
  const int exit_code = UbiRunCli(3, argv, &error);
  const std::string stdout_output = testing::internal::GetCapturedStdout();

  EXPECT_EQ(exit_code, 0) << "error=" << error;
  EXPECT_TRUE(error.empty()) << "error=" << error;
  EXPECT_NE(stdout_output.find("42"), std::string::npos);
}

TEST_F(Test1CliPhase01, BeforeExitCanScheduleMoreWork) {
  const std::string script_path = WriteTempScript(
      "ubi_phase01_cli_before_exit_loop",
      "const dns = require('dns');\n"
      "let count = 0;\n"
      "process.on('beforeExit', (code) => {\n"
      "  console.log('beforeExit:' + count + ':' + code);\n"
      "  if (count === 0) {\n"
      "    count += 1;\n"
      "    dns.lookup('localhost', () => {\n"
      "      console.log('lookup-fired');\n"
      "      process.exitCode = 3;\n"
      "    });\n"
      "  }\n"
      "});\n"
      "process.on('exit', (code) => console.log('exit:' + code));\n");
  const char* argv[] = {"ubi", script_path.c_str()};

  testing::internal::CaptureStdout();
  std::string error;
  const int exit_code = UbiRunCli(2, argv, &error);
  const std::string stdout_output = testing::internal::GetCapturedStdout();

  EXPECT_EQ(exit_code, 3) << "error=" << error;
  EXPECT_TRUE(error.empty()) << "error=" << error;
  EXPECT_NE(stdout_output.find("beforeExit:0:0"), std::string::npos);
  EXPECT_NE(stdout_output.find("lookup-fired"), std::string::npos);
  EXPECT_NE(stdout_output.find("beforeExit:1:3"), std::string::npos);
  EXPECT_NE(stdout_output.find("exit:3"), std::string::npos);

  RemoveTempScript(script_path);
}

TEST_F(Test1CliPhase01, ProcessExitCodeWithoutExplicitProcessExitIsReturned) {
  const std::string script_path = WriteTempScript(
      "ubi_phase01_cli_exit_code_only",
      "process.exitCode = 7;\n"
      "process.on('exit', (code) => console.log('exit:' + code));\n");
  const char* argv[] = {"ubi", script_path.c_str()};

  testing::internal::CaptureStdout();
  std::string error;
  const int exit_code = UbiRunCli(2, argv, &error);
  const std::string stdout_output = testing::internal::GetCapturedStdout();

  EXPECT_EQ(exit_code, 7) << "error=" << error;
  EXPECT_TRUE(error.empty()) << "error=" << error;
  EXPECT_NE(stdout_output.find("exit:7"), std::string::npos);

  RemoveTempScript(script_path);
}

TEST_F(Test1CliPhase01, ExplicitProcessExitDoesNotEmitBeforeExit) {
  const std::string script_path = WriteTempScript(
      "ubi_phase01_cli_explicit_exit",
      "process.on('beforeExit', () => console.log('beforeExit'));\n"
      "process.on('exit', (code) => console.log('exit:' + code));\n"
      "process.exit(5);\n");
  const char* argv[] = {"ubi", script_path.c_str()};

  testing::internal::CaptureStdout();
  std::string error;
  const int exit_code = UbiRunCli(2, argv, &error);
  const std::string stdout_output = testing::internal::GetCapturedStdout();

  EXPECT_EQ(exit_code, 5);
  EXPECT_EQ(error, "process.exit(5)");
  EXPECT_EQ(stdout_output.find("beforeExit"), std::string::npos);
  EXPECT_NE(stdout_output.find("exit:5"), std::string::npos);

  RemoveTempScript(script_path);
}
