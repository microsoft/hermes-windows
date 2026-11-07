# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

# Turn on EH and RTTI for NodeApiJsiTest
set(HERMES_ENABLE_EH ON)
set(HERMES_ENABLE_RTTI ON)
# For some reason (bug?) add_unittest() is clearing LLVM_REQUIRES_RTTI, so
# we need to set this one.
set(LLVM_ENABLE_RTTI ON)

add_hermes_unittest(NodeApiJsiTest
  NodeApiJsiTestFactory.cpp
  ${HERMES_JSI_DIR}/jsi/test/testlib.cpp
  ${HERMES_JSI_DIR}/jsi/test/testlib_ext.cpp
)

target_link_libraries(NodeApiJsiTest
  hermesNodeApiJsiRuntime
)

# Refresh even when only hermes.dll changes and the test does not relink.
add_custom_target(NodeApiJsiTest_copy_dll ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:NodeApiJsiTest>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:libshared>
    $<TARGET_FILE_DIR:NodeApiJsiTest>
  DEPENDS libshared
  VERBATIM
)
add_dependencies(NodeApiJsiTest NodeApiJsiTest_copy_dll)
