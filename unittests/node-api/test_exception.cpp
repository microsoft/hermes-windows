// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <thread>
#include "node_api_test.h"

#define Init test_exception_init
#include "js-native-api/test_exception/test_exception.c"

using namespace node_api_test;

TEST_P(NodeApiTest, test_exception) {
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_exception",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_exception/test.js");
  });
}

TEST_P(NodeApiTest, test_exception_finalizer) {
  auto spawnSyncCallback = [](napi_env env,
                              napi_callback_info info) -> napi_value {
    NodeApiTest *test;
    napi_get_cb_info(
        env, info, nullptr, nullptr, nullptr, reinterpret_cast<void **>(&test));
    std::string error;
    auto childThread = std::thread([test, &error]() {
      test->ExecuteNodeApi([&error](NodeApiTestContext *testContext, napi_env env) {
        testContext->AddNativeModule(
            "./build/x86/test_exception", [](napi_env env, napi_value exports) {
              return Init(env, exports);
            });

        testContext->RunScript(R"(
          process = { argv:['', '', 'child'] };
        )");

        testContext->RunTestScript("test_exception/testFinalizerException.js")
            .Throws("Error", [&error](NodeApiTestException const &ex) noexcept {
              error = ex.ErrorInfo()->Message;
            });
      });
    });
    childThread.join();

    napi_value child{}, null{}, errValue{};
    THROW_IF_NOT_OK(napi_create_object(env, &child));
    THROW_IF_NOT_OK(napi_get_null(env, &null));
    THROW_IF_NOT_OK(napi_set_named_property(env, child, "signal", null));
    THROW_IF_NOT_OK(
        napi_create_string_utf8(env, error.c_str(), error.length(), &errValue));
    THROW_IF_NOT_OK(napi_set_named_property(env, child, "stderr", errValue));
    return child;
  };

  ExecuteNodeApi([&](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_exception",
        [](napi_env env, napi_value exports) { return Init(env, exports); });

    testContext->RunScript(R"(
      process = { argv:[] };
      __filename = '';
    )");

    testContext->AddNativeModule(
        "child_process", [&](napi_env env, napi_value exports) {
          napi_value spawnSync{};
          THROW_IF_NOT_OK(napi_create_function(
              env,
              "spawnSync",
              NAPI_AUTO_LENGTH,
              spawnSyncCallback,
              this,
              &spawnSync));
          THROW_IF_NOT_OK(
              napi_set_named_property(env, exports, "spawnSync", spawnSync));
          return exports;
        });

    testContext->RunTestScript("test_exception/testFinalizerException.js");
  });
}
