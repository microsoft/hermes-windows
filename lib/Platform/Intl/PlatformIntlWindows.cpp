/*
 * Copyright (c) Microsoft Corporation.
 * Licensed under the MIT License.
 */

#include "hermes/Platform/Intl/PlatformIntl.h"

#include <icu.h>
#include <deque>
#include <string>
#include <unordered_map>
#include "llvh/Support/ConvertUTF.h"

using namespace ::hermes;

namespace hermes {
namespace platform_intl {

// convert utf8 string to utf16
vm::CallResult<std::u16string> UTF8toUTF16(
    vm::Runtime &runtime,
    std::string_view in) {
  std::u16string out;
  size_t length = in.length();
  out.resize(length);
  const llvh::UTF8 *sourceStart = reinterpret_cast<const llvh::UTF8 *>(&in[0]);
  const llvh::UTF8 *sourceEnd = sourceStart + length;
  llvh::UTF16 *targetStart = reinterpret_cast<llvh::UTF16 *>(&out[0]);
  llvh::UTF16 *targetEnd = targetStart + out.size();
  llvh::ConversionResult convRes = ConvertUTF8toUTF16(
      &sourceStart,
      sourceEnd,
      &targetStart,
      targetEnd,
      llvh::lenientConversion);
  if (convRes != llvh::ConversionResult::conversionOK) {
    return runtime.raiseRangeError("utf8 to utf16 conversion failed");
  }
  out.resize(reinterpret_cast<char16_t *>(targetStart) - &out[0]);
  return out;
}

// convert utf16 string to utf8
vm::CallResult<std::string> UTF16toUTF8(
    vm::Runtime &runtime,
    std::u16string in) {
  std::string out;
  size_t length = in.length();
  out.resize(length);
  const llvh::UTF16 *sourceStart =
      reinterpret_cast<const llvh::UTF16 *>(&in[0]);
  const llvh::UTF16 *sourceEnd = sourceStart + length;
  llvh::UTF8 *targetStart = reinterpret_cast<llvh::UTF8 *>(&out[0]);
  llvh::UTF8 *targetEnd = targetStart + out.size();
  llvh::ConversionResult convRes = ConvertUTF16toUTF8(
      &sourceStart,
      sourceEnd,
      &targetStart,
      targetEnd,
      llvh::lenientConversion);
  if (convRes != llvh::ConversionResult::conversionOK) {
    return runtime.raiseRangeError("utf16 to utf8 conversion failed");
  }
  out.resize(reinterpret_cast<char *>(targetStart) - &out[0]);
  return out;
}

// roughly translates to
// https://tc39.es/ecma402/#sec-canonicalizeunicodelocaleid while doing some
// minimal tag validation
vm::CallResult<std::u16string> NormalizeLanguageTag(
    vm::Runtime &runtime,
    const std::u16string &locale) {
  if (locale.length() == 0) {
    return runtime.raiseRangeError("RangeError: Invalid language tag");
  }

  auto conversion = UTF16toUTF8(runtime, locale);
  const char *locale8 = conversion.getValue().c_str();

  // [Comment from ChakreCore] ICU doesn't have a full-fledged canonicalization
  // implementation that correctly replaces all preferred values and
  // grandfathered tags, as required by #sec-canonicalizelanguagetag. However,
  // passing the locale through uloc_forLanguageTag -> uloc_toLanguageTag gets
  // us most of the way there by replacing some(?) values, correctly
  // capitalizing the tag, and re-ordering extensions
  UErrorCode status = U_ZERO_ERROR;
  int32_t parsedLength = 0;
  char localeID[ULOC_FULLNAME_CAPACITY] = {0};
  char fullname[ULOC_FULLNAME_CAPACITY] = {0};
  char languageTag[ULOC_FULLNAME_CAPACITY] = {0};

  int32_t forLangTagResultLength = uloc_forLanguageTag(
      locale8, localeID, ULOC_FULLNAME_CAPACITY, &parsedLength, &status);
  if (forLangTagResultLength < 0 || parsedLength < locale.length() ||
      status == U_ILLEGAL_ARGUMENT_ERROR) {
    return runtime.raiseRangeError(
        vm::TwineChar16("Invalid language tag: ") + vm::TwineChar16(locale8));
  }

  int32_t canonicalizeResultLength =
      uloc_canonicalize(localeID, fullname, ULOC_FULLNAME_CAPACITY, &status);
  if (canonicalizeResultLength <= 0) {
    return runtime.raiseRangeError(
        vm::TwineChar16("Invalid language tag: ") + vm::TwineChar16(locale8));
  }

  int32_t toLangTagResultLength = uloc_toLanguageTag(
      fullname, languageTag, ULOC_FULLNAME_CAPACITY, true, &status);
  if (toLangTagResultLength <= 0) {
    return runtime.raiseRangeError(
        vm::TwineChar16("Invalid language tag: ") + vm::TwineChar16(locale8));
  }

  return UTF8toUTF16(runtime, languageTag);
}

// https://tc39.es/ecma402/#sec-canonicalizelocalelist
vm::CallResult<std::vector<std::u16string>> CanonicalizeLocaleList(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales) {
  // 1. If locales is undefined, then a. Return a new empty list
  if (locales.empty()) {
    return std::vector<std::u16string>{};
  }
  // 2. Let seen be a new empty List
  std::vector<std::u16string> seen = std::vector<std::u16string>{};

  // 3. If Type(locales) is String or Type(locales) is Object and locales has an
  // [[InitializedLocale]] internal slot, then
  // 4. Else
  //  > Windows/Apple don't support Locale object -
  //  https://tc39.es/ecma402/#locale-objects > As of now, 'locales' can only be
  //  a string list/array. Validation occurs in NormalizeLangugeTag for windows.
  //  > This function just takes a vector of strings.
  // 5-7. Let len be ? ToLength(? Get(O, "length")). Let k be 0. Repeat, while k
  // < len
  for (size_t k = 0; k < locales.size(); k++) {
    // minimal tag validation is done with ICU, ChakraCore\V8 does not do tag
    // validation with ICU, may be missing needed API 7.c.iii.1 Let tag be
    // kValue[[locale]]
    std::u16string tag = locales[k];
    // 7.c.vi Let canonicalizedTag be CanonicalizeUnicodeLocaleID(tag)
    auto canonicalizedTag = NormalizeLanguageTag(runtime, tag);
    if (LLVM_UNLIKELY(canonicalizedTag == vm::ExecutionStatus::EXCEPTION)) {
      return vm::ExecutionStatus::EXCEPTION;
    }
    // 7.c.vii. If canonicalizedTag is not an element of seen, append
    // canonicalizedTag as the last element of seen.
    if (std::find(seen.begin(), seen.end(), canonicalizedTag.getValue()) ==
        seen.end()) {
      seen.push_back(std::move(canonicalizedTag.getValue()));
    }
  }
  return seen;
}

// https://tc39.es/ecma402/#sec-intl.getcanonicallocales
vm::CallResult<std::vector<std::u16string>> getCanonicalLocales(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales) {
  return CanonicalizeLocaleList(runtime, locales);
}

// Not yet implemented. Tracked by
// https://github.com/microsoft/hermes-windows/issues/87
vm::CallResult<std::u16string> toLocaleLowerCase(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales,
    const std::u16string &str) {
  return std::u16string(u"lowered");
}

// Not yet implemented. Tracked by
// https://github.com/microsoft/hermes-windows/issues/87
vm::CallResult<std::u16string> toLocaleUpperCase(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales,
    const std::u16string &str) {
  return std::u16string(u"uppered");
}

// Collator - Not yet implemented. Tracked by
// https://github.com/microsoft/hermes-windows/issues/87
struct Collator::Impl {
  std::u16string locale;
};

Collator::Collator() : impl_(std::make_unique<Impl>()) {}
Collator::~Collator() {}

vm::CallResult<std::vector<std::u16string>> Collator::supportedLocalesOf(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales,
    const Options &options) noexcept {
  return std::vector<std::u16string>{u"en-CA", u"de-DE"};
}

vm::ExecutionStatus Collator::initialize(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales,
    const Options &options) noexcept {
  impl_->locale = u"en-US";
  return vm::ExecutionStatus::RETURNED;
}

Options Collator::resolvedOptions() noexcept {
  Options options;
  options.emplace(u"locale", Option(impl_->locale));
  options.emplace(u"numeric", Option(false));
  return options;
}

double Collator::compare(
    const std::u16string &x,
    const std::u16string &y) noexcept {
  return x.compare(y);
}

// DateTimeFormat - Not yet implemented. Tracked by
// https://github.com/microsoft/hermes-windows/issues/87
struct DateTimeFormat::Impl {
  std::u16string locale;
};

DateTimeFormat::DateTimeFormat() : impl_(std::make_unique<Impl>()) {}
DateTimeFormat::~DateTimeFormat() {}

vm::CallResult<std::vector<std::u16string>> DateTimeFormat::supportedLocalesOf(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales,
    const Options &options) noexcept {
  return std::vector<std::u16string>{u"en-CA", u"de-DE"};
}

vm::ExecutionStatus DateTimeFormat::initialize(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales,
    const Options &options) noexcept {
  impl_->locale = u"en-US";
  return vm::ExecutionStatus::RETURNED;
}

Options DateTimeFormat::resolvedOptions() noexcept {
  Options options;
  options.emplace(u"locale", Option(impl_->locale));
  options.emplace(u"numeric", Option(false));
  return options;
}

std::u16string DateTimeFormat::format(double jsTimeValue) noexcept {
  auto s = std::to_string(jsTimeValue);
  return std::u16string(s.begin(), s.end());
}

std::vector<std::unordered_map<std::u16string, std::u16string>>
DateTimeFormat::formatToParts(double jsTimeValue) noexcept {
  std::unordered_map<std::u16string, std::u16string> part;
  part[u"type"] = u"integer";
  // This isn't right, but I didn't want to do more work for a stub.
  std::string s = std::to_string(jsTimeValue);
  part[u"value"] = {s.begin(), s.end()};
  return std::vector<std::unordered_map<std::u16string, std::u16string>>{part};
}

// NumberFormat - Not yet implemented. Tracked by
// https://github.com/microsoft/hermes-windows/issues/87
struct NumberFormat::Impl {
  std::u16string locale;
};

NumberFormat::NumberFormat() : impl_(std::make_unique<Impl>()) {}
NumberFormat::~NumberFormat() {}

vm::CallResult<std::vector<std::u16string>> NumberFormat::supportedLocalesOf(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales,
    const Options &options) noexcept {
  return std::vector<std::u16string>{u"en-CA", u"de-DE"};
}

vm::ExecutionStatus NumberFormat::initialize(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales,
    const Options &options) noexcept {
  impl_->locale = u"en-US";
  return vm::ExecutionStatus::RETURNED;
}

Options NumberFormat::resolvedOptions() noexcept {
  Options options;
  options.emplace(u"locale", Option(impl_->locale));
  options.emplace(u"numeric", Option(false));
  return options;
}

std::u16string NumberFormat::format(double number) noexcept {
  auto s = std::to_string(number);
  return std::u16string(s.begin(), s.end());
}

std::vector<std::unordered_map<std::u16string, std::u16string>>
NumberFormat::formatToParts(double number) noexcept {
  std::unordered_map<std::u16string, std::u16string> part;
  part[u"type"] = u"integer";
  // This isn't right, but I didn't want to do more work for a stub.
  std::string s = std::to_string(number);
  part[u"value"] = {s.begin(), s.end()};
  return std::vector<std::unordered_map<std::u16string, std::u16string>>{part};
}

} // namespace platform_intl
} // namespace hermes
