/*
 * Copyright (c) Microsoft Corporation.
 * Licensed under the MIT License.
 */

#include "./PlatformIntlShared.cpp"
#include "hermes/Platform/Intl/PlatformIntl.h"

#include <icu.h>
#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include "llvh/Support/ConvertUTF.h"

using namespace ::hermes;

namespace hermes {
namespace platform_intl {

namespace {

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
vm::CallResult<std::vector<std::u16string>> canonicalizeLocaleList(
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

/// https://402.ecma-international.org/8.0/#sec-getoption
vm::CallResult<std::u16string> getOptionString(
    vm::Runtime &runtime,
    const Options &options,
    const std::u16string &property,
    const std::vector<std::u16string> &validValues,
    const std::u16string &fallback) {
  // 1. Assert type(options) is object
  // 2. Let value be ? Get(options, property).
  auto valueIt = options.find(property);
  // 3. If value is undefined, return fallback.
  if (valueIt == options.end())
    return std::u16string(fallback);

  const auto &value = valueIt->second.getString();
  // 4. Assert: type is "boolean" or "string".
  // 5. If type is "boolean", then
  // a. Set value to ! ToBoolean(value).
  // 6. If type is "string", then
  // a. Set value to ? ToString(value).
  // 7. If values is not undefined and values does not contain an element equal
  // to value, throw a RangeError exception.
  if (!validValues.empty() && llvh::find(validValues, value) == validValues.end())
    return runtime.raiseRangeError(
        vm::TwineChar16(property.c_str()) +
        vm::TwineChar16("Value is invalid."));
  // 8. Return value.
  return std::u16string(value);
}

/// https://402.ecma-international.org/8.0/#sec-supportedlocales
std::vector<std::u16string> supportedLocales(
    const std::vector<std::u16string> &availableLocales,
    const std::vector<std::u16string> &requestedLocales,
    const Options &options) {
  // 1. Set options to ? CoerceOptionsToObject(options).
  // 2. Let matcher be ? GetOption(options, "localeMatcher", "string", «
  //    "lookup", "best fit" », "best fit").
  // 3. If matcher is "best fit", then
  //   a. Let supportedLocales be BestFitSupportedLocales(availableLocales,
  //      requestedLocales).
  // 4. Else,
  //   a. Let supportedLocales be LookupSupportedLocales(availableLocales,
  //      requestedLocales).
  // 5. Return CreateArrayFromList(supportedLocales).

  // We do not implement a BestFitMatcher, so we can just use LookupMatcher.
  return lookupSupportedLocales(availableLocales, requestedLocales);
}
} // namespace

// https://tc39.es/ecma402/#sec-intl.getcanonicallocales
vm::CallResult<std::vector<std::u16string>> getCanonicalLocales(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales) {
  return canonicalizeLocaleList(runtime, locales);
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
namespace {
struct CollatorWindows : Collator {
  CollatorWindows(const char16_t *l) : locale(l) {}
  std::u16string locale;
};
} // namespace

Collator::Collator() = default;
Collator::~Collator() = default;

vm::CallResult<std::vector<std::u16string>> Collator::supportedLocalesOf(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales,
    const Options &options) noexcept {
  return std::vector<std::u16string>{u"en-CA", u"de-DE"};
}

vm::CallResult<std::unique_ptr<Collator>> Collator::create(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales,
    const Options &options) noexcept {
  return std::make_unique<CollatorWindows>(u"en-US");
}

Options Collator::resolvedOptions() noexcept {
  Options options;
  options.emplace(
      u"locale", Option(static_cast<CollatorWindows *>(this)->locale));
  options.emplace(u"numeric", Option(false));
  return options;
}

double Collator::compare(
    const std::u16string &x,
    const std::u16string &y) noexcept {
  return x.compare(y);
}

namespace {
// Implementation of
// https://402.ecma-international.org/8.0/#datetimeformat-objects
class DateTimeFormatWindows : public DateTimeFormat {
 public:
  DateTimeFormatWindows() = default;
  ~DateTimeFormatWindows() {
    udat_close(dtf_);
  };

  vm::ExecutionStatus initialize(
      vm::Runtime &runtime,
      const std::vector<std::u16string> &locales,
      const Options &inputOptions) noexcept;

  Options resolvedOptions() noexcept;

  std::u16string format(double jsTimeValue) noexcept;

  std::vector<Part> formatToParts(double x) noexcept;

 private:
  // Options used with DateTimeFormat
  std::u16string locale_;
  std::u16string timeZone_;
  std::u16string weekday_;
  std::u16string era_;
  std::u16string year_;
  std::u16string month_;
  std::u16string day_;
  std::u16string dayPeriod_; // Not yet supported
  std::u16string hour_;
  std::u16string minute_;
  std::u16string second_;
  std::u16string timeZoneName_;
  std::u16string dateStyle_;
  std::u16string timeStyle_;
  std::u16string hourCycle_;
  // Internal use
  UDateFormat *dtf_;
  std::string locale8_;
  UDateFormat *getUDateFormatter(vm::Runtime &runtime);
  vm::CallResult<std::u16string> getDefaultHourCycle(vm::Runtime &runtime);
};
} // namespace

DateTimeFormat::DateTimeFormat() = default;
DateTimeFormat::~DateTimeFormat() = default;

// Implementation of
// https://402.ecma-international.org/8.0/#sec-intl.datetimeformat.supportedlocalesof
// without options
vm::CallResult<std::vector<std::u16string>> DateTimeFormat::supportedLocalesOf(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales,
    const Options &options) noexcept {
  // 1. Let availableLocales be %DateTimeFormat%.[[AvailableLocales]].
  std::vector<std::u16string> availableLocales = {};
  for (int32_t i = 0, count = uloc_countAvailable(); i < count; i++) {
    auto locale = uloc_getAvailable(i);
    availableLocales.push_back(UTF8toUTF16(runtime, locale).getValue());
  }

  // 2. Let requestedLocales be ? CanonicalizeLocaleList(locales).
  auto requestedLocales = getCanonicalLocales(runtime, locales).getValue();

  // 3. Return ? SupportedLocales(availableLocales, requestedLocales, options).
  return supportedLocales(availableLocales, requestedLocales, options);
}

// Implementation of
// https://402.ecma-international.org/8.0/#sec-initializedatetimeformat
vm::ExecutionStatus DateTimeFormatWindows::initialize(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales,
    const Options &inputOptions) noexcept {
  auto requestedLocalesRes = canonicalizeLocaleList(runtime, locales);
  if (requestedLocalesRes.getStatus() == vm::ExecutionStatus::EXCEPTION) {
    return requestedLocalesRes.getStatus();
  }
  locale_ = locales.front();

  auto conversion = UTF16toUTF8(runtime, locale_);
  if (conversion.getStatus() == vm::ExecutionStatus::EXCEPTION) {
    return conversion.getStatus();
  }
  locale8_ = conversion.getValue(); // store the UTF8 version of locale since it
                                    // is used in almost all other functions

  // 2. Let options be ? ToDateTimeOptions(options, "any", "date").
  Options options =
      toDateTimeOptions(runtime, inputOptions, u"any", u"date").getValue();
  // 3. Let opt be a new Record.
  std::unordered_map<std::u16string, std::u16string> opt;
  // 4. Let matcher be ? GetOption(options, "localeMatcher", "string",
  // «"lookup", "best fit" », "best fit").
  auto matcher = getOptionString(
      runtime, options, u"localeMatcher", {u"lookup", u"best fit"}, u"lookup");
  // 5. Set opt.[[localeMatcher]] to matcher.
  opt.emplace(u"localeMatcher", matcher.getValue());
  // 6. Let calendar be ? GetOption(options, "calendar", "string", undefined,
  // undefined).
  auto calendar = getOptionString(runtime, options, u"calendar", {}, {});
  opt.emplace(u"ca", calendar.getValue());

  // 9. Let numberingSystem be ? GetOption(options, "numberingSystem",
  // "string", undefined, undefined).
  // 10. If numberingSystem is not undefined, then
  // a. If numberingSystem does not match the Unicode Locale Identifier
  // type nonterminal, throw a RangeError exception.

  // 11. Set opt.[[nu]] to numberingSystem.
  opt.emplace(u"nu", u"");

  // 12. Let hour12 be ? GetOption(options, "hour12", "boolean", undefined,
  // undefined).
  auto hour12 = getOptionBool(runtime, options, u"hour12", {});

  // 13. Let hourCycle be ? GetOption(options, "hourCycle", "string", «"h11",
  // "h12", "h23", "h24" », undefined).
  static const std::vector<std::u16string> hourCycles = {
      u"h11", u"h12", u"h23", u"h24"};
  auto hourCycleRes =
      getOptionString(runtime, options, u"hourCycle", hourCycles, {});
  std::u16string hourCycle = hourCycleRes.getValue();

  // 14. If hour12 is not undefined, then a. Set hourCycle to null.
  if (hour12.has_value()) {
    hourCycle = u"";
  }
  // 15. Set opt.[[hc]] to hourCycle.
  opt.emplace(u"hc", hourCycle);
  hourCycle_ = hourCycle;

  // 16. Let localeData be %DateTimeFormat%.[[LocaleData]].
  // 17. Let r be ResolveLocale(%DateTimeFormat%.[[AvailableLocales]],
  // requestedLocales, opt, %DateTimeFormat%.[[RelevantExtensionKeys]],
  // localeData).
  // 18. Set dateTimeFormat.[[Locale]] to r.[[locale]].
  // 19. Let calendar be r.[[ca]
  // 20. Set dateTimeFormat.[[Calendar]] to calendar.
  // 21. Set dateTimeFormat.[[HourCycle]] to r.[[hc]].
  // 22. Set dateTimeFormat.[[NumberingSystem]] to r.[[nu]].
  // 23. Let dataLocale be r.[[dataLocale]].

  // 24. Let timeZone be ? Get(options, "timeZone").
  auto timeZoneRes = options.find(u"timeZone");
  //  25. If timeZone is undefined, then
  if (timeZoneRes == options.end()) {
    // a. Let timeZone be DefaultTimeZone().
    // 26. Else,
  } else {
    // a. Let timeZone be ? ToString(timeZone).
    std::u16string timeZone = std::u16string(timeZoneRes->second.getString());
    // b. If the result of IsValidTimeZoneName(timeZone) is false, then
    // i. Throw a RangeError exception.
    // c. Let timeZone be CanonicalizeTimeZoneName(timeZone).
    // 27. Set dateTimeFormat.[[TimeZone]] to timeZone.
    timeZone_ = timeZone;
  }

  // 28. Let opt be a new Record.
  // 29. For each row of Table 4, except the header row, in table order, do
  // a. Let prop be the name given in the Property column of the row.
  // b. If prop is "fractionalSecondDigits", then
  // i. Let value be ? GetNumberOption(options, "fractionalSecondDigits", 1,
  // 3, undefined).
  // d. Set opt.[[<prop>]] to value.
  // c. Else,
  // i. Let value be ? GetOption(options, prop, "string", « the strings
  // given in the Values column of the row », undefined).
  // d. Set opt.[[<prop>]] to value.
  // 30. Let dataLocaleData be localeData.[[<dataLocale>]].
  // 31. Let matcher be ? GetOption(options, "formatMatcher", "string", «
  // "basic", "best fit" », "best fit").

  // 32. Let dateStyle be ? GetOption(options, "dateStyle", "string", « "full",
  // "long", "medium", "short" », undefined).
  static const std::vector<std::u16string> dateStyles = {
      u"full", u"long", u"medium", u"short"};
  auto dateStyleRes =
      getOptionString(runtime, options, u"dateStyle", dateStyles, {});
  if (dateStyleRes.getStatus() == vm::ExecutionStatus::EXCEPTION) {
    return dateStyleRes.getStatus();
  }
  // 33. Set dateTimeFormat.[[DateStyle]] to dateStyle.
  dateStyle_ = dateStyleRes.getValue();

  // 34. Let timeStyle be ? GetOption(options, "timeStyle", "string", « "full",
  // "long", "medium", "short" », undefined).
  static const std::vector<std::u16string> timeStyles = {
      u"full", u"long", u"medium", u"short"};
  auto timeStyleRes =
      getOptionString(runtime, options, u"timeStyle", timeStyles, {});
  if (timeStyleRes.getStatus() == vm::ExecutionStatus::EXCEPTION) {
    return timeStyleRes.getStatus();
  }
  // 35. Set dateTimeFormat.[[TimeStyle]] to timeStyle.
  timeStyle_ = timeStyleRes.getValue();

  // Initialize properties using values from the options.
  static const std::vector<std::u16string> weekdayValues = {
      u"narrow", u"short", u"long"};
  auto weekdayRes =
      getOptionString(runtime, options, u"weekday", weekdayValues, {});
  if (weekdayRes.getStatus() == vm::ExecutionStatus::EXCEPTION) {
    return weekdayRes.getStatus();
  }
  weekday_ = weekdayRes.getValue();

  static const std::vector<std::u16string> eraValues = {
      u"narrow", u"short", u"long"};
  auto eraRes = getOptionString(runtime, options, u"era", eraValues, {});
  if (eraRes.getStatus() == vm::ExecutionStatus::EXCEPTION) {
    return eraRes.getStatus();
  }
  era_ = *eraRes;

  static const std::vector<std::u16string> yearValues = {
      u"2-digit", u"numeric"};
  auto yearRes = getOptionString(runtime, options, u"year", yearValues, {});
  if (yearRes.getStatus() == vm::ExecutionStatus::EXCEPTION) {
    return yearRes.getStatus();
  }
  year_ = *yearRes;

  static const std::vector<std::u16string> monthValues = {
      u"2-digit", u"numeric", u"narrow", u"short", u"long"};
  auto monthRes = getOptionString(runtime, options, u"month", monthValues, {});
  if (monthRes.getStatus() == vm::ExecutionStatus::EXCEPTION) {
    return monthRes.getStatus();
  }
  month_ = *monthRes;

  static const std::vector<std::u16string> dayValues = {u"2-digit", u"numeric"};
  auto dayRes = getOptionString(runtime, options, u"day", dayValues, {});
  if (dayRes.getStatus() == vm::ExecutionStatus::EXCEPTION) {
    return dayRes.getStatus();
  }
  day_ = *dayRes;

  static const std::vector<std::u16string> dayPeriodValues = {
      u"narrow", u"short", u"long"};
  auto dayPeriodRes =
      getOptionString(runtime, options, u"dayPeriod", dayPeriodValues, {});
  if (dayPeriodRes.getStatus() == vm::ExecutionStatus::EXCEPTION) {
    return dayPeriodRes.getStatus();
  }
  dayPeriod_ = *dayPeriodRes;

  static const std::vector<std::u16string> hourValues = {
      u"2-digit", u"numeric"};
  auto hourRes = getOptionString(runtime, options, u"hour", hourValues, {});
  if (hourRes.getStatus() == vm::ExecutionStatus::EXCEPTION) {
    return hourRes.getStatus();
  }
  hour_ = *hourRes;

  static const std::vector<std::u16string> minuteValues = {
      u"2-digit", u"numeric"};
  auto minuteRes =
      getOptionString(runtime, options, u"minute", minuteValues, {});
  if (minuteRes.getStatus() == vm::ExecutionStatus::EXCEPTION) {
    return minuteRes.getStatus();
  }
  minute_ = *minuteRes;

  static const std::vector<std::u16string> secondValues = {
      u"2-digit", u"numeric"};
  auto secondRes =
      getOptionString(runtime, options, u"second", secondValues, {});
  if (secondRes.getStatus() == vm::ExecutionStatus::EXCEPTION) {
    return secondRes.getStatus();
  }
  second_ = *secondRes;

  static const std::vector<std::u16string> timeZoneNameValues = {
      u"short",
      u"long",
      u"shortOffset",
      u"longOffset",
      u"shortGeneric",
      u"longGeneric"};
  auto timeZoneNameRes = getOptionString(
      runtime, options, u"timeZoneName", timeZoneNameValues, {});
  if (timeZoneNameRes.getStatus() == vm::ExecutionStatus::EXCEPTION) {
    return timeZoneNameRes.getStatus();
  }
  timeZoneName_ = *timeZoneNameRes;

  // 36. If dateStyle is not undefined or timeStyle is not undefined, then
  // a. For each row in Table 4, except the header row, do
  // i. Let prop be the name given in the Property column of the row.
  // ii. Let p be opt.[[<prop>]].
  // iii. If p is not undefined, then
  // 1. Throw a TypeError exception.
  // b. Let styles be dataLocaleData.[[styles]].[[<calendar>]].
  // c. Let bestFormat be DateTimeStyleFormat(dateStyle, timeStyle, styles).
  // 37. Else,
  // a. Let formats be dataLocaleData.[[formats]].[[<calendar>]].
  // b. If matcher is "basic", then
  // i. Let bestFormat be BasicFormatMatcher(opt, formats).
  // c. Else,
  // i. Let bestFormat be BestFitFormatMatcher(opt, formats).
  // 38. For each row in Table 4, except the header row, in table order, do
  // for (auto const &row : table4) {
  // a. Let prop be the name given in the Property column of the row.
  // auto prop = row.first;
  // b. If bestFormat has a field [[<prop>]], then
  // i. Let p be bestFormat.[[<prop>]].
  // ii. Set dateTimeFormat's internal slot whose name is the Internal
  // Slot column of the row to p.

  // 39. If dateTimeFormat.[[Hour]] is undefined, then
  if (hour_.empty()) {
    // a. Set dateTimeFormat.[[HourCycle]] to undefined.
    hourCycle_ = u"";
    // b. Let pattern be bestFormat.[[pattern]].
    // c. Let rangePatterns be bestFormat.[[rangePatterns]].
    // 40. Else,
  } else {
    // a. Let hcDefault be dataLocaleData.[[hourCycle]].
    std::u16string hcDefault = getDefaultHourCycle(runtime).getValue();
    // b. Let hc be dateTimeFormat.[[HourCycle]].
    auto hc = hourCycle_;
    // c. If hc is null, then
    if (hc.empty())
      // i. Set hc to hcDefault.
      hc = hcDefault;
    // d. If hour12 is not undefined, then
    if (hour12.has_value()) {
      // i. If hour12 is true, then
      if (*hour12 == true) {
        // 1. If hcDefault is "h11" or "h23", then
        if (hcDefault == u"h11" || hcDefault == u"h23") {
          // a. Set hc to "h11".
          hc = u"h11";
          // 2. Else,
        } else {
          // a. Set hc to "h12".
          hc = u"h12";
        }
        // ii. Else,
      } else {
        // 1. Assert: hour12 is false.
        // 2. If hcDefault is "h11" or "h23", then
        if (hcDefault == u"h11" || hcDefault == u"h23") {
          // a. Set hc to "h23".
          hc = u"h23";
          // 3. Else,
        } else {
          // a. Set hc to "h24".
          hc = u"h24";
        }
      }
    }
    // e. Set dateTimeFormat.[[HourCycle]] to hc.
    hourCycle_ = hc;
    // f. If dateTimeformat.[[HourCycle]] is "h11" or "h12", then
    // i. Let pattern be bestFormat.[[pattern12]].
    // ii. Let rangePatterns be bestFormat.[[rangePatterns12]].
    // g. Else,
    // i. Let pattern be bestFormat.[[pattern]].
    // ii. Let rangePatterns be bestFormat.[[rangePatterns]].
  }
  // 41. Set dateTimeFormat.[[Pattern]] to pattern.
  // 42. Set dateTimeFormat.[[RangePatterns]] to rangePatterns.
  // 43. Return dateTimeFormat
  
  dtf_ = getUDateFormatter(runtime);
  return vm::ExecutionStatus::RETURNED;
}

vm::CallResult<std::unique_ptr<DateTimeFormat>> DateTimeFormat::create(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales,
    const Options &inputOptions) noexcept {
  auto instance = std::make_unique<DateTimeFormatWindows>();
  if (LLVM_UNLIKELY(
          instance->initialize(runtime, locales, inputOptions) ==
          vm::ExecutionStatus::EXCEPTION)) {
    return vm::ExecutionStatus::EXCEPTION;
  }
  return instance;
}

Options DateTimeFormatWindows::resolvedOptions() noexcept {
  Options options;
  options.emplace(u"locale", Option(locale_));
  options.emplace(u"timeZone", Option(timeZone_));
  options.emplace(u"weekday", weekday_);
  options.emplace(u"era", era_);
  options.emplace(u"year", year_);
  options.emplace(u"month", month_);
  options.emplace(u"day", day_);
  options.emplace(u"hour", hour_);
  options.emplace(u"minute", minute_);
  options.emplace(u"second", second_);
  options.emplace(u"timeZoneName", timeZoneName_);
  options.emplace(u"timeZone", timeZone_);
  options.emplace(u"dateStyle", dateStyle_);
  options.emplace(u"timeStyle", timeStyle_);
  return options;
}

Options DateTimeFormat::resolvedOptions() noexcept {
  return static_cast<DateTimeFormatWindows *>(this)->resolvedOptions();
}

std::u16string DateTimeFormatWindows::format(double jsTimeValue) noexcept {
  auto timeInSeconds = jsTimeValue;
  UDate date = UDate(timeInSeconds);
  UErrorCode status = U_ZERO_ERROR;
  std::u16string myString;
  int32_t myStrlen = 0;

  myStrlen = udat_format(dtf_, date, nullptr, myStrlen, nullptr, &status);
  if (status == U_BUFFER_OVERFLOW_ERROR) {
    status = U_ZERO_ERROR;
    myString.resize(myStrlen);
    udat_format(dtf_, date, &myString[0], myStrlen, nullptr, &status);
  }

  assert(status <= 0); // Check for errors
  return myString;
}

vm::CallResult<std::u16string> DateTimeFormatWindows::getDefaultHourCycle(
    vm::Runtime &runtime) {
  UErrorCode status = U_ZERO_ERROR;
  std::u16string myString;
  // open the default UDateFormat and Pattern of locale
  UDateFormat *defaultDTF = udat_open(
      UDAT_DEFAULT,
      UDAT_DEFAULT,
      &locale8_[0],
      nullptr,
      -1,
      nullptr,
      -1,
      &status);
  int32_t size = udat_toPattern(defaultDTF, true, nullptr, 0, &status);
  if (status == U_BUFFER_OVERFLOW_ERROR) {
    status = U_ZERO_ERROR;
    myString.resize(size + 1);
    udat_toPattern(defaultDTF, true, &myString[0], 40, &status);
    assert(status <= 0); // Check for errors
    udat_close(defaultDTF);
    // find the default hour cycle and return it
    for (int32_t i = 0; i < size; i++) {
      char16_t ch = myString[i];
      switch (ch) {
        case 'K':
          return u"h11";
          break;
        case 'h':
          return u"h12";
          break;
        case 'H':
          return u"h23";
          break;
        case 'k':
          return u"h24";
          break;
      }
    }
  }

  // There should always be a default hour cycle, return an exception if not
  return vm::ExecutionStatus::EXCEPTION;
}

// gets the UDateFormat with options set in initialize
UDateFormat *DateTimeFormatWindows::getUDateFormatter(vm::Runtime &runtime) {
  static std::u16string eLong = u"long", eShort = u"short", eNarrow = u"narrow",
                        eMedium = u"medium", eFull = u"full",
                        eNumeric = u"numeric", eTwoDigit = u"2-digit",
                        eShortOffset = u"shortOffset",
                        eLongOffset = u"longOffset",
                        eShortGeneric = u"shortGeneric",
                        eLongGeneric = u"longGeneric";

  // timeStyle and dateStyle cannot be used in conjunction with the other
  // options.
  if (!timeStyle_.empty() || !dateStyle_.empty()) {
    UDateFormatStyle dateStyleRes = UDAT_DEFAULT;
    UDateFormatStyle timeStyleRes = UDAT_DEFAULT;

    if (!dateStyle_.empty()) {
      if (dateStyle_ == eFull)
        dateStyleRes = UDAT_FULL;
      else if (dateStyle_ == eLong)
        dateStyleRes = UDAT_LONG;
      else if (dateStyle_ == eMedium)
        dateStyleRes = UDAT_MEDIUM;
      else if (dateStyle_ == eShort)
        dateStyleRes = UDAT_SHORT;
    }

    if (!timeStyle_.empty()) {
      if (timeStyle_ == eFull)
        timeStyleRes = UDAT_FULL;
      else if (timeStyle_ == eLong)
        timeStyleRes = UDAT_LONG;
      else if (timeStyle_ == eMedium)
        timeStyleRes = UDAT_MEDIUM;
      else if (timeStyle_ == eShort)
        timeStyleRes = UDAT_SHORT;
    }

    UErrorCode status = U_ZERO_ERROR;
    UDateFormat *dtf;
    // if timezone is specified, use that instead, else use default
    if (!timeZone_.empty()) {
      const UChar *timeZoneRes =
          reinterpret_cast<const UChar *>(timeZone_.c_str());
      int32_t timeZoneLength = timeZone_.length();
      dtf = udat_open(
          timeStyleRes,
          dateStyleRes,
          &locale8_[0],
          timeZoneRes,
          timeZoneLength,
          nullptr,
          -1,
          &status);
    } else {
      dtf = udat_open(
          timeStyleRes,
          dateStyleRes,
          &locale8_[0],
          nullptr,
          -1,
          nullptr,
          -1,
          &status);
    }
    assert(status == U_ZERO_ERROR);
    return dtf;
  }

  // Else: lets create the skeleton
  std::u16string skeleton = u"";
  if (!weekday_.empty()) {
    if (weekday_ == eNarrow)
      skeleton += u"EEEEE";
    else if (weekday_ == eLong)
      skeleton += u"EEEE";
    else if (weekday_ == eShort)
      skeleton += u"EEE";
  }

  if (!timeZoneName_.empty()) {
    if (timeZoneName_ == eShort)
      skeleton += u"z";
    else if (timeZoneName_ == eLong)
      skeleton += u"zzzz";
    else if (timeZoneName_ == eShortOffset)
      skeleton += u"O";
    else if (timeZoneName_ == eLongOffset)
      skeleton += u"OOOO";
    else if (timeZoneName_ == eShortGeneric)
      skeleton += u"v";
    else if (timeZoneName_ == eLongGeneric)
      skeleton += u"vvvv";
  }

  if (!era_.empty()) {
    if (era_ == eNarrow)
      skeleton += u"GGGGG";
    else if (era_ == eShort)
      skeleton += u"G";
    else if (era_ == eLong)
      skeleton += u"GGGG";
  }

  if (!year_.empty()) {
    if (year_ == eNumeric)
      skeleton += u"y";
    else if (year_ == eTwoDigit)
      skeleton += u"yy";
  }

  if (!month_.empty()) {
    if (month_ == eTwoDigit)
      skeleton += u"MM";
    else if (month_ == eNumeric)
      skeleton += u'M';
    else if (month_ == eNarrow)
      skeleton += u"MMMMM";
    else if (month_ == eShort)
      skeleton += u"MMM";
    else if (month_ == eLong)
      skeleton += u"MMMM";
  }

  if (!day_.empty()) {
    if (day_ == eNumeric)
      skeleton += u"d";
    else if (day_ == eTwoDigit)
      skeleton += u"dd";
  }

  if (!hour_.empty()) {
    if (hourCycle_ == u"h12") {
      if (hour_ == eNumeric)
        skeleton += u"h";
      else if (hour_ == eTwoDigit)
        skeleton += u"hh";
    } else if (hourCycle_ == u"h24") {
      if (hour_ == eNumeric)
        skeleton += u"k";
      else if (hour_ == eTwoDigit)
        skeleton += u"kk";
    } else if (hourCycle_ == u"h23") {
      if (hour_ == eNumeric)
        skeleton += u"k";
      else if (hour_ == eTwoDigit)
        skeleton += u"KK";
    } else {
      if (hour_ == eNumeric)
        skeleton += u"h";
      else if (hour_ == eTwoDigit)
        skeleton += u"HH";
    }
  }

  if (!minute_.empty()) {
    if (minute_ == eNumeric)
      skeleton += u"m";
    else if (minute_ == eTwoDigit)
      skeleton += u"mm";
  }

  if (!second_.empty()) {
    if (second_ == eNumeric)
      skeleton += u"s";
    else if (second_ == eTwoDigit)
      skeleton += u"ss";
  }

  UErrorCode status = U_ZERO_ERROR;
  std::u16string bestpattern;
  int32_t patternLength;

  UDateTimePatternGenerator *dtpGenerator = udatpg_open(&locale8_[0], &status);
  patternLength = udatpg_getBestPatternWithOptions(
      dtpGenerator,
      &skeleton[0],
      -1,
      UDATPG_MATCH_ALL_FIELDS_LENGTH,
      nullptr,
      0,
      &status);

  if (status == U_BUFFER_OVERFLOW_ERROR) {
    status = U_ZERO_ERROR;
    bestpattern.resize(patternLength);
    udatpg_getBestPatternWithOptions(
        dtpGenerator,
        &skeleton[0],
        skeleton.length(),
        UDATPG_MATCH_ALL_FIELDS_LENGTH,
        &bestpattern[0],
        patternLength,
        &status);
  }

  // if timezone is specified, use that instead, else use default
  if (!timeZone_.empty()) {
    const UChar *timeZoneRes =
        reinterpret_cast<const UChar *>(timeZone_.c_str());
    int32_t timeZoneLength = timeZone_.length();
    return udat_open(
        UDAT_PATTERN,
        UDAT_PATTERN,
        &locale8_[0],
        timeZoneRes,
        timeZoneLength,
        &bestpattern[0],
        patternLength,
        &status);
  } else {
    return udat_open(
        UDAT_PATTERN,
        UDAT_PATTERN,
        &locale8_[0],
        nullptr,
        -1,
        &bestpattern[0],
        patternLength,
        &status);
  }
}

std::u16string DateTimeFormat::format(double jsTimeValue) noexcept {
  return static_cast<DateTimeFormatWindows *>(this)->format(jsTimeValue);
}

// Not yet implemented. Tracked by
// https://github.com/microsoft/hermes-windows/issues/87
std::vector<std::unordered_map<std::u16string, std::u16string>>
DateTimeFormatWindows::formatToParts(double jsTimeValue) noexcept {
  std::unordered_map<std::u16string, std::u16string> part;
  part[u"type"] = u"integer";
  // This isn't right, but I didn't want to do more work for a stub.
  std::string s = std::to_string(jsTimeValue);
  part[u"value"] = {s.begin(), s.end()};
  return std::vector<std::unordered_map<std::u16string, std::u16string>>{part};
}

// Not yet implemented. Tracked by
// https://github.com/microsoft/hermes-windows/issues/87
std::vector<Part> DateTimeFormat::formatToParts(double x) noexcept {
  return static_cast<DateTimeFormatWindows *>(this)->formatToParts(x);
}

// NumberFormat - Not yet implemented. Tracked by
// https://github.com/microsoft/hermes-windows/issues/87
namespace {
struct NumberFormatDummy : NumberFormat {
  NumberFormatDummy(const char16_t *l) : locale(l) {}
  std::u16string locale;
};
} // namespace

NumberFormat::NumberFormat() = default;
NumberFormat::~NumberFormat() = default;

vm::CallResult<std::vector<std::u16string>> NumberFormat::supportedLocalesOf(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales,
    const Options &options) noexcept {
  return std::vector<std::u16string>{u"en-CA", u"de-DE"};
}

vm::CallResult<std::unique_ptr<NumberFormat>> NumberFormat::create(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales,
    const Options &options) noexcept {
  return std::make_unique<NumberFormatDummy>(u"en-US");
}

Options NumberFormat::resolvedOptions() noexcept {
  Options options;
  options.emplace(
      u"locale", Option(static_cast<NumberFormatDummy *>(this)->locale));
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
