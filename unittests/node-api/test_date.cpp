// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "node_api_test.h"

#define Init test_date_init
#include "js-native-api/test_date/test_date.c"

using namespace node_api_test;

TEST_P(NodeApiTest, test_date) {
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_date",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_date/test.js");
  });
}
