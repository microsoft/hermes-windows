// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "node_api_test.h"

#define Init test_array_init
#include "js-native-api/test_array/test_array.c"

using namespace node_api_test;

TEST_P(NodeApiTest, test_array) {
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_array",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_array/test.js");
  });
}
