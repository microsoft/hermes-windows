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

vm::CallResult<std::vector<std::u16string>> getCanonicalLocales(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales) {
  return std::vector<std::u16string>{u"fr-FR", u"es-ES"};
}

vm::CallResult<std::u16string> toLocaleLowerCase(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales,
    const std::u16string &str) {
  return std::u16string(u"lowered");
}
vm::CallResult<std::u16string> toLocaleUpperCase(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales,
    const std::u16string &str) {
  return std::u16string(u"uppered");
}

/// https://402.ecma-international.org/8.0/#sec-getoption
/// Split into getOptionString and getOptionBool to help readability
vm::CallResult<std::u16string> getOptionString(
    vm::Runtime &runtime,
    const Options &options,
    const std::u16string &property,
    std::vector<std::u16string> values,
    std::u16string fallback) {
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
  if (!values.empty() && llvh::find(values, value) == values.end())
    return runtime.raiseRangeError(
        vm::TwineChar16(property.c_str()) +
        vm::TwineChar16("Value is invalid."));
  // 8. Return value.
  return std::u16string(value);
}

// boolean + null option
enum BoolNull { eFalse, eTrue, eNull };
BoolNull getOptionBool(
    vm::Runtime &runtime,
    const Options &options,
    const std::u16string &property,
    bool fallback) {
  //  1. Assert: Type(options) is Object.
  //  2. Let value be ? Get(options, property).
  auto value = options.find(property);
  //  3. If value is undefined, return fallback.
  if (value == options.end()) {
    return eNull;
  }
  //  8. Return value.
  if (value->second.getBool()) {
    return eTrue;
  }
  return eFalse;
}

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

// Implementation of
// https://402.ecma-international.org/8.0/#sec-todatetimeoptions
Options toDateTimeOptions(
    vm::Runtime &runtime,
    Options options,
    std::u16string required,
    std::u16string defaults) {
  // 1. If options is undefined, let options be null; otherwise let options be ?
  // ToObject(options).
  // 2. Let options be OrdinaryObjectCreate(options).
  // 3. Let needDefaults be true.
  bool needDefaults = true;
  // 4. If required is "date" or "any", then
  if (required == u"date" || required == u"any") {
    // a. For each property name prop of « "weekday", "year", "month", "day" »,
    // do
    // TODO(T116352920): Make this a std::u16string props[] once we have
    // constexpr std::u16string.
    static const std::vector<std::u16string> props = {
        u"weekday", u"year", u"month", u"day"};
    for (const auto &prop : props) {
      // i. Let value be ? Get(options, prop).
      if (options.find(std::u16string(prop)) != options.end()) {
        // ii. If value is not undefined, let needDefaults be false.
        needDefaults = false;
      }
    }
  }
  // 5. If required is "time" or "any", then
  if (required == u"time" || required == u"any") {
    // a. For each property name prop of « "dayPeriod", "hour", "minute",
    // "second", "fractionalSecondDigits" », do
    static const std::vector<std::u16string> props = {
        u"dayPeriod", u"hour", u"minute", u"second", u"fractionalSecondDigits"};
    for (const auto &prop : props) {
      // i. Let value be ? Get(options, prop).
      if (options.find(std::u16string(prop)) != options.end()) {
        // ii. If value is not undefined, let needDefaults be false.
        needDefaults = false;
      }
    }
  }
  // 6. Let dateStyle be ? Get(options, "dateStyle").
  auto dateStyle = options.find(u"dateStyle");
  // 7. Let timeStyle be ? Get(options, "timeStyle").
  auto timeStyle = options.find(u"timeStyle");
  // 8. If dateStyle is not undefined or timeStyle is not undefined, let
  // needDefaults be false.
  if (dateStyle != options.end() || timeStyle != options.end()) {
    needDefaults = false;
  }
  // 9. If required is "date" and timeStyle is not undefined, then
  if (required == u"date" && timeStyle != options.end()) {
    // a. Throw a TypeError exception.
  }
  // 10. If required is "time" and dateStyle is not undefined, then
  if (required == u"time" && dateStyle != options.end()) {
    // a. Throw a TypeError exception.
  }
  // 11. If needDefaults is true and defaults is either "date" or "all", then
  if (needDefaults && (defaults == u"date" || defaults == u"all")) {
    // a. For each property name prop of « "year", "month", "day" », do
    static const std::vector<std::u16string> props = {
        u"year", u"month", u"day"};
    for (const auto &prop : props) {
      // i. Perform ? CreateDataPropertyOrThrow(options, prop, "numeric").
      options.emplace(prop, Option(std::u16string(u"numeric")));
    }
  }
  // 12. If needDefaults is true and defaults is either "time" or "all", then
  if (needDefaults && (defaults == u"time" || defaults == u"all")) {
    // a. For each property name prop of « "hour", "minute", "second" », do
    static const std::vector<std::u16string> props = {
        u"hour", u"minute", u"second"};
    for (const auto &prop : props) {
      // i. Perform ? CreateDataPropertyOrThrow(options, prop, "numeric").
      options.emplace(prop, Option(std::u16string(u"numeric")));
    }
  }
  // 13. return options
  return options;
}

