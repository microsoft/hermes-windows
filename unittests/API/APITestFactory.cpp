/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <hermes/hermes.h>
#include <hermes_abi/HermesABIRuntimeWrapper.h>
#include <hermes_abi/hermes_vtable.h>
#include <hermes_node_api_jsi/ApiLoaders/HermesApi.h>
#include <hermes_node_api_jsi/NodeApiJsiRuntime.h>
#include <hermes_sandbox/HermesSandboxRuntime.h>
#include <jsi/test/testlib.h>
#include <jsi/threadsafe.h>

using namespace facebook::hermes;

namespace facebook {
namespace jsi {

std::vector<RuntimeFactory> runtimeGenerators() {
  return {
      [] { return makeHermesRuntime(); },
      [] { return makeThreadSafeHermesRuntime(); },
      [] { return makeHermesABIRuntimeWrapper(get_hermes_abi_vtable()); },
      [] { return makeHermesSandboxRuntime(); },
      [] {
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
          Microsoft::NodeApiJsi::HermesApi::current()->jsr_delete_runtime(
              runtime);
        });
      }};
}

} // namespace jsi
} // namespace facebook
