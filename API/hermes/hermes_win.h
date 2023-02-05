/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifndef HERMES_HERMES_WIN_H
#define HERMES_HERMES_WIN_H

#include "hermes.h"

namespace facebook::hermes {

HERMES_EXPORT std::unique_ptr<HermesRuntime> __cdecl makeHermesRuntimeWithWER();
HERMES_EXPORT void __cdecl hermesCrashHandler(HermesRuntime &runtime, int fd);

} // namespace facebook::hermes

#if _M_IX86
#define HERMES_CDECL __cdecl
#else
#define HERMES_CDECL
#endif

#define HERMES_API HERMES_EXPORT hermes_status HERMES_CDECL

#ifdef __cplusplus
#define HERMES_EXTERN_C_BEGIN extern "C" {
#define HERMES_EXTERN_C_END }
#else
#define HERMES_EXTERN_C_BEGIN
#define HERMES_EXTERN_C_END
#endif

enum hermes_status {
  hermes_ok,
  hermes_error,
};

typedef struct hermes_runtime_s *hermes_runtime;

HERMES_EXTERN_C_BEGIN

HERMES_API hermes_create_runtime(hermes_runtime *runtime);
HERMES_API hermes_create_runtime_with_wer(hermes_runtime *runtime);
HERMES_API hermes_delete_runtime(hermes_runtime runtime);
HERMES_API hermes_get_non_abi_safe_runtime(
    hermes_runtime runtime,
    void **non_abi_safe_runtime);

HERMES_API hermes_dump_crash_data(hermes_runtime runtime, int32_t fd);

HERMES_API hermes_sampling_profiler_enable();
HERMES_API hermes_sampling_profiler_disable();
HERMES_API hermes_sampling_profiler_add(hermes_runtime runtime);
HERMES_API hermes_sampling_profiler_remove(hermes_runtime runtime);
HERMES_API hermes_sampling_profiler_dump_to_file(const char *filename);

HERMES_EXTERN_C_END

#endif // HERMES_HERMES_WIN_H
