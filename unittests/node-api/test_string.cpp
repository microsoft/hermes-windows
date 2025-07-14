// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "node_api_test.h"

#define Init test_string_init
#include "js-native-api/test_string/test_string.c"

using namespace node_api_test;

TEST_P(NodeApiTest, test_string) {
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_string",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_string/test.js");
  });
}
