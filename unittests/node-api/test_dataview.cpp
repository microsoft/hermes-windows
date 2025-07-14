// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "node_api_test.h"

#define Init test_dataview_init
#include "js-native-api/test_dataview/test_dataview.c"

using namespace node_api_test;

TEST_P(NodeApiTest, test_dataview) {
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_dataview",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_dataview/test.js");
  });
}
