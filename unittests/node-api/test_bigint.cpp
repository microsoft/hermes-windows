// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "node_api_test.h"

#define Init test_bigint_init
#include "js-native-api/test_bigint/test_bigint.c"

using namespace node_api_test;

TEST_P(NodeApiTest, test_bigint) {
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_bigint",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_bigint/test.js");
  });
}
