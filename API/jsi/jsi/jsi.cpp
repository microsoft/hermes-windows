/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <map>
#include <mutex>
#include <stdexcept>

#include <jsi/instrumentation.h>
#include <jsi/jsi.h>

namespace facebook {
namespace jsi {

namespace {

#if JSI_VERSION >= 20
/// A global map used to store custom runtime data for VMs that do not provide
/// their own default implementation of setRuntimeData and getRuntimeData.
struct RuntimeDataGlobal {
  /// Mutex protecting the Runtime data map
  std::mutex mutex_{};
  /// Maps a runtime pointer to a map of its custom data. At destruction of the
  /// runtime, its entry will be removed from the global map.
  std::unordered_map<
      Runtime*,
      std::unordered_map<
          UUID,
          std::pair<const void*, void (*)(const void* data)>,
          UUID::Hash>>
      dataMap_;
};

RuntimeDataGlobal& getRuntimeDataGlobal() {
  static RuntimeDataGlobal runtimeData{};
  return runtimeData;
}

/// A host object that, when destructed, will remove the runtime's custom data
/// entry from the global map of custom data.
class RemoveRuntimeDataHostObject : public jsi::HostObject {
 public:
  explicit RemoveRuntimeDataHostObject(Runtime* runtime) : runtime_(runtime) {}

  RemoveRuntimeDataHostObject(const RemoveRuntimeDataHostObject&) = default;
  RemoveRuntimeDataHostObject(RemoveRuntimeDataHostObject&&) = default;
  RemoveRuntimeDataHostObject& operator=(const RemoveRuntimeDataHostObject&) =
      default;
  RemoveRuntimeDataHostObject& operator=(RemoveRuntimeDataHostObject&&) =
      default;

  ~RemoveRuntimeDataHostObject() override {
    auto& runtimeDataGlobal = getRuntimeDataGlobal();
    std::lock_guard<std::mutex> lock(runtimeDataGlobal.mutex_);
    auto runtimeMapIt = runtimeDataGlobal.dataMap_.find(runtime_);
    // We install the RemoveRuntimeDataHostObject only when the first custom
    // data for the runtime is added, and only this object is responsible for
    // clearing runtime data. Thus, we should always be able to find the data
    // entry.
    assert(
        runtimeMapIt != runtimeDataGlobal.dataMap_.end() &&
        "Custom runtime data not found for this runtime");

    for (auto [_, entry] : runtimeMapIt->second) {
      auto* deleter = entry.second;
      deleter(entry.first);
    }
    runtimeDataGlobal.dataMap_.erase(runtime_);
  }

