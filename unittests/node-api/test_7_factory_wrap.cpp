// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "node_api_test.h"

#define Init test_7_factory_wrap_init
namespace {
#include "js-native-api/7_factory_wrap/binding.cc"
#include "js-native-api/7_factory_wrap/myobject.cc"
} // namespace

using namespace node_api_test;

TEST_P(NodeApiTest, test_7_factory_wrap) {
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/binding",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("7_factory_wrap/test.js");
  });
}
