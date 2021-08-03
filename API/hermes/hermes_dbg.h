/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifndef HERMES_HERMESEX_H
#define HERMES_HERMESEX_H

#include <memory>
#include <string>

#include <hermes/hermes.h>
#include <jsi/jsi.h>
#include <jsi/decorator.h>

using namespace facebook::jsi;

namespace facebook {
namespace hermes {

#ifndef NDEBUG
/*
 * Note: Hermes API uses STL containers (string, vector etc.) which don't have stable ABI.
 *
 * To consume Hermes in RNW, the safest approach is to match the build flags in RNW and Hermes build pipelines. It
 * implies the debug flavor of RNW will need to consume debug flavoured Hermes. It works, but the Javascript
 * interpretation by debug flavored Hermes in intolerably slow. We tried to make hermes debug binaries run faster
 * following the popular techniques such as disabling iterator debugging, but the performance improvements were not
 * sufficient.
 *
 * It is imperative that we use the release binaries of Hermes on Debug builds of RNW. Our long term plan is to
 * implement ABI-stable NAPI (https://nodejs.org/api/n-api.html) over Hermes VM. But, until we have a stable NAPI-Hermes
 * interface implementation, we are resorting to a pragmatic solution. Even though the solution is fragile, we want to
 * push it because (1) Hermes direct debugging adds huge value to the developer workflow . (2) The code is debug only,
 * hence don't run on user machines. (3) In case the developer runs into issues, there is a fallback.
 *
 * Solution:
 * We are building a debug friendlier release binary of hermes with checked iterators turned on (setting
 * _ITERATOR_DEBUG_LEVEL to '1' instead of the default value of '0'). Note that release builds can't be built with full
 * iterator debugging (_ITERATOR_DEBUG_LEVEL to '2'). Our observation is that containers created with debug-flavored STL
 * code (full iterator debugging enabled) can safely be consumed from STL code with checked iterators enabled. but not
 * vice-versa. i.e. The hermes binary built with checked iterators can consume STL containers created by debug flavored
 * RNW, but not vice versa. Luckily, there is only a small number of APIs in JSI which require transferring STL
 * containers from hermes to host. We added a small patch to hermes which enabled us to wrap thos STL containers for
 * safe access through this runtime proxy.
 */
struct RuntimeDebugFlavorProxy
    : public RuntimeDecorator<HermesRuntime, Runtime> {
 public:
  RuntimeDebugFlavorProxy(
      std::unique_ptr<HermesRuntime> plain)
      : RuntimeDecorator(*plain), plain_{std::move(plain)} {}
  std::string utf8(const PropNameID &id) override {
    auto r = plain().__utf8(id);
    return std::string(r->c_str());
  };

  std::string utf8(const String &id) override {
    auto r = plain().__utf8(id);
    return std::string(r->c_str());
  };

  std::string description() override {
    auto r = plain().__description();
    return std::string(r->c_str());
  };

 private:
  std::unique_ptr<facebook::hermes::HermesRuntime> plain_;
};
#endif

} // namespace hermes
} // namespace facebook

#endif
