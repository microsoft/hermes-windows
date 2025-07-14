// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <cstdarg>
#include <thread>
#include "node_api_test.h"

#define Init test_general_init

// Redirect printf output to s_output.
// Define it before the includes.
#define printf(...) test_printf(s_output, __VA_ARGS__)
static std::string s_output;

#include "js-native-api/test_general/test_general.c"

void ResetStatics() {
  deref_item_called = false;
  finalize_called = false;
}

using namespace node_api_test;

TEST_P(NodeApiTest, test_general) {
  ResetStatics();
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_general",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_general/test.js");
  });
}

TEST_P(NodeApiTest, test_general_NapiStatus) {
  ResetStatics();
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_general",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_general/testNapiStatus.js");
  });
}

TEST_P(NodeApiTest, test_general_NapiRun) {
  ResetStatics();
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_general",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_general/testNapiRun.js");
  });
}

// TODO: [vmoroz] The test uses external V8 tests
// TEST_P(NodeApiTest, test_general_InstanceOf) {
//   ResetStatics();
//   ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
//     testContext->AddNativeModule(
//         "./build/x86/test_general",
//         [](napi_env env, napi_value exports) { return Init(env, exports); });
//     testContext->RunTestScript("test_general/testInstanceOf.js");
//   });
// }

TEST_P(NodeApiTest, test_general_Globals) {
  ResetStatics();
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_general",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_general/testGlobals.js");
  });
}

TEST_P(NodeApiTest, test_general_Finalizer) {
  ResetStatics();
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_general",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_general/testFinalizer.js");
  });
}

TEST_P(NodeApiTest, test_general_EnvCleanup) {
  ResetStatics();
  s_output = "";
  auto spawnSyncCallback = [](napi_env env,
                              napi_callback_info info) -> napi_value {
    NodeApiTest *test;
    napi_get_cb_info(
        env, info, nullptr, nullptr, nullptr, reinterpret_cast<void **>(&test));
    auto childThread = std::thread([test]() {
      test->ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
        testContext->AddNativeModule(
            "./build/x86/test_general", [](napi_env env, napi_value exports) {
              return Init(env, exports);
            });

        testContext->RunScript(R"(
          process = { argv:['', '', 'child'] };
        )");

        testContext->RunTestScript("test_general/testEnvCleanup.js");
      });
    });
    childThread.join();

    napi_value child{}, strValue{}, statusValue{};
    THROW_IF_NOT_OK(napi_create_object(env, &child));
    THROW_IF_NOT_OK(napi_create_string_utf8(
        env, s_output.c_str(), s_output.length(), &strValue));
    THROW_IF_NOT_OK(napi_set_named_property(env, child, "stdout", strValue));
    THROW_IF_NOT_OK(napi_create_int32(env, 0, &statusValue));
    THROW_IF_NOT_OK(napi_set_named_property(env, child, "status", statusValue));
    return child;
  };

  ExecuteNodeApi([&](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_general",
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

    testContext->RunTestScript("test_general/testEnvCleanup.js");
  });
}
