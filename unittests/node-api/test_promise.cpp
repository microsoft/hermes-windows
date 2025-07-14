// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "node_api_test.h"

#define Init test_promise_init
#include "js-native-api/test_promise/test_promise.c"

using namespace node_api_test;

TEST_P(NodeApiTest, test_promise) {
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_promise",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_promise/test.js");
  });
}
