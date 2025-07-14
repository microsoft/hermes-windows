# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

add_library(hermesNodeApi INTERFACE
  js_native_api_types.h
  js_native_api.h
  js_runtime_api.h)

target_include_directories(hermesNodeApi INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

install(DIRECTORY "${PROJECT_SOURCE_DIR}/API/node-api/" DESTINATION include
  FILES_MATCHING PATTERN "*.h")
