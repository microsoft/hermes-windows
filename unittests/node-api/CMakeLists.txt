# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

add_subdirectory(js)

set(NodeApiTests
  node_api_test_hermes.cpp
  node_api_test.cpp
  node_api_test.h
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
set(HERMES_ENABLE_EH_RTTI ON)
# For some reason (bug?) add_unittest() is clearing LLVM_REQUIRES_RTTI, so
# we need to set this one.
set(LLVM_ENABLE_RTTI ON)


# Enable exeption
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")

# Enable RTTI
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR")

# Generate PDBs
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zi")

add_hermes_unittest(NodeApiTests ${NodeApiTests} LINK_LIBS hermesNodeApi)
target_link_libraries(NodeApiTests hermesNodeApi libshared)
target_include_directories(NodeApiTests PUBLIC ../../API/hermes_shared)

# transform test JS files to be Hermes-compatible
add_dependencies(NodeApiTests transformJSFiles copyBabelFiles)

# copy hermes.dll next to the NodeApiTests.exe
add_custom_command(TARGET NodeApiTests POST_BUILD
  DEPENDS libshared
  VERBATIM
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  $<TARGET_FILE:libshared>
  $<TARGET_FILE_DIR:NodeApiTests>)
