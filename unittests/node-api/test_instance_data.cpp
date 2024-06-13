// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "node_api_test.h"

#define Init test_instance_data_init

// Redirect printf output to s_output.
// Define it before the includes.
#define printf(...) test_printf(s_output, __VA_ARGS__)
static std::string s_output;

#include "js-native-api/test_instance_data/test_instance_data.c"

using namespace node_api_test;

TEST_P(NodeApiTest, test_instance_data) {
  s_output = "";
  ExecuteNodeApi([](NodeApiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_instance_data",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript("test_instance_data/test.js");
  });
  EXPECT_EQ(s_output, "deleting addon data\n");
}
