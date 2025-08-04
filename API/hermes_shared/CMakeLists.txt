# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.
#
# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

set(HERMES_ENABLE_EH_RTTI ON)

add_subdirectory(inspector)

add_library(hermesRuntimeApi INTERFACE
  hermes_api.h
  js_runtime_api.h
)

target_include_directories(hermesRuntimeApi INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(libshared SHARED
  hermes_api.cpp
  MurmurHash.cpp
  MurmurHash.h
  ScriptStore.h
)

target_link_libraries(libshared
  PUBLIC
    hermesRuntimeApi
    jsi
    nodeApi
  PRIVATE
    hermesinspector
    hermesNodeApi
    hermesParser
    hermesVMRuntime
    libhermes
)
target_link_options(libshared PRIVATE ${HERMES_EXTRA_LINKER_FLAGS})

if(WIN32)
  configure_file(version.rc.in version.rc @ONLY)
  target_sources(libshared PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()

# Export the required header directory
target_include_directories(libshared PUBLIC
  ..
  ../../public
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${HERMES_JSI_DIR}
  ${REACT_NATIVE_SOURCE}/ReactCommon
  ${REACT_NATIVE_SOURCE}/ReactCommon/runtimeexecutor
  ${REACT_NATIVE_SOURCE_72}/ReactCommon)

if(MSVC)
  # Same as above, but for windows. Note that there is no equivalent of -fvisibility=default.
  set(compile_flags "")

  # Enable exception
  set(compile_flags "${compile_flags} /EHsc")

  # Enable RTTI
  set(compile_flags "${compile_flags} /GR")

  # Generate PDBs
  set(compile_flags "${compile_flags} /Zi")
endif()

target_link_options(libshared PRIVATE ${HERMES_EXTRA_LINKER_FLAGS})

set_target_properties(libshared PROPERTIES
  COMPILE_FLAGS "${compile_flags}"

  # To make sure that the resulting DLL name is hermes.dll
  OUTPUT_NAME hermes
)

if(MSVC)
  # Temporary avoid the auto-vectorization optimization since VS 17.14.0 produces incorrect code.
  # Set /O1 only for Release configs using modern CMake target_compile_options.
  target_compile_options(libshared PRIVATE $<$<CONFIG:Release>:/O1>)
endif()
