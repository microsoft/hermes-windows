// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "node_api_test.h"

#define Init test_symbol_init
#include "js-native-api/test_symbol/test_symbol.c"

using namespace node_api_test;

TEST_P(NodeApiTest, test_symbol1) {
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_symbol",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_symbol/test1.js");
  });
}

TEST_P(NodeApiTest, test_symbol2) {
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_symbol",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_symbol/test2.js");
  });
}

TEST_P(NodeApiTest, test_symbol3) {
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_symbol",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_symbol/test3.js");
  });
}
