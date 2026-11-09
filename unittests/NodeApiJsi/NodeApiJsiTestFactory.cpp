// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <hermes_node_api_jsi/ApiLoaders/HermesApi.h>
#include <hermes_node_api_jsi/NodeApiJsiRuntime.h>
#include <jsi/test/testlib.h>

namespace facebook::jsi {

std::vector<RuntimeFactory> runtimeGenerators() {
  return {[] {
    Microsoft::NodeApiJsi::HermesApi *hermesApi =
        Microsoft::NodeApiJsi::HermesApi::fromLib();
    Microsoft::NodeApiJsi::HermesApi::setCurrent(hermesApi);

    jsr_config config{};
    jsr_runtime runtime{};
    napi_env env{};
    hermesApi->jsr_create_config(&config);
    hermesApi->jsr_config_enable_gc_api(config, true);
    hermesApi->jsr_create_runtime(config, &runtime);
    hermesApi->jsr_delete_config(config);
    hermesApi->jsr_runtime_get_node_api_env(runtime, &env);

    Microsoft::NodeApiJsi::NodeApiEnvScope envScope{env};

    return makeNodeApiJsiRuntime(env, hermesApi, [runtime]() {
      Microsoft::NodeApiJsi::HermesApi::current()->jsr_delete_runtime(runtime);
    });
  }};
}

} // namespace facebook::jsi

namespace facebook::jsi {
namespace {

class CApiMicrotaskTest : public ::testing::TestWithParam<int> {
 protected:
  void SetUp() override {
    auto *api = Microsoft::NodeApiJsi::HermesApi::current();
    jsr_config config{};
    ASSERT_EQ(api->jsr_create_config(&config), napi_ok);
    if (GetParam() >= 0) {
      EXPECT_EQ(api->jsr_config_set_explicit_microtasks(config, GetParam() != 0),
                napi_ok);
    }
    auto status = api->jsr_create_runtime(config, &runtime_);
    EXPECT_EQ(api->jsr_delete_config(config), napi_ok);
    ASSERT_EQ(status, napi_ok);
    napi_env env{};
    ASSERT_EQ(api->jsr_runtime_get_node_api_env(runtime_, &env), napi_ok);
    Microsoft::NodeApiJsi::NodeApiEnvScope envScope{env};
    rt_ = makeNodeApiJsiRuntime(env, api, [] {});
  }

  void TearDown() override {
    rt_.reset();
    if (runtime_) {
      EXPECT_EQ(Microsoft::NodeApiJsi::HermesApi::current()->jsr_delete_runtime(
                    runtime_),
                napi_ok);
    }
  }

  Value eval(const char *source) {
    return rt_->evaluateJavaScript(
        std::make_shared<StringBuffer>(source), "microtask-scheduling.js");
  }

  std::string events() {
    return eval("events.join(',')").getString(*rt_).utf8(*rt_);
  }

  Microsoft::NodeApiJsi::HermesApi::Scope apiScope_;
  jsr_runtime runtime_{};
  std::unique_ptr<Runtime> rt_;
};

TEST_P(CApiMicrotaskTest, PromiseJobsRunOnlyAtTheConfiguredCheckpoint) {
  const bool explicitMicrotasks = GetParam() == 1;
  EXPECT_EQ(eval("HermesInternal.useEngineQueue()").getBool(),
            explicitMicrotasks);
  eval(R"JS(
    var events = [], hostTasks = [];
    globalThis.setImmediate = function (job) { hostTasks.push(job); };
    Promise.resolve().then(function () {
      events.push('promise');
    }).then(function () { events.push('chained'); });
    (async function () { await 0; events.push('await'); })();
    events.push('script');
  )JS");
  EXPECT_EQ(events(), "script");
  EXPECT_EQ(eval("hostTasks.length > 0").getBool(), !explicitMicrotasks);
  EXPECT_TRUE(rt_->drainMicrotasks());
  if (explicitMicrotasks) {
    EXPECT_EQ(events(), "script,promise,await,chained");
    EXPECT_EQ(eval("hostTasks.length").getNumber(), 0);
  } else {
    // Draining the VM must not steal jobs scheduled on the bridge's task queue.
    EXPECT_EQ(events(), "script");
    eval("while (hostTasks.length) hostTasks.shift()();");
    EXPECT_EQ(events(), "script,promise,await,chained");
    EXPECT_TRUE(rt_->drainMicrotasks());
    EXPECT_EQ(events(), "script,promise,await,chained");
  }
}

// Exercise the C API's unchanged default as well as both explicit option values.
INSTANTIATE_TEST_CASE_P(
    SchedulingModes, CApiMicrotaskTest, ::testing::Values(-1, 0, 1));

} // namespace
} // namespace facebook::jsi
