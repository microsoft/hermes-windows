// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once
#ifndef APILOADERS_HERMESAPI_H_
#define APILOADERS_HERMESAPI_H_

#include <hermes_shared/hermes_api.h>
#include "JSRuntimeApi.h"

namespace Microsoft::NodeApiJsi {

class HermesApi : public JSRuntimeApi {
 public:
  HermesApi(IFuncResolver *funcResolver);

  static HermesApi *current() noexcept {
    return current_;
  }

  static void setCurrent(HermesApi *current) noexcept {
    JSRuntimeApi::setCurrent(current);
    current_ = current;
  }

  static HermesApi *fromLib();

  class Scope : public JSRuntimeApi::Scope {
   public:
    Scope() : Scope(HermesApi::fromLib()) {}

    Scope(HermesApi *hermesApi) : JSRuntimeApi::Scope(hermesApi), prevHermesApi_(HermesApi::current_) {
      HermesApi::current_ = hermesApi;
    }

    ~Scope() {
      HermesApi::current_ = prevHermesApi_;
    }

   private:
    HermesApi *prevHermesApi_;
  };

 public:
#define HERMES_FUNC(func) decltype(::func) *const func;
#define HERMES_INSPECTOR_FUNC(func) HERMES_FUNC(func)
#include "HermesApi.inc"

 private:
  static thread_local HermesApi *current_;
};

} // namespace Microsoft::NodeApiJsi

#endif // !APILOADERS_HERMESAPI_H_
