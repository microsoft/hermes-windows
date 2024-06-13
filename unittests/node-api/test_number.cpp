// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "node_api_test.h"

#define Init test_number_init
#include "js-native-api/test_number/test_number.c"

using namespace node_api_test;

TEST_P(NodeApiTest, test_number) {
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_number",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_number/test.js");
  });
}
