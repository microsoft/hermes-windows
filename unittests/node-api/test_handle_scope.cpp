// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "node_api_test.h"

#define Init test_handle_scope_init
#include "js-native-api/test_handle_scope/test_handle_scope.c"

using namespace node_api_test;

TEST_P(NodeApiTest, test_handle_scope) {
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_handle_scope",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_handle_scope/test.js");
  });
}
