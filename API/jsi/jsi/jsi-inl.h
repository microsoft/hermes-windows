/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

namespace facebook {
namespace jsi {
namespace detail {

inline Value toValue(detail::RuntimeInterface&, std::nullptr_t) {
  return Value::null();
}
inline Value toValue(detail::RuntimeInterface&, bool b) {
  return Value(b);
}
inline Value toValue(detail::RuntimeInterface&, double d) {
  return Value(d);
}
inline Value toValue(detail::RuntimeInterface&, float f) {
  return Value(static_cast<double>(f));
}
inline Value toValue(detail::RuntimeInterface&, int i) {
  return Value(i);
}
inline Value toValue(detail::RuntimeInterface& runtime, const char* str) {
  return String::createFromAscii(runtime, str);
}
inline Value toValue(detail::RuntimeInterface& runtime, const std::string& str) {
  return String::createFromUtf8(runtime, str);
}
template <typename T>
inline Value toValue(detail::RuntimeInterface& runtime, const T& other) {
  static_assert(
      std::is_base_of<Pointer, T>::value,
      "This type cannot be converted to Value");
  return Value(runtime, other);
}
inline Value toValue(detail::RuntimeInterface& runtime, const Value& value) {
  return Value(runtime, value);
}
inline Value&& toValue(detail::RuntimeInterface&, Value&& value) {
  return std::move(value);
}

inline PropNameID toPropNameID(detail::RuntimeInterface& runtime, const char* name) {
  return PropNameID::forAscii(runtime, name);
}
inline PropNameID toPropNameID(detail::RuntimeInterface& runtime, const std::string& name) {
  return PropNameID::forUtf8(runtime, name);
}
inline PropNameID&& toPropNameID(detail::RuntimeInterface&, PropNameID&& name) {
  return std::move(name);
}

/// Helper to throw while still compiling with exceptions turned off.
template <typename E, typename... Args>
[[noreturn]] inline void throwOrDie(Args&&... args) {
  std::rethrow_exception(
      std::make_exception_ptr(E{std::forward<Args>(args)...}));
}

} // namespace detail

template <typename T>
inline T Runtime::make(Runtime::PointerValue* pv) {
  return T(pv);
}

#if JSI_VERSION >= 3
inline Runtime::PointerValue* Runtime::getPointerValue(jsi::Pointer& pointer) {
  return pointer.ptr_;
}
#endif

inline const Runtime::PointerValue* Runtime::getPointerValue(
    const jsi::Pointer& pointer) {
  return pointer.ptr_;
}

inline const Runtime::PointerValue* Runtime::getPointerValue(
    const jsi::Value& value) {
  return value.data_.pointer.ptr_;
}

#if JSI_VERSION >= 20
inline void Runtime::setRuntimeData(
    const UUID& dataUUID,
    const std::shared_ptr<void>& data) {
  auto* dataPtr = new std::shared_ptr<void>(data);
  setRuntimeDataImpl(dataUUID, dataPtr, [](const void* data) {
    delete (const std::shared_ptr<void>*)data;
  });
}

inline std::shared_ptr<void> Runtime::getRuntimeData(const UUID& dataUUID) {
  auto* data = (const std::shared_ptr<void>*)getRuntimeDataImpl(dataUUID);
  return data ? *data : nullptr;
}
#endif

#if JSI_VERSION >= 17
Value Object::getPrototype(detail::RuntimeInterface& runtime) const {
  return runtime.getPrototypeOf(*this);
}
#endif

inline Value Object::getProperty(detail::RuntimeInterface& runtime, const char* name) const {
  return getProperty(runtime, String::createFromAscii(runtime, name));
}

inline Value Object::getProperty(detail::RuntimeInterface& runtime, const String& name) const {
  return runtime.getProperty(*this, name);
}

inline Value Object::getProperty(detail::RuntimeInterface& runtime, const PropNameID& name)
    const {
  return runtime.getProperty(*this, name);
}

#if JSI_VERSION >= 21
inline Value Object::getProperty(detail::RuntimeInterface& runtime, const Value& name) const {
  return runtime.getProperty(*this, name);
}
#endif

inline bool Object::hasProperty(detail::RuntimeInterface& runtime, const char* name) const {
  return hasProperty(runtime, String::createFromAscii(runtime, name));
}

inline bool Object::hasProperty(detail::RuntimeInterface& runtime, const String& name) const {
  return runtime.hasProperty(*this, name);
}

inline bool Object::hasProperty(detail::RuntimeInterface& runtime, const PropNameID& name)
    const {
  return runtime.hasProperty(*this, name);
}

#if JSI_VERSION >= 21
inline bool Object::hasProperty(detail::RuntimeInterface& runtime, const Value& name) const {
  return runtime.hasProperty(*this, name);
}
#endif

template <typename T>
void Object::setProperty(detail::RuntimeInterface& runtime, const char* name, T&& value)
    JSI_CONST_10 {
  setProperty(
      runtime, String::createFromAscii(runtime, name), std::forward<T>(value));
}

template <typename T>
void Object::setProperty(detail::RuntimeInterface& runtime, const String& name, T&& value)
    JSI_CONST_10 {
  setPropertyValue(
      runtime, name, detail::toValue(runtime, std::forward<T>(value)));
}

template <typename T>
void Object::setProperty(detail::RuntimeInterface& runtime, const PropNameID& name, T&& value)
    JSI_CONST_10 {
  setPropertyValue(
      runtime, name, detail::toValue(runtime, std::forward<T>(value)));
}

#if JSI_VERSION >= 21
template <typename T>
void Object::setProperty(detail::RuntimeInterface& runtime, const Value& name, T&& value)
    const {
  setPropertyValue(
      runtime, name, detail::toValue(runtime, std::forward<T>(value)));
}

inline void Object::deleteProperty(detail::RuntimeInterface& runtime, const char* name) const {
  deleteProperty(runtime, String::createFromAscii(runtime, name));
}

inline void Object::deleteProperty(detail::RuntimeInterface& runtime, const String& name)
    const {
  runtime.deleteProperty(*this, name);
}

inline void Object::deleteProperty(detail::RuntimeInterface& runtime, const PropNameID& name)
    const {
  runtime.deleteProperty(*this, name);
}

#if JSI_VERSION >= 21
inline void Object::deleteProperty(detail::RuntimeInterface& runtime, const Value& name) const {
  runtime.deleteProperty(*this, name);
}
#endif
#endif

inline Array Object::getArray(detail::RuntimeInterface& runtime) const& {
  assert(runtime.isArray(*this));
  (void)runtime; // when assert is disabled we need to mark this as used
  return Array(runtime.cloneObject(ptr_));
}

inline Array Object::getArray(detail::RuntimeInterface& runtime) && {
  assert(runtime.isArray(*this));
  (void)runtime; // when assert is disabled we need to mark this as used
  Runtime::PointerValue* value = ptr_;
  ptr_ = nullptr;
  return Array(value);
}

inline ArrayBuffer Object::getArrayBuffer(detail::RuntimeInterface& runtime) const& {
  assert(runtime.isArrayBuffer(*this));
  (void)runtime; // when assert is disabled we need to mark this as used
  return ArrayBuffer(runtime.cloneObject(ptr_));
}

inline ArrayBuffer Object::getArrayBuffer(detail::RuntimeInterface& runtime) && {
  assert(runtime.isArrayBuffer(*this));
  (void)runtime; // when assert is disabled we need to mark this as used
  Runtime::PointerValue* value = ptr_;
  ptr_ = nullptr;
  return ArrayBuffer(value);
}

#if JSI_VERSION >= 22
inline TypedArray Object::getTypedArray(detail::RuntimeInterface& runtime) const& {
  assert(runtime.isTypedArray(*this));
  return TypedArray(runtime.cloneObject(ptr_));
}
#endif

#if JSI_VERSION >= 22
inline Uint8Array Object::getUint8Array(detail::RuntimeInterface& runtime) const& {
  assert(runtime.isUint8Array(*this));
  return Uint8Array(runtime.cloneObject(ptr_));
}
#endif

inline Function Object::getFunction(detail::RuntimeInterface& runtime) const& {
  assert(runtime.isFunction(*this));
  return Function(runtime.cloneObject(ptr_));
}

inline Function Object::getFunction(detail::RuntimeInterface& runtime) && {
  assert(runtime.isFunction(*this));
  (void)runtime; // when assert is disabled we need to mark this as used
  Runtime::PointerValue* value = ptr_;
  ptr_ = nullptr;
  return Function(value);
}

template <typename T>
inline bool Object::isHostObject(detail::RuntimeInterface& runtime) const {
  return runtime.isHostObject(*this) &&
      std::dynamic_pointer_cast<T>(runtime.getHostObject(*this));
}

template <>
inline bool Object::isHostObject<HostObject>(detail::RuntimeInterface& runtime) const {
  return runtime.isHostObject(*this);
}

template <typename T>
inline std::shared_ptr<T> Object::getHostObject(detail::RuntimeInterface& runtime) const {
  assert(isHostObject<T>(runtime));
  return std::static_pointer_cast<T>(runtime.getHostObject(*this));
}

template <typename T>
inline std::shared_ptr<T> Object::asHostObject(detail::RuntimeInterface& runtime) const {
  if (!isHostObject<T>(runtime)) {
    detail::throwOrDie<JSINativeException>(
        "Object is not a HostObject of desired type");
  }
  return std::static_pointer_cast<T>(runtime.getHostObject(*this));
}

template <>
inline std::shared_ptr<HostObject> Object::getHostObject<HostObject>(
    detail::RuntimeInterface& runtime) const {
  assert(runtime.isHostObject(*this));
  return runtime.getHostObject(*this);
}

#if JSI_VERSION >= 7
template <typename T>
inline bool Object::hasNativeState(detail::RuntimeInterface& runtime) const {
  return runtime.hasNativeState(*this) &&
      std::dynamic_pointer_cast<T>(runtime.getNativeState(*this));
}

template <>
inline bool Object::hasNativeState<NativeState>(detail::RuntimeInterface& runtime) const {
  return runtime.hasNativeState(*this);
}

template <typename T>
inline std::shared_ptr<T> Object::getNativeState(detail::RuntimeInterface& runtime) const {
  assert(hasNativeState<T>(runtime));
  return std::static_pointer_cast<T>(runtime.getNativeState(*this));
}

inline void Object::setNativeState(
    detail::RuntimeInterface& runtime,
    std::shared_ptr<NativeState> state) const {
  runtime.setNativeState(*this, state);
}
#endif

#if JSI_VERSION >= 11
inline void Object::setExternalMemoryPressure(detail::RuntimeInterface& runtime, size_t amt)
    const {
  runtime.setExternalMemoryPressure(*this, amt);
}
#endif

inline Array Object::getPropertyNames(detail::RuntimeInterface& runtime) const {
  return runtime.getPropertyNames(*this);
}

inline Value WeakObject::lock(detail::RuntimeInterface& runtime) JSI_CONST_10 {
  return runtime.lockWeakObject(*this);
}

template <typename T>
void Array::setValueAtIndex(detail::RuntimeInterface& runtime, size_t i, T&& value)
    JSI_CONST_10 {
  setValueAtIndexImpl(
      runtime, i, detail::toValue(runtime, std::forward<T>(value)));
}

inline Value Array::getValueAtIndex(detail::RuntimeInterface& runtime, size_t i) const {
  return runtime.getValueAtIndex(*this, i);
}

inline Function Function::createFromHostFunction(
    detail::RuntimeInterface& runtime,
    const jsi::PropNameID& name,
    unsigned int paramCount,
    jsi::HostFunctionType func) {
  return runtime.createFunctionFromHostFunction(
      name, paramCount, std::move(func));
}

inline Value Function::call(detail::RuntimeInterface& runtime, const Value* args, size_t count)
    const {
  return runtime.call(*this, Value::undefined(), args, count);
}

inline Value Function::call(
    detail::RuntimeInterface& runtime,
    std::initializer_list<Value> args) const {
  return call(runtime, args.begin(), args.size());
}

template <typename... Args>
inline Value Function::call(detail::RuntimeInterface& runtime, Args&&... args) const {
  // A more awesome version of this would be able to create raw values
  // which can be used directly without wrapping and unwrapping, but
  // this will do for now.
  return call(runtime, {detail::toValue(runtime, std::forward<Args>(args))...});
}

inline Value Function::callWithThis(
    detail::RuntimeInterface& runtime,
    const Object& jsThis,
    const Value* args,
    size_t count) const {
  return runtime.call(*this, Value(runtime, jsThis), args, count);
}

inline Value Function::callWithThis(
    detail::RuntimeInterface& runtime,
    const Object& jsThis,
    std::initializer_list<Value> args) const {
  return callWithThis(runtime, jsThis, args.begin(), args.size());
}

template <typename... Args>
inline Value Function::callWithThis(
    detail::RuntimeInterface& runtime,
    const Object& jsThis,
    Args&&... args) const {
  // A more awesome version of this would be able to create raw values
  // which can be used directly without wrapping and unwrapping, but
  // this will do for now.
  return callWithThis(
      runtime, jsThis, {detail::toValue(runtime, std::forward<Args>(args))...});
}

template <typename... Args>
inline Array Array::createWithElements(detail::RuntimeInterface& runtime, Args&&... args) {
  return createWithElements(
      runtime, {detail::toValue(runtime, std::forward<Args>(args))...});
}

#if JSI_VERSION >= 22
template <typename... Args>
inline size_t Array::push(detail::RuntimeInterface& runtime, Args&&... args) {
  return push(runtime, {detail::toValue(runtime, std::forward<Args>(args))...});
}

inline size_t Array::push(
    detail::RuntimeInterface& runtime,
    std::initializer_list<Value> elements) {
  return push(runtime, elements.begin(), elements.size());
}

inline size_t
Array::push(detail::RuntimeInterface& runtime, const Value* elements, size_t count) {
  return runtime.push(*this, elements, count);
}
#endif


template <typename... Args>
inline std::vector<PropNameID> PropNameID::names(
    detail::RuntimeInterface& runtime,
    Args&&... args) {
  return names({detail::toPropNameID(runtime, std::forward<Args>(args))...});
}

template <size_t N>
inline std::vector<PropNameID> PropNameID::names(
    PropNameID (&&propertyNames)[N]) {
  std::vector<PropNameID> result;
  result.reserve(N);
  for (auto& name : propertyNames) {
    result.push_back(std::move(name));
  }
  return result;
}

inline Value Function::callAsConstructor(
    detail::RuntimeInterface& runtime,
    const Value* args,
    size_t count) const {
  return runtime.callAsConstructor(*this, args, count);
}

inline Value Function::callAsConstructor(
    detail::RuntimeInterface& runtime,
    std::initializer_list<Value> args) const {
  return callAsConstructor(runtime, args.begin(), args.size());
}

template <typename... Args>
inline Value Function::callAsConstructor(detail::RuntimeInterface& runtime, Args&&... args)
    const {
  return callAsConstructor(
      runtime, {detail::toValue(runtime, std::forward<Args>(args))...});
}

#if JSI_VERSION >= 8
String BigInt::toString(detail::RuntimeInterface& runtime, int radix) const {
  return runtime.bigintToString(*this, radix);
}
#endif

} // namespace jsi
} // namespace facebook
