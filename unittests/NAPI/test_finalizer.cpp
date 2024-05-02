// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitest.h"

#define Init test_finalizer_init
#include "js-native-api/test_finalizer/test_finalizer.c"

using namespace napitest;

TEST_P(NapiTest, test_finalizer) {
  ExecuteNapi([](NapiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_finalizer",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_finalizer/test.js");
  });
}
