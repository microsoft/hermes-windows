// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "node_api_test.h"

#define Init test_5_function_factory_init
#include "js-native-api/5_function_factory/binding.c"

using namespace node_api_test;

TEST_P(NodeApiTest, test_5_function_factory) {
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/binding",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("5_function_factory/test.js");
  });
}
