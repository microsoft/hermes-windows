# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(jsi
        jsi.cpp)

target_include_directories(jsi PUBLIC ..)


set(jsi_compile_flags "")
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang" OR
    "${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
  list(APPEND jsi_compile_flags "-Wno-non-virtual-dtor")
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
  # Turn on Error Handling in MSVC, otherwise objects are not destructed
  # when they go out of scope due to exceptions.
  list(APPEND jsi_compile_flags "/EHsc")
  list(APPEND jsi_compile_flags "/Zi")
  list(APPEND jsi_compile_flags "/Qspectre")
  list(APPEND jsi_compile_flags "/sdl")
endif()
target_compile_options(jsi PRIVATE ${jsi_compile_flags})

install(DIRECTORY "${PROJECT_SOURCE_DIR}/API/jsi/" DESTINATION include
  FILES_MATCHING PATTERN "*.h"
  PATTERN "test" EXCLUDE)
