// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "HermesApi.h"

namespace Microsoft::NodeApiJsi {

namespace {

struct HermesApiNames {
#define HERMES_FUNC(func) static constexpr const char func[] = #func;
#define HERMES_INSPECTOR_FUNC(func) HERMES_FUNC(func);
#include "HermesApi.inc"
};

// Load all inspector functions together to ensure their availability from different threads.
void loadInspectorFuncs() {
  HermesApi *current = HermesApi::current();
#define HERMES_INSPECTOR_FUNC(func)                                                                                  \
  decltype(::func) *loaded_##func = reinterpret_cast<decltype(::func) *>(current->getFuncPtr(HermesApiNames::func)); \
  size_t offset_##func = offsetof(HermesApi, func);                                                                  \
  *reinterpret_cast<decltype(::func) **>(reinterpret_cast<char *>(current) + offset_##func) = loaded_##func;
#include "HermesApi.inc"
}

} // namespace

thread_local HermesApi *HermesApi::current_{};

HermesApi::HermesApi(IFuncResolver *funcResolver)
    : JSRuntimeApi(funcResolver)
#define HERMES_FUNC(func) \
  , func(&ApiFuncResolver<HermesApi, decltype(::func) *, HermesApiNames::func, offsetof(HermesApi, func)>::stub)
#define HERMES_INSPECTOR_FUNC(func)                                                                           \
  ,                                                                                                           \
      func(&ApiFuncResolver<HermesApi, decltype(::func) *, HermesApiNames::func, offsetof(HermesApi, func)>:: \
               preloadStub<&loadInspectorFuncs>)

#include "HermesApi.inc"
{
}

HermesApi *HermesApi::fromLib() {
  static LibFuncResolver funcResolver("hermes");
  static HermesApi *libHermesApi = new HermesApi(&funcResolver);
  return libHermesApi;
}

} // namespace Microsoft::NodeApiJsi
