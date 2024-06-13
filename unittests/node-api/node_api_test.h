// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

// These tests are for Node-API and should not be JS engine specific

#pragma once

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#define NAPI_EXPERIMENTAL
#include "js_runtime_api.h"

extern "C" {
#include "js-native-api/common.h"
}

// Crash if the condition is false.
#define CRASH_IF_FALSE(condition)  \
  do {                             \
    if (!(condition)) {            \
      assert(false && #condition); \
      *((int *)nullptr) = 1;       \
      std::terminate();            \
    }                              \
  } while (false)

// Use this macro to handle Node-API function results in test code.
// It throws NodeApiTestException that we then convert to GTest failure.
#define THROW_IF_NOT_OK(expr)                                \
  do {                                                       \
    napi_status temp_status__ = (expr);                      \
    if (temp_status__ != napi_status::napi_ok) {             \
      throw NodeApiTestException(env, temp_status__, #expr); \
    }                                                        \
  } while (false)

// Runs the script with captured file name and the line number.
// The __LINE__ points to the end of the macro call.
// We must adjust the line number to point to the beginning of hte script.
#define RUN_TEST_SCRIPT(script) \
  testContext->RunTestScript(   \
      script, __FILE__, (__LINE__ - node_api_test::GetEndOfLineCount(script)))

// A shortcut to produce GTest error at specified location.
#define FAIL_AT(file, line) \
  GTEST_MESSAGE_AT_(        \
      file, line, "Fail", ::testing::TestPartResult::kFatalFailure)

// Define operator '|' to allow "or-ing" napi_property_attributes in tests.
constexpr napi_property_attributes operator|(
    napi_property_attributes left,
    napi_property_attributes right) {
  return napi_property_attributes(
      static_cast<int>(left) | static_cast<int>(right));
}

// Use to override printf in tests to send output to a std::string instead of
// stdout.
extern int test_printf(std::string &output, const char *format, ...);

namespace node_api_test {

// Forward declarations
struct NodeApiTest;
struct NodeApiTestContext;
struct NodeApiTestErrorHandler;
struct NodeApiTestException;

struct IEnvHolder {
  virtual ~IEnvHolder() {}
  virtual napi_env getEnv() = 0;
};

// Use for test parameterization.
struct NodeApiTestData {
  std::string TestJSPath;
  std::function<std::unique_ptr<IEnvHolder>()> EnvHolderFactory;
};

std::vector<NodeApiTestData> NodeApiEnvFactories();

// The base class for unit tests that we parameterize by NodeApiTestData.
struct NodeApiTest : ::testing::TestWithParam<NodeApiTestData> {
  NodeApiTestErrorHandler ExecuteNodeApi(
      std::function<void(NodeApiTestContext *, napi_env)> code) noexcept;
};

// Properties from JavaScript Error object.
struct NodeApiErrorInfo {
  std::string Name;
  std::string Message;
  std::string Stack;
};

// Properties from JavaScript AssertionError object.
struct NodeApiAssertionErrorInfo {
  std::string Method;
  std::string Expected;
  std::string Actual;
  std::string SourceFile;
  int32_t SourceLine;
  std::string ErrorStack;
};

struct TestScriptInfo {
  std::string script;
  std::string file;
  int32_t line;
};

inline int32_t GetEndOfLineCount(char const *script) noexcept {
  return std::count(script, script + strlen(script), '\n');
}

// The exception used to propagate Node-API and script errors.
struct NodeApiTestException : std::exception {
  NodeApiTestException() noexcept = default;

  NodeApiTestException(
      napi_env env,
      napi_status errorCode,
      char const *expr) noexcept;

  NodeApiTestException(napi_env env, napi_value error) noexcept;

  const char *what() const noexcept override {
    return m_what.c_str();
  }

  napi_status ErrorCode() const noexcept {
    return m_errorCode;
  }

  std::string const &Expr() const noexcept {
    return m_expr;
  }

  NodeApiErrorInfo const *ErrorInfo() const noexcept {
    return m_errorInfo.get();
  }

  NodeApiAssertionErrorInfo const *AssertionErrorInfo() const noexcept {
    return m_assertionErrorInfo.get();
  }

 private:
  void ApplyScriptErrorData(napi_env env, napi_value error);
  static napi_value GetProperty(napi_env env, napi_value obj, char const *name);
  static std::string
  GetPropertyString(napi_env env, napi_value obj, char const *name);
  static int32_t
  GetPropertyInt32(napi_env env, napi_value obj, char const *name);
  static std::string CoerceToString(napi_env env, napi_value value);
  static std::string ToString(napi_env env, napi_value value);

 private:
  napi_status m_errorCode{};
  std::string m_expr;
  std::string m_what;
  std::shared_ptr<NodeApiErrorInfo> m_errorInfo;
  std::shared_ptr<NodeApiAssertionErrorInfo> m_assertionErrorInfo;
};

// Define NodeApiRef "smart pointer" for napi_ref as unique_ptr with a custom
// deleter.
struct NodeApiRefDeleter {
  NodeApiRefDeleter(napi_env env) noexcept : env(env) {}

  void operator()(napi_ref ref) {
    THROW_IF_NOT_OK(napi_delete_reference(env, ref));
  }

 private:
  napi_env env;
};

using NodeApiRef = std::unique_ptr<napi_ref__, NodeApiRefDeleter>;
extern NodeApiRef MakeNodeApiRef(napi_env env, napi_value value);

struct NodeApiHandleScope {
  NodeApiHandleScope(napi_env env) noexcept : m_env{env} {
    CRASH_IF_FALSE(napi_open_handle_scope(env, &m_scope) == napi_ok);
  }

  ~NodeApiHandleScope() noexcept {
    CRASH_IF_FALSE(napi_close_handle_scope(m_env, m_scope) == napi_ok);
  }

 private:
  napi_env m_env{nullptr};
  napi_handle_scope m_scope{nullptr};
};

struct NodeApiEnvScope {
  NodeApiEnvScope(napi_env env) noexcept : m_env{env} {
    CRASH_IF_FALSE(jsr_open_napi_env_scope(env, &m_scope) == napi_ok);
  }

  ~NodeApiEnvScope() noexcept {
    if (m_env != nullptr) {
      CRASH_IF_FALSE(jsr_close_napi_env_scope(m_env, m_scope) == napi_ok);
    }
  }

  NodeApiEnvScope(NodeApiEnvScope &&other)
      : m_env(std::exchange(other.m_env, nullptr)),
        m_scope(std::exchange(other.m_scope, nullptr)) {}

  NodeApiEnvScope &operator=(NodeApiEnvScope &&other) {
    if (this != &other) {
      NodeApiEnvScope temp(std::move(*this));
      m_env = std::exchange(other.m_env, nullptr);
      m_scope = std::exchange(other.m_scope, nullptr);
    }
    return *this;
  }

  NodeApiEnvScope(const NodeApiEnvScope &) = delete;
  NodeApiEnvScope &operator=(const NodeApiEnvScope &) = delete;

 private:
  napi_env m_env{};
  jsr_napi_env_scope m_scope{};
};

// The context to run a Node-API test.
// Some tests require interaction of multiple JS environments.
// Thus, it is more convenient to have a special NodeApiTestContext instead of
// setting the environment per test.
struct NodeApiTestContext {
  NodeApiTestContext(napi_env env, std::string const &testJSPath);

  static std::map<std::string, TestScriptInfo, std::less<>> GetCommonScripts(
      std::string const &testJSPath) noexcept;

  napi_value RunScript(
      std::string const &code,
      char const *sourceUrl = nullptr);
  napi_value GetModule(std::string const &moduleName);
  TestScriptInfo *GetTestScriptInfo(std::string const &moduleName);

  NodeApiTestErrorHandler
  RunTestScript(char const *script, char const *file, int32_t line);
  NodeApiTestErrorHandler RunTestScript(TestScriptInfo const &scripInfo);
  NodeApiTestErrorHandler RunTestScript(std::string const &scriptFile);

  static std::string ReadScriptText(
      std::string const &testJSPath,
      std::string const &scriptFile);
  static std::string ReadFileText(std::string const &fileName);

  void AddNativeModule(
      char const *moduleName,
      std::function<napi_value(napi_env, napi_value)> initModule);

  void DefineGlobalFunctions();
  void RunCallChecks();
  void HandleUnhandledPromiseRejections();

  std::string ProcessStack(
      std::string const &stack,
      std::string const &assertMethod);

  // The callback function to be executed after the script completion.
  uint32_t AddTask(napi_value callback) noexcept;
  void RemoveTask(uint32_t taskId) noexcept;
  void DrainTaskQueue();

 private:
  napi_env env;
  std::string m_testJSPath;
  NodeApiEnvScope m_envScope;
  NodeApiHandleScope m_handleScope;
  std::map<std::string, NodeApiRef, std::less<>> m_modules;
  std::map<std::string, TestScriptInfo, std::less<>> m_scriptModules;
  std::map<std::string, std::function<napi_value(napi_env, napi_value)>>
      m_nativeModules;
  std::list<std::pair<uint32_t, NodeApiRef>> m_taskQueue;
  uint32_t m_nextTaskId{1};
};

// Handles the exceptions after running tests.
// In case if the exception is expected, we can add a custom Throws exception
// handler.
struct NodeApiTestErrorHandler {
  NodeApiTestErrorHandler(
      NodeApiTestContext *testContext,
      std::exception_ptr const &exception,
      std::string &&script,
      std::string &&file,
      int32_t line,
      int32_t scriptLineOffset) noexcept;
  ~NodeApiTestErrorHandler() noexcept;
  void Catch(
      std::function<void(NodeApiTestException const &)> &&handler) noexcept;
  void Throws(
      std::function<void(NodeApiTestException const &)> &&handler) noexcept;
  void Throws(
      char const *jsErrorName,
      std::function<void(NodeApiTestException const &)> &&handler) noexcept;

  NodeApiTestErrorHandler(NodeApiTestErrorHandler const &) = delete;
  NodeApiTestErrorHandler &operator=(NodeApiTestErrorHandler const &) = delete;

  NodeApiTestErrorHandler(NodeApiTestErrorHandler &&) = default;
  NodeApiTestErrorHandler &operator=(NodeApiTestErrorHandler &&) = default;

 private:
  std::string GetSourceCodeSliceForError(
      int32_t lineIndex,
      int32_t extraLineCount) noexcept;

 private:
  NodeApiTestContext *m_testContext;
  std::exception_ptr m_exception;
  std::string m_script;
  std::string m_file;
  int32_t m_line;
  int32_t m_scriptLineOffset;
  std::function<void(NodeApiTestException const &)> m_handler;
  bool m_mustThrow{false};
  std::string m_jsErrorName;
};

} // namespace node_api_test
