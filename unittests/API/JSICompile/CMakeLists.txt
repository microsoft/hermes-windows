# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

# This directory alone opts out of the shipping JSI epoch. Do not link these
# objects to jsi, hermes.dll, or one another: their C++ contracts differ.
get_directory_property(jsi_compile_definitions COMPILE_DEFINITIONS)
list(FILTER jsi_compile_definitions EXCLUDE REGEX "^JSI_VERSION(=|$)")
set_property(DIRECTORY PROPERTY COMPILE_DEFINITIONS
  ${jsi_compile_definitions})

# OBJECT libraries participate in the default build, but have no link step.
# Compile jsi.cpp at the same epoch as each header/decorator consumer.
foreach(target JSICompileVersion20 JSICompileDefault21)
  add_library(${target} OBJECT
    PublicHeadersCompile.cpp
    ${HERMES_JSI_DIR}/jsi/jsi.cpp)
  hermes_update_cxx_flags(${target})
  target_include_directories(${target} PRIVATE ${HERMES_JSI_DIR})
  set_target_properties(${target} PROPERTIES FOLDER "Tests")
endforeach()

target_compile_definitions(JSICompileVersion20 PRIVATE
  JSI_VERSION=20 EXPECTED_JSI_VERSION=20)
# Intentionally no JSI_VERSION: exercise the public header's fallback.
target_compile_definitions(JSICompileDefault21 PRIVATE
  EXPECTED_JSI_VERSION=21)
