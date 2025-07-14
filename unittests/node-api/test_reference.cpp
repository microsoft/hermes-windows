// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "node_api_test.h"

#define Init test_reference_init
#include "js-native-api/test_reference/test_reference.c"

using namespace node_api_test;

TEST_P(NodeApiTest, test_reference) {
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_reference",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_reference/test.js");
  });
}
