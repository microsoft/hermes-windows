/*
 * Copyright (c) Microsoft Corporation.
 * Licensed under the MIT License.
 */


#include "hermes/Platform/Intl/PlatformIntl.h"
#include <hermes/hermes.h>
#include "hermes/VM/Runtime.h"

#include "gtest/gtest.h"

namespace {
using namespace hermes;
using namespace hermes::platform_intl;

// the simplest of testcases
TEST(getCanonicalLocales, SimpleSingleElement) {
  std::vector<std::u16string> input = std::vector<std::u16string>{u"en-us"};
  std::shared_ptr<hermes::vm::Runtime> runtime = hermes::vm::Runtime::create(
      hermes::vm::RuntimeConfig::Builder().withIntl(true).build());
  auto actual = platform_intl::getCanonicalLocales(*runtime.get(), input);
  auto value = actual.getValue().front();
  auto status = actual.getStatus();
  EXPECT_TRUE(status == vm::ExecutionStatus::RETURNED);
  EXPECT_TRUE(u"en-US" == value);

  input = std::vector<std::u16string>{u"FR"};
  actual = getCanonicalLocales(*runtime.get(), input);
  value = actual.getValue().front();
  status = actual.getStatus();
  EXPECT_TRUE(status == vm::ExecutionStatus::RETURNED);
  EXPECT_TRUE(u"fr" == value);
}

TEST(getCanonicalLocales, SimpleMulipleElement) {
  std::shared_ptr<hermes::vm::Runtime> runtime = hermes::vm::Runtime::create(
      hermes::vm::RuntimeConfig::Builder().withIntl(true).build());
  std::vector<std::u16string> input =
      std::vector<std::u16string>{u"en-us", u"FR"};
  auto actual = getCanonicalLocales(*runtime.get(), input);
  auto value = actual.getValue();
  auto status = actual.getStatus();
  EXPECT_TRUE(status == vm::ExecutionStatus::RETURNED);
  std::vector<std::u16string> expected =
      std::vector<std::u16string>{u"en-US", u"fr"};
  EXPECT_TRUE(value == expected);

  input = std::vector<std::u16string>{u"en-us", u"FR", u"zh-zh", u"ZH"};
  expected = std::vector<std::u16string>{u"en-US", u"fr", u"zh-ZH", u"zh"};
  actual = getCanonicalLocales(*runtime.get(), input);
  value = actual.getValue();
  status = actual.getStatus();
  EXPECT_TRUE(status == vm::ExecutionStatus::RETURNED);
  EXPECT_TRUE(value == expected);
}

TEST(getCanonicalLocales, ComplexSingleElement) {
  std::vector<std::u16string> input =
      std::vector<std::u16string>{u"cmn-hans-cn-t-ca-u-ca-a-blt-x-t-u"};
  std::shared_ptr<hermes::vm::Runtime> runtime = hermes::vm::Runtime::create(
      hermes::vm::RuntimeConfig::Builder().withIntl(true).build());
  auto actual = getCanonicalLocales(*runtime.get(), input);
  auto status = actual.getStatus();
  EXPECT_TRUE(status == vm::ExecutionStatus::RETURNED);
  auto value = actual.getValue().front();  
  EXPECT_TRUE(u"cmn-Hans-CN-a-blt-t-ca-u-ca-x-t-u" == value);

  input = std::vector<std::u16string>{u"en-us-u-asd-a-tbd-0-abc"};
  actual = getCanonicalLocales(*runtime.get(), input);
  value = actual.getValue().front();
  status = actual.getStatus();
  EXPECT_TRUE(status == vm::ExecutionStatus::RETURNED);
  EXPECT_TRUE(u"en-US-0-abc-a-tbd-u-asd" == value);
}

TEST(getCanonicalLocales, EmptyEdgeCases) {
  std::vector<std::u16string> input = std::vector<std::u16string>{};
  std::shared_ptr<hermes::vm::Runtime> runtime = hermes::vm::Runtime::create(
      hermes::vm::RuntimeConfig::Builder().withIntl(true).build());
  auto actual = getCanonicalLocales(*runtime.get(), input);
  auto value = actual.getValue();
  auto status = actual.getStatus();
  EXPECT_TRUE(status == vm::ExecutionStatus::RETURNED);
  EXPECT_TRUE(value.size() == 0);

  input = std::vector<std::u16string>{u""};
  status = getCanonicalLocales(*runtime.get(), input).getStatus();
  EXPECT_TRUE(status == vm::ExecutionStatus::EXCEPTION);
}

TEST(getCanonicalLocales, ErrorCases) {
  std::vector<std::u16string> input = std::vector<std::u16string>{u"en_uk"};
  std::shared_ptr<hermes::vm::Runtime> runtime = hermes::vm::Runtime::create(
      hermes::vm::RuntimeConfig::Builder().withIntl(true).build());
  auto status = getCanonicalLocales(*runtime.get(), input).getStatus();
  EXPECT_TRUE(status == vm::ExecutionStatus::EXCEPTION);

  input = std::vector<std::u16string>{u"und-t-en-us-t-en-us"};
  status = getCanonicalLocales(*runtime.get(), input).getStatus();
  EXPECT_TRUE(status == vm::ExecutionStatus::EXCEPTION);

  input = std::vector<std::u16string>{u"uEN-Us-u-x-test"};
  status = getCanonicalLocales(*runtime.get(), input).getStatus();
  EXPECT_TRUE(status == vm::ExecutionStatus::EXCEPTION);
}

} // end anonymous namespace