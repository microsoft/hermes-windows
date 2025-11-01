// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "HermesApi.h"

// Define cross-platform pragma macros for suppressing offsetof warnings
#ifdef __clang__
#define HERMES_PRAGMA_PUSH_OFFSETOF \
  _Pragma("GCC diagnostic push")    \
      _Pragma("GCC diagnostic ignored \"-Winvalid-offsetof\"")
#define HERMES_PRAGMA_POP_OFFSETOF _Pragma("GCC diagnostic pop")
#else
#define HERMES_PRAGMA_PUSH_OFFSETOF \
  __pragma(warning(push)) __pragma(warning(disable : 4117))
#define HERMES_PRAGMA_POP_OFFSETOF __pragma(warning(pop))
#endif

namespace Microsoft::NodeApiJsi {

namespace {

struct HermesApiNames {
#define HERMES_FUNC(func) static constexpr const char func[] = #func;
#define HERMES_INSPECTOR_FUNC(func) HERMES_FUNC(func);
#include "HermesApi.inc"
};

// Load all inspector functions together to ensure their availability from
// different threads.
void loadInspectorFuncs() {
  HermesApi *current = HermesApi::current();
#define HERMES_INSPECTOR_FUNC(func)                                       \
  HERMES_PRAGMA_PUSH_OFFSETOF                                             \
  decltype(::func) *loaded_##func = reinterpret_cast<decltype(::func) *>( \
      current->getFuncPtr(HermesApiNames::func));                         \
  size_t offset_##func = offsetof(HermesApi, func);                       \
  *reinterpret_cast<decltype(::func) **>(                                 \
      reinterpret_cast<char *>(current) + offset_##func) = loaded_##func; \
  HERMES_PRAGMA_POP_OFFSETOF
#include "HermesApi.inc"
}

} // namespace

thread_local HermesApi *HermesApi::current_{};

HermesApi::HermesApi(IFuncResolver *funcResolver)
    : JSRuntimeApi(funcResolver)
#define HERMES_FUNC(func)        \
  HERMES_PRAGMA_PUSH_OFFSETOF    \
  ,                              \
      func(&ApiFuncResolver<     \
           HermesApi,            \
           decltype(::func) *,   \
           HermesApiNames::func, \
           offsetof(HermesApi, func)>::stub) HERMES_PRAGMA_POP_OFFSETOF
#define HERMES_INSPECTOR_FUNC(func)                                      \
  HERMES_PRAGMA_PUSH_OFFSETOF                                            \
  ,                                                                      \
      func(&ApiFuncResolver<                                             \
           HermesApi,                                                    \
           decltype(::func) *,                                           \
           HermesApiNames::func,                                         \
           offsetof(HermesApi, func)>::preloadStub<&loadInspectorFuncs>) \
          HERMES_PRAGMA_POP_OFFSETOF

#include "HermesApi.inc"
{
}

HermesApi *HermesApi::fromLib() {
  static LibFuncResolver funcResolver("hermes");
  static HermesApi *libHermesApi = new HermesApi(&funcResolver);
  return libHermesApi;
}

} // namespace Microsoft::NodeApiJsi
