# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

if(${HERMES_ENABLE_INTL})
  # TODO: Fix the test compilation.
  # set(PlatformIntlSources getCanonicalLocales.cpp DateTimeFormat.cpp)

  # add_hermes_unittest(PlatformIntlTests
  #   ${PlatformIntlSources}
  # )

  # target_link_libraries(PlatformIntlTests hermesapi)
endif()

add_hermes_unittest(BCP47ParserTests BCP47ParserTest.cpp)
target_link_libraries(BCP47ParserTests hermesBCP47Parser)
