# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

add_library(nodeApi INTERFACE
  js_native_api_types.h
  js_native_api.h)

target_include_directories(nodeApi INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

install(DIRECTORY "${PROJECT_SOURCE_DIR}/API/node_api/" DESTINATION include
  FILES_MATCHING PATTERN "*.h")
