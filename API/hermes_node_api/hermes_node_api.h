/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * Copyright (c) Microsoft Corporation.
 * Licensed under the MIT license.
 */

#ifndef HERMES_NODE_API_H
#define HERMES_NODE_API_H

#include <memory>
#include <string>
#include "hermes/BCGen/HBC/BytecodeProviderFromSrc.h"
#include "hermes/VM/RuntimeModule.h"
#include "node_api/node_api.h"

namespace hermes::node_api {

class NodeApiEnvironment;

// A task to execute by TaskRunner.
class Task {
 public:
  virtual ~Task() = default;
  virtual void invoke() noexcept = 0;
};

// The TaskRunner interface to schedule tasks in JavaScript thread.
class TaskRunner {
 public:
  virtual ~TaskRunner() = default;
  virtual void post(std::unique_ptr<Task> task) noexcept = 0;
};

// Get or create a Node API environment associated with the given Hermes
// runtime. The Node API environment is deleted by the runtime destructor.
vm::CallResult<napi_env> getOrCreateNodeApiEnvironment(
    vm::Runtime &runtime,
    hbc::CompileFlags compileFlags,
    std::shared_ptr<TaskRunner> taskRunner,
    const std::function<void(napi_env, napi_value)> &unhandledErrorCallback,
    int32_t apiVersion) noexcept;

// Initialize new Node API module in a new Node API environment.
napi_status initializeNodeApiModule(
    vm::Runtime &runtime,
    napi_addon_register_func registerModule,
    int32_t apiVersion,
    napi_value *exports) noexcept;

napi_status runScript(
    napi_env env,
    const char *script,
    size_t script_length,
    const char *filename,
    size_t filename_length,
    napi_value *result) noexcept;

napi_status setNodeApiEnvironmentData(
    napi_env env,
    const napi_type_tag &tag,
    void *data) noexcept;

napi_status getNodeApiEnvironmentData(
    napi_env env,
    const napi_type_tag &tag,
    void **data) noexcept;

napi_status checkNodeApiPreconditions(napi_env env) noexcept;

napi_status setNodeApiValue(
    napi_env env,
    ::hermes::vm::CallResult<::hermes::vm::HermesValue> hvResult,
    napi_value *result);

napi_status checkJSErrorStatus(
    napi_env env,
    vm::ExecutionStatus hermesStatus) noexcept;

napi_status queueMicrotask(napi_env env, napi_value callback) noexcept;

napi_status collectGarbage(napi_env env) noexcept;

napi_status hasUnhandledPromiseRejection(napi_env env, bool *result) noexcept;

napi_status getAndClearLastUnhandledPromiseRejection(
    napi_env env,
    napi_value *result) noexcept;

napi_status runBytecode(
    napi_env env,
    std::shared_ptr<hbc::BCProvider> bytecodeProvider,
    vm::RuntimeModuleFlags runtimeFlags,
    const std::string &sourceURL,
    napi_value *result) noexcept;

template <class... TArgs>
napi_status setLastNativeError(
    napi_env env,
    napi_status status,
    const char *fileName,
    uint32_t line,
    TArgs &&...args) noexcept {
  std::ostringstream sb;
  (sb << ... << args);
  const std::string message = sb.str();
  return setLastNativeError(env, status, fileName, line, message);
}

template <class... TArgs>
napi_status setLastNativeError(
    NodeApiEnvironment &env,
    napi_status status,
    const char *fileName,
    uint32_t line,
    TArgs &&...args) noexcept {
  std::ostringstream sb;
  (sb << ... << args);
  const std::string message = sb.str();
  return setLastNativeError(env, status, fileName, line, message);
}

template <>
napi_status setLastNativeError(
    napi_env env,
    napi_status status,
    const char *fileName,
    uint32_t line,
    const std::string &message) noexcept;

template <>
napi_status setLastNativeError(
    NodeApiEnvironment &env,
    napi_status status,
    const char *fileName,
    uint32_t line,
    const std::string &message) noexcept;

napi_status clearLastNativeError(napi_env env) noexcept;

} // namespace hermes::node_api

#endif // HERMES_NODE_API_H
