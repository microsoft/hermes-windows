// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "node_api_test.h"

#define Init test_2_function_arguments_init
#include "js-native-api/2_function_arguments/binding.c"

using namespace node_api_test;

TEST_P(NodeApiTest, test_2_function_arguments) {
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/binding",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("2_function_arguments/test.js");
  });
}
