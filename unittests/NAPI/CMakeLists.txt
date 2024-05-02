# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(js)

set(NAPITests
  napitest_hermes.cpp
  napitest.cpp
  napitest.h
  test_2_function_arguments.cpp
  test_3_callbacks.cpp
  test_4_object_factory.cpp
  test_5_function_factory.cpp
  test_6_object_wrap.cpp
  test_7_factory_wrap.cpp
  test_8_passing_wrapped.cpp
  test_array.cpp
  test_assert.cpp
  test_basics.cpp
  test_bigint.cpp
  test_constructor.cpp
  test_conversions.cpp
  test_dataview.cpp
  test_date.cpp
  test_error.cpp
  test_exception.cpp
  test_ext.cpp
  test_finalizer.cpp
  test_function.cpp
  test_general.cpp
  test_handle_scope.cpp
  test_instance_data.cpp
  test_new_target.cpp
  test_number.cpp
  test_object.cpp
  test_promise.cpp
  test_properties.cpp
  test_reference_double_free.cpp
  test_reference.cpp
  test_string.cpp
  test_symbol.cpp
  test_typedarray.cpp
  )

# Turn on EH and RTTI for APITests
set(HERMES_ENABLE_EH ON)
set(HERMES_ENABLE_RTTI ON)
# For some reason (bug?) add_unittest() is clearing LLVM_REQUIRES_RTTI, so
# we need to set this one.
set(LLVM_ENABLE_RTTI ON)

add_hermes_unittest(NAPITests ${NAPITests} LINK_LIBS hermesNapi)
target_link_libraries(NAPITests hermesNapi libhermes)
target_include_directories(NAPITests PUBLIC ../../API/hermes)

# transform test JS files to be Hermes-compatible
add_dependencies(NAPITests transformJSFiles copyBabelFiles)

# copy hermes.dll next to the NAPITests.exe
add_custom_command(TARGET NAPITests POST_BUILD
  DEPENDS libhermes
  VERBATIM
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  $<TARGET_FILE:libhermes>
  $<TARGET_FILE_DIR:NAPITests>)
