/*
 * Copyright (c) Microsoft Corporation.
 * Licensed under the MIT License.
 */

#include <hermes/hermes.h>
#include "hermes/Platform/Intl/PlatformIntl.h"
#include "hermes/VM/Runtime.h"

#include "gtest/gtest.h"

namespace {
using namespace hermes;
using namespace hermes::platform_intl;

// simplest of testcases, tests one locale without any options
TEST(DateTimeFormat, DatesWithoutOptions) {
  std::vector<std::u16string> AmericanEnglish =
      std::vector<std::u16string>{u"en-us"};
  std::vector<std::u16string> KoreanKorea =
      std::vector<std::u16string>{u"ko-KR"};
  std::vector<std::u16string> french = std::vector<std::u16string>{u"fr"};
  std::shared_ptr<hermes::vm::Runtime> runtime = hermes::vm::Runtime::create(
      hermes::vm::RuntimeConfig::Builder().withIntl(true).build());
  platform_intl::Options testOptions = {};

  auto dtf = platform_intl::DateTimeFormat::create(
      *runtime.get(), AmericanEnglish, testOptions);
  auto result = dtf.getValue().get()->format(1620000000000.00);
  EXPECT_EQ(result, u"5/2/2021");

  auto dtf2 = platform_intl::DateTimeFormat::create(
      *runtime.get(), KoreanKorea, testOptions);
  auto result2 = dtf2.getValue().get()->format(1620000000000.00);
  EXPECT_EQ(result2, u"2021. 5. 2.");

  auto dtf3 = platform_intl::DateTimeFormat::create(
      *runtime.get(), french, testOptions);
  std::u16string result3 = dtf3.getValue().get()->format(1620000000000.00);
  EXPECT_EQ(result3, u"02/05/2021");
}

// tests dateStyle and timeStyle options (full, long, medium, short)
TEST(DateTimeFormat, DatesWithTimeDateStyles) {
  std::vector<std::u16string> AmericanEnglish =
      std::vector<std::u16string>{u"en-us"};
  std::vector<std::u16string> SpanishPeru =
      std::vector<std::u16string>{u"es-PE"};
  std::vector<std::u16string> french = std::vector<std::u16string>{u"fr"};
  std::shared_ptr<hermes::vm::Runtime> runtime = hermes::vm::Runtime::create(
      hermes::vm::RuntimeConfig::Builder().withIntl(true).build());

  // dateStyle = full and timeStye = full
  Options testOptions = {
      {u"dateStyle", Option(std::u16string(u"full"))},
      {u"timeStyle", Option(std::u16string(u"full"))}};
  auto dtf = platform_intl::DateTimeFormat::create(
      *runtime.get(), AmericanEnglish, testOptions);
  auto result = dtf.getValue().get()->format(1620000000000.00);
  EXPECT_EQ(result, u"Sunday, May 2, 2021 at 5:00:00 PM Pacific Daylight Time");

  auto dtf2 = platform_intl::DateTimeFormat::create(
      *runtime.get(), french, testOptions);
  auto result2 = dtf2.getValue().get()->format(1620000000000.00);
  EXPECT_EQ(
      result2,
      u"dimanche 2 mai 2021 \u00E0 17:00:00 heure d\u2019\u00E9t\u00E9 du Pacifique"); // L"dimanche 2 mai 2021 à 17:00:00 heure d’été du Pacifique"

  auto dtf3 = platform_intl::DateTimeFormat::create(
      *runtime.get(), SpanishPeru, testOptions);
  auto result3 = dtf3.getValue().get()->format(1620000000000.00);
  EXPECT_EQ(
      result3,
      u"domingo, 2 de mayo de 2021, 17:00:00 hora de verano del Pac\u00EDfico"); // L"domingo, 2 de mayo de 2021, 17:00:00 hora de verano del Pacífico"

  // dateStyle = short and timeStyle = short
  Options testOptions2 = {
      {u"dateStyle", Option(std::u16string(u"short"))},
      {u"timeStyle", Option(std::u16string(u"short"))}};
  auto dtf4 = platform_intl::DateTimeFormat::create(
      *runtime.get(), AmericanEnglish, testOptions2);
  auto result4 = dtf4.getValue().get()->format(1620000000000.00);
  EXPECT_EQ(result4, u"5/2/21, 5:00 PM");

  auto dtf5 = platform_intl::DateTimeFormat::create(
      *runtime.get(), french, testOptions2);
  auto result5 = dtf5.getValue().get()->format(1620000000000.00);
  EXPECT_EQ(result5, u"02/05/2021 17:00");

  auto dtf6 = platform_intl::DateTimeFormat::create(
      *runtime.get(), SpanishPeru, testOptions2);
  auto result6 = dtf6.getValue().get()->format(1620000000000.00);
  EXPECT_EQ(result6, u"2/05/21 17:00");

  // dateStyle = long and timeStyle = medium
  Options testOptions3 = {
      {u"dateStyle", Option(std::u16string(u"long"))},
      {u"timeStyle", Option(std::u16string(u"medium"))}};
  auto dtf7 = platform_intl::DateTimeFormat::create(
      *runtime.get(), AmericanEnglish, testOptions3);
  auto result7 = dtf7.getValue().get()->format(1620000000000.00);
  EXPECT_EQ(result7, u"May 2, 2021 at 5:00:00 PM");

  auto dtf8 = platform_intl::DateTimeFormat::create(
      *runtime.get(), french, testOptions3);
  auto result8 = dtf8.getValue().get()->format(1620000000000.00);
  EXPECT_EQ(result8, u"2 mai 2021 \u00E0 17:00:00"); // L"2 mai 2021 à 17:00:00"

  auto dtf9 = platform_intl::DateTimeFormat::create(
      *runtime.get(), SpanishPeru, testOptions3);
  auto result9 = dtf9.getValue().get()->format(1620000000000.00);
  EXPECT_EQ(result9, u"2 de mayo de 2021, 17:00:00");
}

// Tests Date with Month (2-digit, numeric, narrow, short, long), Day (2-digit,
// numeric), and Year (2-digit, numeric) options
TEST(DateTimeFormat, DatesWithMonthDayYearOptions) {
  std::vector<std::u16string> DutchBelgium =
      std::vector<std::u16string>{u"nl-BE"};
  std::shared_ptr<hermes::vm::Runtime> runtime = hermes::vm::Runtime::create(
      hermes::vm::RuntimeConfig::Builder().withIntl(true).build());

  Options testOptions = {
      {u"day", Option(std::u16string(u"numeric"))},
      {u"month", Option(std::u16string(u"long"))},
      {u"year", Option(std::u16string(u"numeric"))}};
  auto dtf = platform_intl::DateTimeFormat::create(
      *runtime.get(), DutchBelgium, testOptions);
  auto result = dtf.getValue().get()->format(1620000000000.00);
  EXPECT_EQ(result, u"2 mei 2021");

  Options testOptions2 = {
      {u"day", Option(std::u16string(u"2-digit"))},
      {u"month", Option(std::u16string(u"narrow"))},
      {u"year", Option(std::u16string(u"2-digit"))}};
  auto dtf2 = platform_intl::DateTimeFormat::create(
      *runtime.get(), DutchBelgium, testOptions2);
  auto result2 = dtf2.getValue().get()->format(1620000000000.00);
  EXPECT_EQ(result2, u"02 M 21");

  Options testOptions3 = {
      {u"month", Option(std::u16string(u"numeric"))},
      {u"year", Option(std::u16string(u"2-digit"))}};
  auto dtf3 = platform_intl::DateTimeFormat::create(
      *runtime.get(), DutchBelgium, testOptions3);
  auto result3 = dtf3.getValue().get()->format(1620000000000.00);
  EXPECT_EQ(result3, u"5/21");
}

// Tests Date with Weekday ( narrow, short, long), era ( narrow,
// short, long), TimeZoneName (short, long, shortOffset, longOffset,
// shortGeneric, longGeneric)
TEST(DateTimeFormat, DatesWithWeekdayEraTimeZoneNameOptions) {
  std::vector<std::u16string> ItalianItaly =
      std::vector<std::u16string>{u"it-IT"}; // it-IT
  std::shared_ptr<hermes::vm::Runtime> runtime = hermes::vm::Runtime::create(
      hermes::vm::RuntimeConfig::Builder().withIntl(true).build());

  Options testOptions = {
      {u"weekday", Option(std::u16string(u"long"))},
      {u"era", Option(std::u16string(u"long"))},
      {u"timeZoneName", Option(std::u16string(u"long"))}};
  auto dtf = platform_intl::DateTimeFormat::create(
      *runtime.get(), ItalianItaly, testOptions);
  auto result = dtf.getValue().get()->format(1620000000000.00);
  EXPECT_EQ(result, u"dopo Cristo domenica, Ora legale del Pacifico USA");

  Options testOptions2 = {
      {u"weekday", Option(std::u16string(u"short"))},
      {u"era", Option(std::u16string(u"narrow"))},
      {u"timeZoneName", Option(std::u16string(u"shortOffset"))}};
  auto dtf2 = platform_intl::DateTimeFormat::create(
      *runtime.get(), ItalianItaly, testOptions2);
  auto result2 = dtf2.getValue().get()->format(1620000000000.00);
  EXPECT_EQ(result2, u"dC dom, GMT-7");

  Options testOptions3 = {
      {u"weekday", Option(std::u16string(u"narrow"))},
      {u"era", Option(std::u16string(u"short"))},
      {u"timeZoneName", Option(std::u16string(u"longGeneric"))}};
  auto dtf3 = platform_intl::DateTimeFormat::create(
      *runtime.get(), ItalianItaly, testOptions3);
  auto result3 = dtf3.getValue().get()->format(1620000000000.00);
  EXPECT_EQ(result3, u"d.C. D, Ora del Pacifico USA");
}

// Tests Date with Hour (2-digit, numeric), Minute (2-digit, numeric), Second
// (2-digit, numeric)
TEST(DateTimeFormat, DatesWithHourMinuteSecondOptions) {
  std::vector<std::u16string> AmericanEnglish =
      std::vector<std::u16string>{u"en-US"};
  std::shared_ptr<hermes::vm::Runtime> runtime = hermes::vm::Runtime::create(
      hermes::vm::RuntimeConfig::Builder().withIntl(true).build());

  Options testOptions = {
      {u"hour", Option(std::u16string(u"2-digit"))},
      {u"minute", Option(std::u16string(u"2-digit"))},
      {u"second", Option(std::u16string(u"2-digit"))}};
  auto dtf = platform_intl::DateTimeFormat::create(
      *runtime.get(), AmericanEnglish, testOptions);
  auto result = dtf.getValue().get()->format(1620000303000);
  EXPECT_EQ(result, u"05:05:03 PM");

  Options testOptions2 = {
      {u"hour", Option(std::u16string(u"numeric"))},
      {u"minute", Option(std::u16string(u"numeric"))},
      {u"second", Option(std::u16string(u"numeric"))}};
  auto dtf2 = platform_intl::DateTimeFormat::create(
      *runtime.get(), AmericanEnglish, testOptions2);
  auto result2 = dtf2.getValue().get()->format(1620000303000);
  EXPECT_EQ(result2, u"5:05:03 PM");

  Options testOptions3 = {{u"minute", Option(std::u16string(u"2-digit"))}};
  auto dtf3 = platform_intl::DateTimeFormat::create(
      *runtime.get(), AmericanEnglish, testOptions3);
  auto result3 = dtf3.getValue().get()->format(1620000303000);
  EXPECT_EQ(result3, u"05");

  Options testOptions4 = {{u"hour", Option(std::u16string(u"2-digit"))}};
  auto dtf4 = platform_intl::DateTimeFormat::create(
      *runtime.get(), AmericanEnglish, testOptions4);
  auto result4 = dtf4.getValue().get()->format(1620000303000);
  EXPECT_EQ(result4, u"05 PM");

  Options testOptions5 = {
      {u"hour", Option(std::u16string(u"2-digit"))},
      {u"second", Option(std::u16string(u"numeric"))}};
  auto dtf5 = platform_intl::DateTimeFormat::create(
      *runtime.get(), AmericanEnglish, testOptions5);
  auto result5 = dtf5.getValue().get()->format(1620000303000);
  EXPECT_EQ(result5, u"05 PM (second: 3)");
}

// Tests Date with HourCycle (h11, h12, h23, h24)
TEST(DateTimeFormat, DatesWithHourCyclesOptions) {
  std::vector<std::u16string> AmericanEnglish =
      std::vector<std::u16string>{u"en-US"};
  std::shared_ptr<hermes::vm::Runtime> runtime = hermes::vm::Runtime::create(
      hermes::vm::RuntimeConfig::Builder().withIntl(true).build());

  Options testOptions = {
      {u"hour", Option(std::u16string(u"numeric"))},
      {u"minute", Option(std::u16string(u"numeric"))},
      {u"hourCycle", Option(std::u16string(u"h12"))}};
  auto dtf = platform_intl::DateTimeFormat::create(
      *runtime.get(), AmericanEnglish, testOptions);
  auto result = dtf.getValue().get()->format(1620008000000);
  EXPECT_EQ(result, u"7:13 PM");

  Options testOptions2 = {
      {u"hour", Option(std::u16string(u"numeric"))},
      {u"minute", Option(std::u16string(u"numeric"))},
      {u"hourCycle", Option(std::u16string(u"h24"))}};
  auto dtf2 = platform_intl::DateTimeFormat::create(
      *runtime.get(), AmericanEnglish, testOptions2);
  auto result2 = dtf2.getValue().get()->format(1620008000000);
  EXPECT_EQ(result2, u"19:13");

  Options testOptions3 = {
      {u"hour", Option(std::u16string(u"numeric"))},
      {u"minute", Option(std::u16string(u"numeric"))},
      {u"hourCycle", Option(std::u16string(u"h11"))}};
  auto dtf3 = platform_intl::DateTimeFormat::create(
      *runtime.get(), AmericanEnglish, testOptions3);
  auto result3 = dtf3.getValue().get()->format(1620008000000);
  EXPECT_EQ(result3, u"7:13 PM");
}

// Tests Date with specified TimeZone
TEST(DateTimeFormat, DatesWithTimeZone) {
  std::vector<std::u16string> AmericanEnglish =
      std::vector<std::u16string>{u"en-US"};
  std::shared_ptr<hermes::vm::Runtime> runtime = hermes::vm::Runtime::create(
      hermes::vm::RuntimeConfig::Builder().withIntl(true).build());

  Options testOptions = {
      {u"dateStyle", Option(std::u16string(u"long"))},
      {u"timeStyle", Option(std::u16string(u"long"))},
      {u"timeZone", Option(std::u16string(u"UTC"))}};
  auto dtf = platform_intl::DateTimeFormat::create(
      *runtime.get(), AmericanEnglish, testOptions);
  auto result = dtf.getValue().get()->format(1620000000000.00);
  EXPECT_EQ(result, u"May 3, 2021 at 12:00:00 AM UTC");

  Options testOptions2 = {
      {u"dateStyle", Option(std::u16string(u"full"))},
      {u"timeStyle", Option(std::u16string(u"full"))},
      {u"timeZone", Option(std::u16string(u"Australia/Sydney"))}};
  auto dtf2 = platform_intl::DateTimeFormat::create(
      *runtime.get(), AmericanEnglish, testOptions2);
  auto result2 = dtf2.getValue().get()->format(1620000000000.00);
  EXPECT_EQ(
      result2,
      u"Monday, May 3, 2021 at 10:00:00 AM Australian Eastern Standard Time");
}

// Tests Date with all options
TEST(DateTimeFormat, DatesWithAllOptions) {
  std::vector<std::u16string> AmericanEnglish =
      std::vector<std::u16string>{u"en-US"};
  std::shared_ptr<hermes::vm::Runtime> runtime = hermes::vm::Runtime::create(
      hermes::vm::RuntimeConfig::Builder().withIntl(true).build());

  Options testOptions = {
      {u"day", Option(std::u16string(u"numeric"))},
      {u"month", Option(std::u16string(u"long"))},
      {u"year", Option(std::u16string(u"numeric"))},
      {u"weekday", Option(std::u16string(u"long"))},
      {u"era", Option(std::u16string(u"long"))},
      {u"timeZoneName", Option(std::u16string(u"long"))},
      {u"hour", Option(std::u16string(u"2-digit"))},
      {u"minute", Option(std::u16string(u"2-digit"))},
      {u"second", Option(std::u16string(u"2-digit"))},
      {u"hourCycle", Option(std::u16string(u"h12"))},
      {u"timeZone", Option(std::u16string(u"UTC"))}};
  auto dtf = platform_intl::DateTimeFormat::create(
      *runtime.get(), AmericanEnglish, testOptions);
  auto result = dtf.getValue().get()->format(1620008000000.00);
  EXPECT_EQ(
      result,
      u"Monday, May 3, 2021 Anno Domini, 02:13:20 AM Coordinated Universal Time");

  Options testOptions2 = {
      {u"day", Option(std::u16string(u"2-digit"))},
      {u"month", Option(std::u16string(u"short"))},
      {u"year", Option(std::u16string(u"2-digit"))},
      {u"weekday", Option(std::u16string(u"short"))},
      {u"era", Option(std::u16string(u"narrow"))},
      {u"timeZoneName", Option(std::u16string(u"longGeneric"))},
      {u"hour", Option(std::u16string(u"2-digit"))},
      {u"minute", Option(std::u16string(u"2-digit"))},
      {u"second", Option(std::u16string(u"2-digit"))},
      {u"hourCycle", Option(std::u16string(u"h24"))},
      {u"timeZone", Option(std::u16string(u"Europe/Madrid"))}};
  auto dtf2 = platform_intl::DateTimeFormat::create(
      *runtime.get(), AmericanEnglish, testOptions2);
  auto result2 = dtf2.getValue().get()->format(1620008000000.00);
  EXPECT_EQ(result2, u"Mon, May 03, 21 A, 04:13:20 Central European Time");
}

// Tests DateTimeFormat.supportedLocalesOf
TEST(DateTimeFormat, SupportedLocales) {
  std::shared_ptr<hermes::vm::Runtime> runtime = hermes::vm::Runtime::create(
      hermes::vm::RuntimeConfig::Builder().withIntl(true).build());

  Options testOptions = {};
  std::vector<std::u16string> expected =
      std::vector<std::u16string>{u"en-US", u"fr"};
  auto result = platform_intl::DateTimeFormat::supportedLocalesOf(
      *runtime.get(), {u"en-us", u"fr"}, testOptions);
  auto value = result.getValue();
  EXPECT_EQ(value, expected);

  std::vector<std::u16string> expected2 =
      std::vector<std::u16string>{u"en-US", u"fr", u"it-IT"};
  auto result2 = platform_intl::DateTimeFormat::supportedLocalesOf(
      *runtime.get(), {u"en-us", u"fr", u"bans", u"it-it"}, testOptions);
  auto value2 = result2.getValue();
  EXPECT_EQ(value2, expected2);
}
} // end anonymous namespace