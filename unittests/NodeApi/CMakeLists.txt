# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

# Registers new test module.
# The caller must provide a set of SOURCES files. It can also provide an
# optional set of DEFINES as the macro definitions such as the "NAPI_VERSION=10"
# used for the target.
function(add_node_api_module MODULE_TARGET)
  cmake_parse_arguments(PARSE_ARGV 0 ARG "" "" "SOURCES;DEFINES")

  # Ensure unique module target name.
  get_filename_component(FOLDER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

  if(NOT ${MODULE_TARGET} STREQUAL ${FOLDER_NAME})
    # Prefix the target name are with the test folder name to avoid duplicate
    # target names between different tests. It is only done when the module
    # target name is different from the test folder name since the folder name
    # is usually unique between tests.
    set(MODULE_TARGET "${FOLDER_NAME}_${MODULE_TARGET}")
  endif()

  add_library(${MODULE_TARGET} MODULE)
  target_sources(${MODULE_TARGET} PRIVATE ${ARG_SOURCES})
  target_link_libraries(${MODULE_TARGET} PRIVATE libshared)
  target_compile_definitions(${MODULE_TARGET} PRIVATE ${ARG_DEFINES})

  set(MODULE_OUTPUT_DIR
    ${CMAKE_CURRENT_BINARY_DIR}/build/$<IF:$<CONFIG:Debug>,Debug,Release>)
  set_target_properties(${MODULE_TARGET} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${MODULE_OUTPUT_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${MODULE_OUTPUT_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${MODULE_OUTPUT_DIR}
    SUFFIX .node
  )
endfunction()

add_subdirectory(test)

# Turn on EH and RTTI for node_lite and NodeApiTests
set(HERMES_ENABLE_EH_RTTI ON)

# For some reason (bug?) add_unittest() is clearing LLVM_REQUIRES_RTTI, so
# we need to set this one.
set(LLVM_ENABLE_RTTI ON)

add_hermes_executable(node_lite
  child_process.cpp
  child_process.h
  compat.h
  node_lite.cpp
  node_lite.h
  node_lite_hermes.cpp
  node_lite_windows.cpp
  string_utils.cpp
  string_utils.h
)
target_link_libraries(node_lite PRIVATE libshared)
target_include_directories(node_lite
  PRIVATE
    ../../API/hermes_node_api/node_api
    ../../API/hermes_shared
)

target_include_directories(node_lite
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test
)

# copy hermes.dll next to the node_lite.exe
add_custom_command(TARGET node_lite POST_BUILD
  VERBATIM
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:libshared>
    $<TARGET_FILE_DIR:node_lite>
)

add_hermes_unittest(NodeApiTests
  child_process.cpp
  child_process.h
  string_utils.cpp
  string_utils.h
  test_basics.cpp
  test_main.cpp
  test_main.h
)

target_link_libraries(NodeApiTests libshared)

# transform test JS files to be Hermes-compatible
add_dependencies(NodeApiTests transformJSFiles node_lite)
