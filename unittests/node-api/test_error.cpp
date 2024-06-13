// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "node_api_test.h"

#define Init test_error_init
#include "js-native-api/test_error/test_error.c"

using namespace node_api_test;

TEST_P(NodeApiTest, test_error) {
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_error",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_error/test.js");
  });
}
