// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "node_api_test.h"

#define Init test_new_target_init
#include "js-native-api/test_new_target/binding.c"

using namespace node_api_test;

TEST_P(NodeApiTest, test_new_target) {
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/binding",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_new_target/test.js");
  });
}
