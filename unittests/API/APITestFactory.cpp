/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <hermes/hermes.h>
#ifdef HERMES_HAS_SANDBOX
#include <hermes_sandbox/HermesSandboxRuntime.h>
#endif
#include <jsi/test/testlib.h>
#include <jsi/threadsafe.h>

using namespace facebook::hermes;

namespace facebook::jsi {

std::vector<RuntimeFactory> runtimeGenerators() {
  return {
      [] {
        return makeHermesRuntime(::hermes::vm::RuntimeConfig::Builder()
                                     .withMicrotaskQueue(true)
                                     .withEnableHermesInternalTestMethods(true)
                                     .build());
      },
      [] {
        return makeThreadSafeHermesRuntime(
            ::hermes::vm::RuntimeConfig::Builder()
                .withEnableHermesInternalTestMethods(true)
                .build());
      }};
}

/// The factories in runtimeGenerators(), plus HermesSandboxRuntime when the
/// sandbox is built.
std::vector<RuntimeFactory> runtimeGeneratorsWithSandBox() {
  auto factories = runtimeGenerators();
#ifdef HERMES_HAS_SANDBOX
  factories.push_back([] { return makeHermesSandboxRuntime(); });
#endif
  return factories;
}

} // namespace facebook::jsi
