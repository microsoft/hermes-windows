// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "node_api_test.h"

#define Init test_3_callbacks_init
#include "js-native-api/3_callbacks/binding.c"

using namespace node_api_test;

TEST_P(NodeApiTest, test_3_callbacks) {
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/binding",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("3_callbacks/test.js");
  });
}