struct DateTimeFormat::Impl {
  // Options used with DateTimeFormat
  std::u16string locale;
  std::u16string timeZone;
  std::u16string weekday;
  std::u16string era;
  std::u16string year;
  std::u16string month;
  std::u16string day;
  std::u16string dayPeriod; // Not Supported
  std::u16string hour;
  std::u16string minute;
  std::u16string second;
  std::u16string timeZoneName;
  std::u16string dateStyle;
  std::u16string timeStyle;
  std::u16string hourCycle;
  // Internal use
  UDateFormat *dtf;
  const char *locale8;
  UDateFormat *getUDateFormatter(vm::Runtime &runtime);
  vm::CallResult<std::u16string> getDefaultHourCycle(vm::Runtime &runtime);
};

DateTimeFormat::DateTimeFormat() : impl_(std::make_unique<Impl>()) {}
DateTimeFormat::~DateTimeFormat() {}

// get the supported locales of dateTimeFormat
vm::CallResult<std::vector<std::u16string>> DateTimeFormat::supportedLocalesOf(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales,
    const Options &options) noexcept {
  std::vector<std::u16string> result = {};
  for (int32_t i = 0; i < uloc_countAvailable(); i++) {
    auto locale = uloc_getAvailable(i);
    result.push_back(UTF8toUTF16(runtime, locale).getValue());
  }
  return result;
}

