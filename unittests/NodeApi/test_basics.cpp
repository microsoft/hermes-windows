// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include "child_process.h"
#include "test_main.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace node_api_tests {

class BasicsTest : public TestFixtureBase {
 protected:
  void SetUp() override { basics_js_dir_ = js_root_dir_ / "basics"; }

  ProcessResult RunScript(std::string_view script_filename) noexcept {
    return SpawnSync(node_lite_path_.string(),
                     {(basics_js_dir_ / script_filename).string()});
  }

  bool StringContains(std::string_view str, std::string_view substr) {
    return str.find(substr) != std::string::npos;
  }

 private:
  fs::path basics_js_dir_;
};

#ifdef _WIN32
TEST_F(BasicsTest, ChildAbortReportsToStderrWithoutDialogs) {
  ProcessResult result = SpawnSync(test_exe_path_.string(), {"--abort-test-child"});
  ASSERT_FALSE(result.timed_out) << result.std_error;
  EXPECT_NE(result.status, 0);
  EXPECT_TRUE(StringContains(result.std_error, "intentional test child abort"));
  EXPECT_TRUE(StringContains(result.std_error, "abort() has been called"));
}

// Exercise timeout and normal exit: both must release inherited pipes and kill
// the grandchild. The PID proves cleanup, not just return from the pipe read.
TEST_F(BasicsTest, ChildJobClosesInheritedPipesAndTerminatesGrandchildren) {
  for (bool timeout : {false, true}) {
    SCOPED_TRACE(timeout);
    ProcessResult result =
        SpawnSync(test_exe_path_.string(),
                  {timeout ? "--job-test-timeout" : "--job-test-exit"},
                  2000);
    EXPECT_EQ(result.timed_out, timeout);
    EXPECT_EQ(result.status, timeout ? ERROR_TIMEOUT : 0);
    char* end = nullptr;
    const DWORD pid = std::strtoul(result.std_output.c_str(), &end, 10);
    ASSERT_NE(end, result.std_output.c_str()) << result.std_error;
    ASSERT_NE(pid, 0);
    HANDLE grandchild = ::OpenProcess(
        SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (grandchild) {
      DWORD wait = ::WaitForSingleObject(grandchild, 5000);
      if (wait == WAIT_TIMEOUT) {
        char image[MAX_PATH];
        DWORD imageSize = MAX_PATH;
        const BOOL queried =
            ::QueryFullProcessImageNameA(grandchild, 0, image, &imageSize);
        // Exited processes may no longer have an image to query.
        if (::WaitForSingleObject(grandchild, 0) == WAIT_OBJECT_0 ||
            (queried && fs::path(std::string(image, imageSize)).filename() !=
                            test_exe_path_.filename())) {
          wait = WAIT_OBJECT_0;
        }
      }
      // Do not terminate by PID: it may have been reused after job cleanup.
      ::CloseHandle(grandchild);
      EXPECT_EQ(wait, WAIT_OBJECT_0) << "Surviving grandchild PID " << pid;
    } else {
      EXPECT_EQ(::GetLastError(), ERROR_INVALID_PARAMETER) << "PID " << pid;
    }
  }
}
#endif

TEST_F(BasicsTest, TestHello) {
  ProcessResult result = RunScript("hello.js");
  ASSERT_TRUE(StringContains(result.std_output, "Hello"));
}

TEST_F(BasicsTest, TestThrowString) {
  ProcessResult result = RunScript("throw_string.js");
  ASSERT_TRUE(StringContains(result.std_error, "Script failed"));
}

TEST_F(BasicsTest, TestAsyncResolved) {
  ProcessResult result = RunScript("async_resolved.js");
  ASSERT_TRUE(StringContains(result.std_output, "test async calling"));
  ASSERT_TRUE(
      StringContains(result.std_output, "Expected: test async resolved"));
}

TEST_F(BasicsTest, TestAsyncRejected) {
  ProcessResult result = RunScript("async_rejected.js");
  ASSERT_TRUE(StringContains(result.std_output, "test async calling"));
  ASSERT_TRUE(
      StringContains(result.std_error, "Expected: test async rejected"));
}

TEST_F(BasicsTest, TestMustCallSuccess) {
  ProcessResult result = RunScript("mustcall_success.js");
  ASSERT_TRUE(result.status == 0);
}

TEST_F(BasicsTest, TestMustCallFailure) {
  ProcessResult result = RunScript("mustcall_failure.js");
  ASSERT_TRUE(result.status != 0);
  ASSERT_TRUE(
      StringContains(result.std_error, "Mismatched noop function calls"));
}

TEST_F(BasicsTest, TestMustNotCallSuccess) {
  ProcessResult result = RunScript("mustnotcall_success.js");
  ASSERT_TRUE(result.status == 0);
}

TEST_F(BasicsTest, TestMustNotCallFailure) {
  ProcessResult result = RunScript("mustnotcall_failure.js");
  ASSERT_TRUE(result.status != 0);
  ASSERT_TRUE(
      StringContains(result.std_error, "Function should not have been called"));
}

}  // namespace node_api_tests
