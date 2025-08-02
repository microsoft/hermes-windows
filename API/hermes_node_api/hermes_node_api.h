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
#include "node_api/js_native_api.h"

namespace hermes::node_api {

class NodeApiEnvironment;

napi_status incEnvRefCount(napi_env env) noexcept;

napi_status decEnvRefCount(napi_env env) noexcept;

napi_status setNodeApiEnvironmentData(
    napi_env env,
    const napi_type_tag &tag,
    void *data) noexcept;

napi_status getNodeApiEnvironmentData(
    napi_env env,
    const napi_type_tag &tag,
    void **data) noexcept;

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
