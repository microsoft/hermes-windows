// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "node_api_test.h"

#define Init test_function_init
#include "js-native-api/test_function/test_function.c"

using namespace node_api_test;

TEST_P(NodeApiTest, test_function) {
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_function",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_function/test.js");
  });
}
