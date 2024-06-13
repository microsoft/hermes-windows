// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "node_api_test.h"

#define Init test_reference_double_free_init
#define delete delete1
#include "js-native-api/test_reference_double_free/test_reference_double_free.c"
#undef delete

using namespace node_api_test;

TEST_P(NodeApiTest, test_reference_double_free) {
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_reference_double_free",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_reference_double_free/test.js");
  });
}
