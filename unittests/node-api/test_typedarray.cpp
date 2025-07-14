// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "node_api_test.h"

#define Init test_typedarray_init
#include "js-native-api/test_typedarray/test_typedarray.c"

using namespace node_api_test;

TEST_P(NodeApiTest, test_typedarray) {
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_typedarray",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_typedarray/test.js");
  });
}