 private:
  Runtime* runtime_;
};
#endif

// This is used for generating short exception strings.
std::string kindToString(const Value& v, Runtime* rt = nullptr) {
  if (v.isUndefined()) {
    return "undefined";
  } else if (v.isNull()) {
    return "null";
  } else if (v.isBool()) {
    return v.getBool() ? "true" : "false";
  } else if (v.isNumber()) {
    return "a number";
  } else if (v.isString()) {
    return "a string";
  } else if (v.isSymbol()) {
    return "a symbol";
#if JSI_VERSION >= 6
  } else if (v.isBigInt()) {
    return "a bigint";
#endif
  } else {
    assert(v.isObject() && "Expecting object.");
    return rt != nullptr && v.getObject(*rt).isFunction(*rt) ? "a function"
                                                             : "an object";
  }
}

// getPropertyAsFunction() will try to create a JSError.  If the
// failure is in building a JSError, this will lead to infinite
// recursion.  This function is used in place of getPropertyAsFunction
// when building JSError, to avoid that infinite recursion.
Value callGlobalFunction(Runtime& runtime, const char* name, const Value& arg) {
  Value v = runtime.global().getProperty(runtime, name);
  if (!v.isObject()) {
    throw JSINativeException(
        std::string("callGlobalFunction: JS global property '") + name +
        "' is " + kindToString(v, &runtime) + ", expected a Function");
  }
  Object o = v.getObject(runtime);
  if (!o.isFunction(runtime)) {
    throw JSINativeException(
        std::string("callGlobalFunction: JS global property '") + name +
        "' is a non-callable Object, expected a Function");
  }
  Function f = std::move(o).getFunction(runtime);
  return f.call(runtime, arg);
}

#if JSI_VERSION >= 14
// Given a sequence of UTF8 encoded bytes, advance the input to past where a
// 32-bit unicode codepoint as been decoded and return the codepoint. If the
// UTF8 encoding is invalid, then return the value with the unicode replacement
// character (U+FFFD). This decoder also relies on zero termination at end of
// the input for bound checks.
// \param input char pointer pointing to the current character
// \return Unicode codepoint
uint32_t decodeUTF8(const char*& input) {
  uint32_t ch = (unsigned char)input[0];
  if (ch <= 0x7f) {
    input += 1;
    return ch;
  }
  uint32_t ret;
  constexpr uint32_t replacementCharacter = 0xFFFD;
  if ((ch & 0xE0) == 0xC0) {
    uint32_t ch1 = (unsigned char)input[1];
    if ((ch1 & 0xC0) != 0x80) {
      input += 1;
      return replacementCharacter;
    }
    ret = ((ch & 0x1F) << 6) | (ch1 & 0x3F);
    input += 2;
    if (ret <= 0x7F) {
      return replacementCharacter;
    }
  } else if ((ch & 0xF0) == 0xE0) {
    uint32_t ch1 = (unsigned char)input[1];
    if ((ch1 & 0x40) != 0 || (ch1 & 0x80) == 0) {
      input += 1;
      return replacementCharacter;
    }
    uint32_t ch2 = (unsigned char)input[2];
    if ((ch2 & 0x40) != 0 || (ch2 & 0x80) == 0) {
      input += 2;
      return replacementCharacter;
    }
    ret = ((ch & 0x0F) << 12) | ((ch1 & 0x3F) << 6) | (ch2 & 0x3F);
    input += 3;
    if (ret <= 0x7FF) {
      return replacementCharacter;
    }
  } else if ((ch & 0xF8) == 0xF0) {
    uint32_t ch1 = (unsigned char)input[1];
    if ((ch1 & 0x40) != 0 || (ch1 & 0x80) == 0) {
      input += 1;
      return replacementCharacter;
    }
    uint32_t ch2 = (unsigned char)input[2];
    if ((ch2 & 0x40) != 0 || (ch2 & 0x80) == 0) {
      input += 2;
      return replacementCharacter;
    }
    uint32_t ch3 = (unsigned char)input[3];
    if ((ch3 & 0x40) != 0 || (ch3 & 0x80) == 0) {
      input += 3;
      return replacementCharacter;
    }
    ret = ((ch & 0x07) << 18) | ((ch1 & 0x3F) << 12) | ((ch2 & 0x3F) << 6) |
        (ch3 & 0x3F);
    input += 4;
    if (ret <= 0xFFFF) {
      return replacementCharacter;
    }
    if (ret > 0x10FFFF) {
      return replacementCharacter;
    }
  } else {
    input += 1;
    return replacementCharacter;
  }
  return ret;
}

// Given a valid 32-bit unicode codepoint, encode it as UTF-16 into the output.
void encodeUTF16(std::u16string& out, uint32_t cp) {
  if (cp < 0x10000) {
    out.push_back((uint16_t)cp);
    return;
  }
  cp -= 0x10000;
  uint16_t highSurrogate = 0xD800 + ((cp >> 10) & 0x3FF);
  out.push_back(highSurrogate);
  uint16_t lowSurrogate = 0xDC00 + (cp & 0x3FF);
  out.push_back(lowSurrogate);
}

// Convert the UTF8 encoded string into a UTF16 encoded string. If the
// input is not valid UTF8, the replacement character (U+FFFD) is used to
// represent the invalid sequence.
std::u16string convertUTF8ToUTF16(const std::string& utf8) {
  std::u16string ret;
  const char* curr = utf8.data();
  const char* end = curr + utf8.length();
  while (curr < end) {
    auto cp = decodeUTF8(curr);
    encodeUTF16(ret, cp);
  }
  return ret;
}
#endif

#if JSI_VERSION >= 19
// Given a unsigned number, which is less than 16, return the hex character.
inline char hexDigit(unsigned x) {
  return static_cast<char>(x < 10 ? '0' + x : 'A' + (x - 10));
}

// Given a sequence of UTF 16 code units, return true if all code units are
// ASCII characters
bool isAllASCII(const char16_t* utf16, size_t length) {
  for (const char16_t* e = utf16 + length; utf16 != e; ++utf16) {
    if (*utf16 > 0x7F)
      return false;
  }
  return true;
}

// Given a sequences of UTF 16 code units, return a string that explicitly
// expresses the code units
std::string getUtf16CodeUnitString(const char16_t* utf16, size_t length) {
  // Every character will need 4 hex digits + the character escape "\u".
  // Plus 2 character for the opening and closing single quote.
  std::string s = std::string(6 * length + 2, 0);
  s.front() = '\'';

  for (size_t i = 0; i != length; ++i) {
    char16_t ch = utf16[i];
    size_t start = (6 * i) + 1;

    s[start] = '\\';
    s[start + 1] = 'u';

    s[start + 2] = hexDigit((ch >> 12) & 0x000f);
    s[start + 3] = hexDigit((ch >> 8) & 0x000f);
    s[start + 4] = hexDigit((ch >> 4) & 0x000f);
    s[start + 5] = hexDigit(ch & 0x000f);
  }
  s.back() = '\'';
  return s;
}
#endif

} // namespace

Buffer::~Buffer() = default;

#if JSI_VERSION >= 9
MutableBuffer::~MutableBuffer() = default;
#endif

PreparedJavaScript::~PreparedJavaScript() = default;

Value HostObject::get(Runtime&, const PropNameID&) {
  return Value();
}

void HostObject::set(Runtime& rt, const PropNameID& name, const Value&) {
  std::string msg("TypeError: Cannot assign to property '");
  msg += name.utf8(rt);
  msg += "' on HostObject with default setter";
  throw JSError(rt, msg);
}

HostObject::~HostObject() {}

#if JSI_VERSION >= 7
NativeState::~NativeState() {}
#endif

Runtime::~Runtime() {}

#if JSI_VERSION >= 20
ICast* Runtime::castInterface(const UUID& /*interfaceUUID*/) {
  return nullptr;
}
#endif

Instrumentation& Runtime::instrumentation() {
  class NoInstrumentation : public Instrumentation {
    std::string getRecordedGCStats() override {
      return "";
    }

    std::unordered_map<std::string, int64_t> getHeapInfo(bool) override {
      return std::unordered_map<std::string, int64_t>{};
    }

    void collectGarbage(std::string) override {}

    void startTrackingHeapObjectStackTraces(
        std::function<void(
            uint64_t,
            std::chrono::microseconds,
            std::vector<HeapStatsUpdate>)>) override {}
    void stopTrackingHeapObjectStackTraces() override {}

    void startHeapSampling(size_t) override {}
    void stopHeapSampling(std::ostream&) override {}

#if JSI_VERSION >= 13
    void createSnapshotToFile(
        const std::string& /*path*/,
        const HeapSnapshotOptions& /*options*/) override
#else
    void createSnapshotToFile(const std::string&) override
#endif
    {
      throw JSINativeException(
          "Default instrumentation cannot create a heap snapshot");
    }

#if JSI_VERSION >= 13
    void createSnapshotToStream(
        std::ostream& /*os*/,
        const HeapSnapshotOptions& /*options*/) override
#else
    void createSnapshotToStream(std::ostream&) override
#endif
    {
      throw JSINativeException(
          "Default instrumentation cannot create a heap snapshot");
    }

    std::string flushAndDisableBridgeTrafficTrace() override {
      std::abort();
    }

    void writeBasicBlockProfileTraceToFile(const std::string&) const override {
      std::abort();
    }

    void dumpProfilerSymbolsToFile(const std::string&) const override {
      std::abort();
    }
  };

  static NoInstrumentation sharedInstance;
  return sharedInstance;
}

#if JSI_VERSION >= 2
Value Runtime::createValueFromJsonUtf8(const uint8_t* json, size_t length) {
  Function parseJson = global()
                           .getPropertyAsObject(*this, "JSON")
                           .getPropertyAsFunction(*this, "parse");
  return parseJson.call(*this, String::createFromUtf8(*this, json, length));
}
#else
Value Value::createFromJsonUtf8(
    Runtime& runtime,
    const uint8_t* json,
    size_t length) {
  Function parseJson = runtime.global()
                           .getPropertyAsObject(runtime, "JSON")
                           .getPropertyAsFunction(runtime, "parse");
  return parseJson.call(runtime, String::createFromUtf8(runtime, json, length));
}
#endif

#if JSI_VERSION >= 19
String Runtime::createStringFromUtf16(const char16_t* utf16, size_t length) {
  if (isAllASCII(utf16, length)) {
    std::string buffer(length, '\0');
    for (size_t i = 0; i < length; ++i) {
      buffer[i] = static_cast<char>(utf16[i]);
    }
    return createStringFromAscii(buffer.data(), length);
  }
  auto s = getUtf16CodeUnitString(utf16, length);
  return global()
      .getPropertyAsFunction(*this, "eval")
      .call(*this, s)
      .getString(*this);
}

PropNameID Runtime::createPropNameIDFromUtf16(
    const char16_t* utf16,
    size_t length) {
  auto jsString = createStringFromUtf16(utf16, length);
  return createPropNameIDFromString(jsString);
}
#endif

#if JSI_VERSION >= 14
std::u16string Runtime::utf16(const PropNameID& sym) {
  auto utf8Str = utf8(sym);
  return convertUTF8ToUTF16(utf8Str);
}

std::u16string Runtime::utf16(const String& str) {
  auto utf8Str = utf8(str);
  return convertUTF8ToUTF16(utf8Str);
}
#endif

#if JSI_VERSION >= 16
void Runtime::getStringData(
    const jsi::String& str,
    void* ctx,
    void (*cb)(void* ctx, bool ascii, const void* data, size_t num)) {
  auto utf16Str = utf16(str);
  cb(ctx, false, utf16Str.data(), utf16Str.size());
}

void Runtime::getPropNameIdData(
    const jsi::PropNameID& sym,
    void* ctx,
    void (*cb)(void* ctx, bool ascii, const void* data, size_t num)) {
  auto utf16Str = utf16(sym);
  cb(ctx, false, utf16Str.data(), utf16Str.size());
}
#endif

#if JSI_VERSION >= 17
void Runtime::setPrototypeOf(const Object& object, const Value& prototype) {
  auto setPrototypeOfFn = global()
                              .getPropertyAsObject(*this, "Object")
                              .getPropertyAsFunction(*this, "setPrototypeOf");
  setPrototypeOfFn.call(*this, object, prototype).asObject(*this);
}

Value Runtime::getPrototypeOf(const Object& object) {
  auto setPrototypeOfFn = global()
                              .getPropertyAsObject(*this, "Object")
                              .getPropertyAsFunction(*this, "getPrototypeOf");
  return setPrototypeOfFn.call(*this, object);
}
#endif

#if JSI_VERSION >= 18
Object Runtime::createObjectWithPrototype(const Value& prototype) {
  auto createFn = global()
                      .getPropertyAsObject(*this, "Object")
                      .getPropertyAsFunction(*this, "create");
  return createFn.call(*this, prototype).asObject(*this);
}
#endif

#if JSI_VERSION >= 20
void Runtime::setRuntimeDataImpl(
    const UUID& uuid,
    const void* data,
    void (*deleter)(const void* data)) {
  auto& runtimeDataGlobal = getRuntimeDataGlobal();
  std::lock_guard<std::mutex> lock(runtimeDataGlobal.mutex_);
  if (auto it = runtimeDataGlobal.dataMap_.find(this);
      it != runtimeDataGlobal.dataMap_.end()) {
    auto& map = it->second;
    if (auto entryIt = map.find(uuid); entryIt != map.end()) {
      // Free the old data
      auto oldData = entryIt->second.first;
      auto oldDataDeleter = entryIt->second.second;
      oldDataDeleter(oldData);
    }
    map[uuid] = {data, deleter};
    return;
  }
  // No custom data entry exist for this runtime in the global map, so create
  // one.
  runtimeDataGlobal.dataMap_[this][uuid] = {data, deleter};

  // The first time data is added for this runtime is added to the map, install
  // a host object on the global object of the runtime. This host object is used
  // to release the runtime's entry from the global custom data map when the
  // runtime is destroyed.
  // Also, try to protect the host object by making it non-configurable,
  // non-enumerable, and non-writable. These JSI operations are purposely
  // performed after runtime-specific data map is added and the host object is
  // created to prevent data leaks if any operations fail.
  Object ho = Object::createFromHostObject(
      *this, std::make_shared<RemoveRuntimeDataHostObject>(this));
  global().setProperty(*this, "_jsiRuntimeDataCleanUp", ho);
  auto definePropertyFn = global()
                              .getPropertyAsObject(*this, "Object")
                              .getPropertyAsFunction(*this, "defineProperty");
  auto desc = Object(*this);
  desc.setProperty(*this, "configurable", Value(false));
  desc.setProperty(*this, "enumerable", Value(false));
  desc.setProperty(*this, "writable", Value(false));
  definePropertyFn.call(*this, global(), "_jsiRuntimeDataCleanUp", desc);
}

const void* Runtime::getRuntimeDataImpl(const UUID& uuid) {
  auto& runtimeDataGlobal = getRuntimeDataGlobal();
  std::lock_guard<std::mutex> lock(runtimeDataGlobal.mutex_);
  if (auto runtimeMapIt = runtimeDataGlobal.dataMap_.find(this);
      runtimeMapIt != runtimeDataGlobal.dataMap_.end()) {
    if (auto customDataIt = runtimeMapIt->second.find(uuid);
        customDataIt != runtimeMapIt->second.end()) {
      return customDataIt->second.first;
    }
  }
  return nullptr;
}
#endif

Pointer& Pointer::operator=(Pointer&& other) JSI_NOEXCEPT_15 {
  if (ptr_) {
    ptr_->invalidate();
  }
  ptr_ = other.ptr_;
  other.ptr_ = nullptr;
  return *this;
}

Object Object::getPropertyAsObject(Runtime& runtime, const char* name) const {
  Value v = getProperty(runtime, name);

  if (!v.isObject()) {
    throw JSError(
        runtime,
        std::string("getPropertyAsObject: property '") + name + "' is " +
            kindToString(v, &runtime) + ", expected an Object");
  }

  return v.getObject(runtime);
}

Function Object::getPropertyAsFunction(Runtime& runtime, const char* name)
    const {
  Object obj = getPropertyAsObject(runtime, name);
  if (!obj.isFunction(runtime)) {
    throw JSError(
        runtime,
        std::string("getPropertyAsFunction: property '") + name + "' is " +
            kindToString(std::move(obj), &runtime) + ", expected a Function");
  };

  return std::move(obj).getFunction(runtime);
}

Array Object::asArray(Runtime& runtime) const& {
  if (!isArray(runtime)) {
    throw JSError(
        runtime,
        "Object is " + kindToString(Value(runtime, *this), &runtime) +
            ", expected an array");
  }
  return getArray(runtime);
}

Array Object::asArray(Runtime& runtime) && {
  if (!isArray(runtime)) {
    throw JSError(
        runtime,
        "Object is " + kindToString(Value(runtime, *this), &runtime) +
            ", expected an array");
  }
  return std::move(*this).getArray(runtime);
}

Function Object::asFunction(Runtime& runtime) const& {
  if (!isFunction(runtime)) {
    throw JSError(
        runtime,
        "Object is " + kindToString(Value(runtime, *this), &runtime) +
            ", expected a function");
  }
  return getFunction(runtime);
}

Function Object::asFunction(Runtime& runtime) && {
  if (!isFunction(runtime)) {
    throw JSError(
        runtime,
        "Object is " + kindToString(Value(runtime, *this), &runtime) +
            ", expected a function");
  }
  return std::move(*this).getFunction(runtime);
}

Value::Value(Value&& other) JSI_NOEXCEPT_15 : Value(other.kind_) {
  if (kind_ == BooleanKind) {
    data_.boolean = other.data_.boolean;
  } else if (kind_ == NumberKind) {
    data_.number = other.data_.number;
  } else if (kind_ >= PointerKind) {
    new (&data_.pointer) Pointer(std::move(other.data_.pointer));
  }
  // when the other's dtor runs, nothing will happen.
  other.kind_ = UndefinedKind;
}

Value::Value(Runtime& runtime, const Value& other) : Value(other.kind_) {
  // data_ is uninitialized, so use placement new to create non-POD
  // types in it.  Any other kind of initialization will call a dtor
  // first, which is incorrect.
  if (kind_ == BooleanKind) {
    data_.boolean = other.data_.boolean;
  } else if (kind_ == NumberKind) {
    data_.number = other.data_.number;
  } else if (kind_ == SymbolKind) {
    new (&data_.pointer) Pointer(runtime.cloneSymbol(other.data_.pointer.ptr_));
#if JSI_VERSION >= 6
  } else if (kind_ == BigIntKind) {
    new (&data_.pointer) Pointer(runtime.cloneBigInt(other.data_.pointer.ptr_));
#endif
  } else if (kind_ == StringKind) {
    new (&data_.pointer) Pointer(runtime.cloneString(other.data_.pointer.ptr_));
  } else if (kind_ >= ObjectKind) {
    new (&data_.pointer) Pointer(runtime.cloneObject(other.data_.pointer.ptr_));
  }
}

Value::~Value() {
  if (kind_ >= PointerKind) {
    data_.pointer.~Pointer();
  }
}

bool Value::strictEquals(Runtime& runtime, const Value& a, const Value& b) {
  if (a.kind_ != b.kind_) {
    return false;
  }
  switch (a.kind_) {
    case UndefinedKind:
    case NullKind:
      return true;
    case BooleanKind:
      return a.data_.boolean == b.data_.boolean;
    case NumberKind:
      return a.data_.number == b.data_.number;
    case SymbolKind:
      return runtime.strictEquals(
          static_cast<const Symbol&>(a.data_.pointer),
          static_cast<const Symbol&>(b.data_.pointer));
#if JSI_VERSION >= 6
    case BigIntKind:
      return runtime.strictEquals(
          static_cast<const BigInt&>(a.data_.pointer),
          static_cast<const BigInt&>(b.data_.pointer));
#endif
    case StringKind:
      return runtime.strictEquals(
          static_cast<const String&>(a.data_.pointer),
          static_cast<const String&>(b.data_.pointer));
    case ObjectKind:
      return runtime.strictEquals(
          static_cast<const Object&>(a.data_.pointer),
          static_cast<const Object&>(b.data_.pointer));
  }
  return false;
}

bool Value::asBool() const {
  if (!isBool()) {
    throw JSINativeException(
        "Value is " + kindToString(*this) + ", expected a boolean");
  }

  return getBool();
}

double Value::asNumber() const {
  if (!isNumber()) {
    throw JSINativeException(
        "Value is " + kindToString(*this) + ", expected a number");
  }

  return getNumber();
}

Object Value::asObject(Runtime& rt) const& {
  if (!isObject()) {
    throw JSError(
        rt, "Value is " + kindToString(*this, &rt) + ", expected an Object");
  }

  return getObject(rt);
}

Object Value::asObject(Runtime& rt) && {
  if (!isObject()) {
    throw JSError(
        rt, "Value is " + kindToString(*this, &rt) + ", expected an Object");
  }
  auto ptr = data_.pointer.ptr_;
  data_.pointer.ptr_ = nullptr;
  return static_cast<Object>(ptr);
}

Symbol Value::asSymbol(Runtime& rt) const& {
  if (!isSymbol()) {
    throw JSError(
        rt, "Value is " + kindToString(*this, &rt) + ", expected a Symbol");
  }

  return getSymbol(rt);
}

Symbol Value::asSymbol(Runtime& rt) && {
  if (!isSymbol()) {
    throw JSError(
        rt, "Value is " + kindToString(*this, &rt) + ", expected a Symbol");
  }

  return std::move(*this).getSymbol(rt);
}

#if JSI_VERSION >= 6
BigInt Value::asBigInt(Runtime& rt) const& {
  if (!isBigInt()) {
    throw JSError(
        rt, "Value is " + kindToString(*this, &rt) + ", expected a BigInt");
  }

  return getBigInt(rt);
}

BigInt Value::asBigInt(Runtime& rt) && {
  if (!isBigInt()) {
    throw JSError(
        rt, "Value is " + kindToString(*this, &rt) + ", expected a BigInt");
  }

  return std::move(*this).getBigInt(rt);
}
#endif

String Value::asString(Runtime& rt) const& {
  if (!isString()) {
    throw JSError(
        rt, "Value is " + kindToString(*this, &rt) + ", expected a String");
  }

  return getString(rt);
}

String Value::asString(Runtime& rt) && {
  if (!isString()) {
    throw JSError(
        rt, "Value is " + kindToString(*this, &rt) + ", expected a String");
  }

  return std::move(*this).getString(rt);
}

String Value::toString(Runtime& runtime) const {
  Function toString = runtime.global().getPropertyAsFunction(runtime, "String");
  return toString.call(runtime, *this).getString(runtime);
}

#if JSI_VERSION >= 8
uint64_t BigInt::asUint64(Runtime& runtime) const {
  if (!isUint64(runtime)) {
    throw JSError(runtime, "Lossy truncation in BigInt64::asUint64");
  }
  return getUint64(runtime);
}

int64_t BigInt::asInt64(Runtime& runtime) const {
  if (!isInt64(runtime)) {
    throw JSError(runtime, "Lossy truncation in BigInt64::asInt64");
  }
  return getInt64(runtime);
}
#endif

Array Array::createWithElements(
    Runtime& rt,
    std::initializer_list<Value> elements) {
  Array result(rt, elements.size());
  size_t index = 0;
  for (const auto& element : elements) {
    result.setValueAtIndex(rt, index++, element);
  }
  return result;
}

std::vector<PropNameID> HostObject::getPropertyNames(Runtime&) {
  return {};
}

Runtime::ScopeState* Runtime::pushScope() {
  return nullptr;
}

void Runtime::popScope(ScopeState*) {}

JSError::JSError(Runtime& rt, Value&& value) {
  setValue(rt, std::move(value));
}

JSError::JSError(Runtime& rt, std::string msg) : message_(std::move(msg)) {
  try {
    setValue(
        rt,
        callGlobalFunction(rt, "Error", String::createFromUtf8(rt, message_)));
  } catch (const JSIException& ex) {
    message_ = std::string(ex.what()) + " (while raising " + message_ + ")";
    setValue(rt, String::createFromUtf8(rt, message_));
  }
}

JSError::JSError(Runtime& rt, std::string msg, std::string stack)
    : message_(std::move(msg)), stack_(std::move(stack)) {
  try {
    Object e(rt);
    e.setProperty(rt, "message", String::createFromUtf8(rt, message_));
    e.setProperty(rt, "stack", String::createFromUtf8(rt, stack_));
    setValue(rt, std::move(e));
  } catch (const JSIException& ex) {
    setValue(rt, String::createFromUtf8(rt, ex.what()));
  }
}

JSError::JSError(std::string what, Runtime& rt, Value&& value)
    : JSIException(std::move(what)) {
  setValue(rt, std::move(value));
}

JSError::JSError(Value&& value, std::string message, std::string stack)
    : JSIException(message + "\n\n" + stack),
      value_(std::make_shared<Value>(std::move(value))),
      message_(std::move(message)),
      stack_(std::move(stack)) {}

void JSError::setValue(Runtime& rt, Value&& value) {
  value_ = std::make_shared<Value>(std::move(value));

  if ((message_.empty() || stack_.empty()) && value_->isObject()) {
    auto obj = value_->getObject(rt);

    if (message_.empty()) {
      try {
        Value message = obj.getProperty(rt, "message");
        if (!message.isUndefined() && !message.isString()) {
          message = callGlobalFunction(rt, "String", message);
        }
        if (message.isString()) {
          message_ = message.getString(rt).utf8(rt);
        } else if (!message.isUndefined()) {
          message_ = "String(e.message) is a " + kindToString(message, &rt);
        }
      } catch (const JSIException& ex) {
        message_ = std::string("[Exception while creating message string: ") +
            ex.what() + "]";
      }
    }

    if (stack_.empty()) {
      try {
        Value stack = obj.getProperty(rt, "stack");
        if (!stack.isUndefined() && !stack.isString()) {
          stack = callGlobalFunction(rt, "String", stack);
        }
        if (stack.isString()) {
          stack_ = stack.getString(rt).utf8(rt);
        } else if (!stack.isUndefined()) {
          stack_ = "String(e.stack) is a " + kindToString(stack, &rt);
        }
      } catch (const JSIException& ex) {
        message_ = std::string("[Exception while creating stack string: ") +
            ex.what() + "]";
      }
    }
  }

  if (message_.empty()) {
    try {
      if (value_->isString()) {
        message_ = value_->getString(rt).utf8(rt);
      } else {
        Value message = callGlobalFunction(rt, "String", *value_);
        if (message.isString()) {
          message_ = message.getString(rt).utf8(rt);
        } else {
          message_ = "String(e) is a " + kindToString(message, &rt);
        }
      }
    } catch (const JSIException& ex) {
      message_ = std::string("[Exception while creating message string: ") +
          ex.what() + "]";
    }
  }

  if (stack_.empty()) {
    stack_ = "no stack";
  }

  if (what_.empty()) {
    what_ = message_ + "\n\n" + stack_;
  }
}

JSIException::~JSIException() {}

JSINativeException::~JSINativeException() {}

JSError::~JSError() {}

} // namespace jsi
} // namespace facebook