// initalize dtf
vm::ExecutionStatus DateTimeFormat::initialize(
    vm::Runtime &runtime,
    const std::vector<std::u16string> &locales,
    const Options &inputOptions) noexcept {
  auto requestedLocalesRes = CanonicalizeLocaleList(runtime, locales);
  if (requestedLocalesRes.getStatus() == vm::ExecutionStatus::EXCEPTION) {
    return requestedLocalesRes.getStatus();
  }
  impl_->locale = locales.front();

  auto conversion = UTF16toUTF8(runtime, impl_->locale);
  if (conversion.getStatus() == vm::ExecutionStatus::EXCEPTION) {
    return conversion.getStatus();
  }
  const char *locale8 = conversion.getValue().c_str();
  impl_->locale8 = locale8; // store the UTF8 version of locale since it is used
                            // in almost all other functions

  // 2. Let options be ? ToDateTimeOptions(options, "any", "date").
  Options options = toDateTimeOptions(runtime, inputOptions, u"any", u"date");
  // 3. Let opt be a new Record.
  std::unordered_map<std::u16string, std::u16string> opt;
  // 4. Let matcher be ? GetOption(options, "localeMatcher", "string",
  // «"lookup", "best fit" », "best fit").
  auto matcher = getOptionString(
      runtime,
      options,
      u"localeMatcher",
      {u"lookup", u"best fit"},
      u"best fit");
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
  BoolNull hour12 = getOptionBool(runtime, options, u"hour12", {});

  // 13. Let hourCycle be ? GetOption(options, "hourCycle", "string", «"h11",
  // "h12", "h23", "h24" », undefined).
  static const std::vector<std::u16string> hourCycles = {
      u"h11", u"h12", u"h23", u"h24"};
  auto hourCycleRes =
      getOptionString(runtime, options, u"hourCycle", hourCycles, {});
  std::u16string hourCycle = hourCycleRes.getValue();

  // 14. If hour12 is not undefined, then a. Set hourCycle to null.
  if (!(hour12 == eNull)) {
    hourCycle = u"";
  }
  // 15. Set opt.[[hc]] to hourCycle.
  opt.emplace(u"hc", hourCycle);
  impl_->hourCycle = hourCycle;

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
    impl_->timeZone = timeZone;
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
  impl_->dateStyle = dateStyleRes.getValue();

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
  impl_->timeStyle = timeStyleRes.getValue();

  // Initialize properties using values from the options.
  static const std::vector<std::u16string> weekdayValues = {
      u"narrow", u"short", u"long"};
  auto weekdayRes =
      getOptionString(runtime, options, u"weekday", weekdayValues, {});
  if (weekdayRes.getStatus() == vm::ExecutionStatus::EXCEPTION) {
    return weekdayRes.getStatus();
  }
  impl_->weekday = weekdayRes.getValue();

  static const std::vector<std::u16string> eraValues = {
      u"narrow", u"short", u"long"};
  auto eraRes = getOptionString(runtime, options, u"era", eraValues, {});
  if (eraRes.getStatus() == vm::ExecutionStatus::EXCEPTION) {
    return eraRes.getStatus();
  }
  impl_->era = *eraRes;

  static const std::vector<std::u16string> yearValues = {
      u"2-digit", u"numeric"};
  auto yearRes = getOptionString(runtime, options, u"year", yearValues, {});
  if (yearRes.getStatus() == vm::ExecutionStatus::EXCEPTION) {
    return yearRes.getStatus();
  }
  impl_->year = *yearRes;

  static const std::vector<std::u16string> monthValues = {
      u"2-digit", u"numeric", u"narrow", u"short", u"long"};
  auto monthRes = getOptionString(runtime, options, u"month", monthValues, {});
  if (monthRes.getStatus() == vm::ExecutionStatus::EXCEPTION) {
    return monthRes.getStatus();
  }
  impl_->month = *monthRes;

  static const std::vector<std::u16string> dayValues = {u"2-digit", u"numeric"};
  auto dayRes = getOptionString(runtime, options, u"day", dayValues, {});
  if (dayRes.getStatus() == vm::ExecutionStatus::EXCEPTION) {
    return dayRes.getStatus();
  }
  impl_->day = *dayRes;

  static const std::vector<std::u16string> dayPeriodValues = {
      u"narrow", u"short", u"long"};
  auto dayPeriodRes =
      getOptionString(runtime, options, u"dayPeriod", dayPeriodValues, {});
  if (dayPeriodRes.getStatus() == vm::ExecutionStatus::EXCEPTION) {
    return dayPeriodRes.getStatus();
  }
  impl_->dayPeriod = *dayPeriodRes;

  static const std::vector<std::u16string> hourValues = {
      u"2-digit", u"numeric"};
  auto hourRes = getOptionString(runtime, options, u"hour", hourValues, {});
  if (hourRes.getStatus() == vm::ExecutionStatus::EXCEPTION) {
    return hourRes.getStatus();
  }
  impl_->hour = *hourRes;

  static const std::vector<std::u16string> minuteValues = {
      u"2-digit", u"numeric"};
  auto minuteRes =
      getOptionString(runtime, options, u"minute", minuteValues, {});
  if (minuteRes.getStatus() == vm::ExecutionStatus::EXCEPTION) {
    return minuteRes.getStatus();
  }
  impl_->minute = *minuteRes;

  static const std::vector<std::u16string> secondValues = {
      u"2-digit", u"numeric"};
  auto secondRes =
      getOptionString(runtime, options, u"second", secondValues, {});
  if (secondRes.getStatus() == vm::ExecutionStatus::EXCEPTION) {
    return secondRes.getStatus();
  }
  impl_->second = *secondRes;

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
  impl_->timeZoneName = *timeZoneNameRes;

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
  if (impl_->hour.empty()) {
    // a. Set dateTimeFormat.[[HourCycle]] to undefined.
    impl_->hourCycle = u"";
    // b. Let pattern be bestFormat.[[pattern]].
    // c. Let rangePatterns be bestFormat.[[rangePatterns]].
    // 40. Else,
  } else {
    // a. Let hcDefault be dataLocaleData.[[hourCycle]].
    std::u16string hcDefault = impl_->getDefaultHourCycle(runtime).getValue();
    // b. Let hc be dateTimeFormat.[[HourCycle]].
    auto hc = impl_->hourCycle;
    // c. If hc is null, then
    if (hc.empty())
      // i. Set hc to hcDefault.
      hc = hcDefault;
    // d. If hour12 is not undefined, then
    if (!(hour12 == eNull)) {
      // i. If hour12 is true, then
      if ((hour12 == eTrue)) {
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
    impl_->hourCycle = hc;
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

  impl_->dtf = impl_->getUDateFormatter(runtime);
  return vm::ExecutionStatus::RETURNED;
}

Options DateTimeFormat::resolvedOptions() noexcept {
  Options options;
  options.emplace(u"locale", Option(impl_->locale));
  options.emplace(u"numeric", Option(false));
  options.emplace(u"timeZone", Option(impl_->timeZone));
  options.emplace(u"weekday", impl_->weekday);
  options.emplace(u"era", impl_->era);
  options.emplace(u"year", impl_->year);
  options.emplace(u"month", impl_->month);
  options.emplace(u"day", impl_->day);
  options.emplace(u"hour", impl_->hour);
  options.emplace(u"minute", impl_->minute);
  options.emplace(u"second", impl_->second);
  options.emplace(u"timeZoneName", impl_->timeZoneName);
  options.emplace(u"timeZone", impl_->timeZone);
  options.emplace(u"dateStyle", impl_->dateStyle);
  options.emplace(u"timeStyle", impl_->timeStyle);
  return options;
}

std::u16string DateTimeFormat::format(double jsTimeValue) noexcept {
  auto timeInSeconds = jsTimeValue;
  UDate *date = new UDate(timeInSeconds);
  UErrorCode status = U_ZERO_ERROR;
  UChar *myString;
  int32_t myStrlen = 0;

  myStrlen = udat_format(impl_->dtf, *date, NULL, myStrlen, NULL, &status);
  if (status == U_BUFFER_OVERFLOW_ERROR) {
    status = U_ZERO_ERROR;
    myString = (UChar *)malloc(sizeof(UChar) * (myStrlen + 1));
    udat_format(impl_->dtf, *date, myString, myStrlen + 1, NULL, &status);
    return myString;
  }

  return u"";
}

vm::CallResult<std::u16string> DateTimeFormat::Impl::getDefaultHourCycle(
    vm::Runtime &runtime) {
  UErrorCode status = U_ZERO_ERROR;
  UChar *myString;
  // open the default UDateFormat and Pattern of locale
  UDateFormat *defaultDTF =
      udat_open(UDAT_DEFAULT, UDAT_DEFAULT, locale8, 0, -1, NULL, -1, &status);
  int32_t size = udat_toPattern(defaultDTF, true, NULL, 0, &status);
  if (status == U_BUFFER_OVERFLOW_ERROR) {
    status = U_ZERO_ERROR;
    myString = (UChar *)malloc(sizeof(UChar) * (size + 1));
    udat_toPattern(defaultDTF, true, myString, 40, &status);
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

// gets the UDateFormat with options set in initalize
UDateFormat *DateTimeFormat::Impl::getUDateFormatter(vm::Runtime &runtime) {
  static std::u16string eLong = u"long", eShort = u"short", eNarrow = u"narrow",
                        eMedium = u"medium", eFull = u"full",
                        eNumeric = u"numeric", eTwoDigit = u"2-digit",
                        eShortOffset = u"shortOffset",
                        eLongOffset = u"longOffset",
                        eShortGeneric = u"shortGeneric",
                        eLongGeneric = u"longGeneric";

  // timeStyle and dateStyle cannot be used in conjunction with the other
  // options.
  if (!timeStyle.empty() || !dateStyle.empty()) {
    UDateFormatStyle dateStyleRes = UDAT_DEFAULT;
    UDateFormatStyle timeStyleRes = UDAT_DEFAULT;

    if (!dateStyle.empty()) {
      if (dateStyle == eFull)
        dateStyleRes = UDAT_FULL;
      else if (dateStyle == eLong)
        dateStyleRes = UDAT_LONG;
      else if (dateStyle == eMedium)
        dateStyleRes = UDAT_MEDIUM;
      else if (dateStyle == eShort)
        dateStyleRes = UDAT_SHORT;
    }

    if (!timeStyle.empty()) {
      if (timeStyle == eFull)
        timeStyleRes = UDAT_FULL;
      else if (timeStyle == eLong)
        timeStyleRes = UDAT_LONG;
      else if (timeStyle == eMedium)
        timeStyleRes = UDAT_MEDIUM;
      else if (timeStyle == eShort)
        timeStyleRes = UDAT_SHORT;
    }

    UErrorCode status = U_ZERO_ERROR;
    // if timezone is specified, use that instead, else use default
    if (!timeZone.empty()) {
      const UChar *timeZoneRes =
          reinterpret_cast<const UChar *>(timeZone.c_str());
      int32_t timeZoneLength = timeZone.length();
      return udat_open(
          timeStyleRes,
          dateStyleRes,
          locale8,
          timeZoneRes,
          timeZoneLength,
          NULL,
          -1,
          &status);
    }
    return udat_open(
        timeStyleRes, dateStyleRes, locale8, 0, -1, NULL, -1, &status);
  }

  // Else: lets create the skeleton
  std::u16string customDate = u"";
  if (!weekday.empty()) {
    if (weekday == eNarrow)
      customDate += u"EEEEE";
    else if (weekday == eLong)
      customDate += u"EEEE";
    else if (weekday == eShort)
      customDate += u"EEE";
  }

  if (!timeZoneName.empty()) {
    if (timeZoneName == eShort)
      customDate += u"z";
    else if (timeZoneName == eLong)
      customDate += u"zzzz";
    else if (timeZoneName == eShortOffset)
      customDate += u"O";
    else if (timeZoneName == eLongOffset)
      customDate += u"OOOO";
    else if (timeZoneName == eShortGeneric)
      customDate += u"v";
    else if (timeZoneName == eLongGeneric)
      customDate += u"vvvv";
  }

  if (!era.empty()) {
    if (era == eNarrow)
      customDate += u"GGGGG";
    else if (era == eShort)
      customDate += u"G";
    else if (era == eLong)
      customDate += u"GGGG";
  }

  if (!year.empty()) {
    if (year == eNumeric)
      customDate += u"y";
    else if (year == eTwoDigit)
      customDate += u"yy";
  }

  if (!month.empty()) {
    if (month == eTwoDigit)
      customDate += u"MM";
    else if (month == eNumeric)
      customDate += u'M';
    else if (month == eNarrow)
      customDate += u"MMMMM";
    else if (month == eShort)
      customDate += u"MMM";
    else if (month == eLong)
      customDate += u"MMMM";
  }

  if (!day.empty()) {
    if (day == eNumeric)
      customDate += u"d";
    else if (day == eTwoDigit)
      customDate += u"dd";
  }

  if (!hour.empty()) {
    if (hourCycle == u"h12") {
      if (hour == eNumeric)
        customDate += u"h";
      else if (hour == eTwoDigit)
        customDate += u"hh";
    } else if (hourCycle == u"h24") {
      if (hour == eNumeric)
        customDate += u"k";
      else if (hour == eTwoDigit)
        customDate += u"kk";
    } else if (hourCycle == u"h23") {
      if (hour == eNumeric)
        customDate += u"k";
      else if (hour == eTwoDigit)
        customDate += u"KK";
    } else {
      if (hour == eNumeric)
        customDate += u"h";
      else if (hour == eTwoDigit)
        customDate += u"HH";
    }
  }

  if (!minute.empty()) {
    if (minute == eNumeric)
      customDate += u"m";
    else if (minute == eTwoDigit)
      customDate += u"mm";
  }

  if (!second.empty()) {
    if (second == eNumeric)
      customDate += u"s";
    else if (second == eTwoDigit)
      customDate += u"ss";
  }

  UErrorCode status = U_ZERO_ERROR;
  const UChar *skeleton = reinterpret_cast<const UChar *>(customDate.c_str());
  UChar *bestpattern;
  int32_t patternLength;

  UDateTimePatternGenerator *dtpGenerator = udatpg_open(locale8, &status);
  patternLength = udatpg_getBestPatternWithOptions(
      dtpGenerator,
      skeleton,
      -1,
      UDATPG_MATCH_ALL_FIELDS_LENGTH,
      NULL,
      0,
      &status);

  if (status == U_BUFFER_OVERFLOW_ERROR) {
    status = U_ZERO_ERROR;
    bestpattern = (UChar *)malloc(sizeof(UChar) * (patternLength + 1));
    udatpg_getBestPatternWithOptions(
        dtpGenerator,
        skeleton,
        customDate.length(),
        UDATPG_MATCH_ALL_FIELDS_LENGTH,
        bestpattern,
        patternLength,
        &status);
  }

  // if timezone is specified, use that instead, else use default
  if (!timeZone.empty()) {
    const UChar *timeZoneRes =
        reinterpret_cast<const UChar *>(timeZone.c_str());
    int32_t timeZoneLength = timeZone.length();
    return udat_open(
        UDAT_PATTERN,
        UDAT_PATTERN,
        locale8,
        timeZoneRes,
        timeZoneLength,
        bestpattern,
        patternLength,
        &status);
  } else {
    return udat_open(
        UDAT_PATTERN,
        UDAT_PATTERN,
        locale8,
        0,
        -1,
        bestpattern,
        patternLength,
        &status);
  }
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
