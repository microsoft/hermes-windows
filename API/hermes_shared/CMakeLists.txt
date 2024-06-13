# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

set(HERMES_ENABLE_EH_RTTI ON)

add_subdirectory(node-api)
add_subdirectory(inspector)

# List the files that define exported functions explicitly, and they can link
# against the internal functionality they need.
set(api_sources
  hermes_napi.cpp
  MurmurHash.cpp
  hermes_win.cpp)

add_library(libshared SHARED ${api_sources})
target_link_libraries(libshared PRIVATE
  jsi
  libhermes
  hermesVMRuntime
  hermesNodeApi
  hermesinspector
  hermesParser)
target_link_options(libshared PRIVATE ${HERMES_EXTRA_LINKER_FLAGS})

if (WIN32)
  configure_file(version.rc.in version.rc @ONLY)
  target_sources(libshared PRIVATE  ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()

# Export the required header directory
target_include_directories(libshared PUBLIC
  ..
  ../../public
  node-api
  ${HERMES_JSI_DIR}
  ${REACT_NATIVE_SOURCE}/ReactCommon
  ${REACT_NATIVE_SOURCE}/ReactCommon/runtimeexecutor
  ${REACT_NATIVE_SOURCE_72}/ReactCommon)

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
  # Same as above, but for windows. Note that there is no equivalent of -fvisibility=default.
  set(compile_flags "")

  # Enable exeption
  set(compile_flags "${compile_flags} /EHsc")

  # Enable RTTI
  set(compile_flags "${compile_flags} /GR")

  # Generate PDBs
  set(compile_flags "${compile_flags} /Zi")

  set(link_flags_debug "/DEBUG")
  # Use /OPT:NOICF because hermes associates function pointer with its name.
  # The optimization that merges functions with the same body breaks that code.
  # See getFunctionNameImpl in lib\VM\JSNativeFunctions.cpp.
  set(link_flags_release "/DEBUG;/DEBUGTYPE:CV$<COMMA>FIXUP;/OPT:REF;/OPT:NOICF;/INCREMENTAL:NO")
  if(CMAKE_VS_PLATFORM_NAME MATCHES "^(x64|x86|Win32)$")
    # CETCOMPAT is not supported by ARM or ARM64
    list(APPEND link_flags_release "/CETCOMPAT")
  endif()
endif()

set_target_properties(libshared PROPERTIES
  COMPILE_FLAGS "${compile_flags}"
  LINK_OPTIONS "$<IF:$<CONFIG:Debug>,${link_flags_debug},${link_flags_release}>"
  # To make sure that the resulting DLL name is hermes.dll
  OUTPUT_NAME hermes
)
