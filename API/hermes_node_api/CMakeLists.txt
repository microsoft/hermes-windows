# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

set(HERMES_ENABLE_EH_RTTI ON)

add_subdirectory(node_api)

add_hermes_library(hermesNodeApi
  hermes_node_api.h
  hermes_node_api.cpp
)

target_link_libraries(hermesNodeApi
  PUBLIC nodeApi
  PRIVATE hermesVMRuntime
)

target_include_directories(hermesNodeApi
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)
