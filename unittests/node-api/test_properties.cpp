// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "node_api_test.h"

#define Init test_properties_init
#include "js-native-api/test_properties/test_properties.c"

using namespace node_api_test;

TEST_P(NodeApiTest, test_properties) {
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_properties",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_properties/test.js");
  });
}
