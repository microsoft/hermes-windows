# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

add_custom_target(HermesUnitTests)
set_target_properties(HermesUnitTests PROPERTIES FOLDER "Tests")

function(add_hermes_unittest test_dirname)
  add_unittest(HermesUnitTests ${test_dirname} ${ARGN})
  # gtest uses LLVHSupport, so LLVHSupport needs to be listed after gtest on
  # the link line.
  target_link_libraries(${test_dirname} LLVHSupport)
  target_compile_definitions(${test_dirname} PUBLIC UNIT_TEST)
  if(MSVC)
    # Use the same linker settings as hermes.dll
    set(link_flags_debug "/DEBUG")
    set(link_flags_release "/DEBUG;/DEBUGTYPE:CV$<COMMA>FIXUP;/OPT:REF;/OPT:NOICF;/INCREMENTAL:NO")
    set_target_properties(${test_dirname} PROPERTIES
      LINK_OPTIONS "$<IF:$<CONFIG:Debug>,${link_flags_debug},${link_flags_release}>")
  endif()
endfunction()

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang" OR
    "${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
  # without this, because we build -Wextra, gtest warns when comparing
  # against integer constants
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-sign-compare")
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
  # use of std::tr1 namespace (in GTest headers)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING")
endif()

add_subdirectory(AST)
add_subdirectory(IR)
add_subdirectory(BCGen)
add_subdirectory(Parser)
add_subdirectory(VMRuntime)
add_subdirectory(Support)
add_subdirectory(dtoa)
add_subdirectory(PlatformIntl)
add_subdirectory(PlatformUnicode)
add_subdirectory(API)
add_subdirectory(ADT)
add_subdirectory(Optimizer)
add_subdirectory(node-api)
