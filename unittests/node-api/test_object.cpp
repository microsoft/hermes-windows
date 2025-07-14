// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "node_api_test.h"

#define Init test_object_init
#include "js-native-api/test_object/test_null.c"
#include "js-native-api/test_object/test_object.c"

using namespace node_api_test;

TEST_P(NodeApiTest, test_object) {
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_object",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_object/test.js");
  });
}

TEST_P(NodeApiTest, test_object_null) {
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_object",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_object/test_null.js");
  });
}
