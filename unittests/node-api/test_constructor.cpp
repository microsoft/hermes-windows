// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "node_api_test.h"

#define Init test_constructor_init
#include "js-native-api/test_constructor/test_constructor.c"

using namespace node_api_test;

TEST_P(NodeApiTest, test_constructor) {
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_constructor",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_constructor/test.js");
  });
}

TEST_P(NodeApiTest, test_constructor2) {
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_constructor",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_constructor/test2.js");
  });
}
