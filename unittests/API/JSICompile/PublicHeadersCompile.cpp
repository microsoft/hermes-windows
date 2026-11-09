// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

// Catch inherited build definitions that would bypass the header fallback.
#if EXPECTED_JSI_VERSION == 21 && defined(JSI_VERSION)
#error The default JSI compile check must not define JSI_VERSION
#endif

#include <jsi/jsi.h>
#include <jsi/jsi-inl.h>
#include <jsi/decorator.h>
#include <jsi/instrumentation.h>

#include <type_traits>

static_assert(JSI_VERSION == EXPECTED_JSI_VERSION, "Wrong public JSI epoch");
static_assert(
    std::is_same<facebook::jsi::detail::RuntimeInterface,
                 facebook::jsi::Runtime>::value,
    "Legacy helpers must retain Runtime signatures");

namespace facebook {
namespace jsi {

/// Hooks used to instantiate every WithRuntimeDecorator forwarding body.
struct CompileHooks {
  void before() {}
  void after() {}
};

// Explicit instantiation checks template bodies, not just header parsing.
template class RuntimeDecorator<Runtime, Runtime>;
template class WithRuntimeDecorator<CompileHooks, Runtime, Runtime>;

static_assert(
    !std::is_abstract<RuntimeDecorator<>>::value,
    "The legacy decorator must implement every pure virtual method");
static_assert(
    !std::is_abstract<WithRuntimeDecorator<CompileHooks>>::value,
    "The legacy hooked decorator must implement every pure virtual method");

/// Instantiate inline array/property helpers against the selected epoch.
void compileLegacyHelpers(Runtime& runtime) {
  auto array = Array::createWithElements(runtime, 1, "value");
  array.setValueAtIndex(runtime, 0, 2);
  auto name = PropNameID::forAscii(runtime, "property");
  Object object(runtime);
  object.setProperty(runtime, name, array);
  (void)object.getProperty(runtime, name);
#if JSI_VERSION >= 21
  auto key = Value(String::createFromAscii(runtime, "property"));
  object.setProperty(runtime, key, true);
  (void)object.hasProperty(runtime, key);
  object.deleteProperty(runtime, key);
#endif
}

} // namespace jsi
} // namespace facebook
