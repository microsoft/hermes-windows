# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

# We are building 'hermesinspector.dll' using code from react-native, folly, boost etc.

add_subdirectory(boost)
add_subdirectory(folly)
add_subdirectory(react-native)
add_subdirectory(react-native-0.72)

include(CMakePrintHelpers)
cmake_print_variables(BOOST_INCLUDES)
cmake_print_variables(FOLLY_SOURCE)
cmake_print_variables(REACT_NATIVE_SOURCE)
cmake_print_variables(REACT_NATIVE_SOURCE_72)

set(inspector_sources
  ${REACT_NATIVE_SOURCE_72}/ReactCommon/hermes/inspector/Inspector.cpp
  ${REACT_NATIVE_SOURCE_72}/ReactCommon/hermes/inspector/InspectorState.cpp
  ${REACT_NATIVE_SOURCE_72}/ReactCommon/hermes/inspector/RuntimeAdapter.cpp
  ${REACT_NATIVE_SOURCE_72}/ReactCommon/hermes/inspector/detail/CallbackOStream.cpp
  ${REACT_NATIVE_SOURCE_72}/ReactCommon/hermes/inspector/detail/SerialExecutor.cpp
  ${REACT_NATIVE_SOURCE_72}/ReactCommon/hermes/inspector/detail/Thread.cpp
  ${REACT_NATIVE_SOURCE_72}/ReactCommon/hermes/inspector/chrome/Connection.cpp
  ${REACT_NATIVE_SOURCE_72}/ReactCommon/hermes/inspector/chrome/ConnectionDemux.cpp
  ${REACT_NATIVE_SOURCE_72}/ReactCommon/hermes/inspector/chrome/MessageConverters.cpp
  ${REACT_NATIVE_SOURCE_72}/ReactCommon/hermes/inspector/chrome/MessageTypes.cpp
  ${REACT_NATIVE_SOURCE_72}/ReactCommon/hermes/inspector/chrome/Registration.cpp
  ${REACT_NATIVE_SOURCE_72}/ReactCommon/hermes/inspector/chrome/RemoteObjectsTable.cpp
)

list(APPEND inspector_sources
  ${REACT_NATIVE_SOURCE}/ReactCommon/hermes/inspector-modern/chrome/ConnectionDemux.cpp
  ${REACT_NATIVE_SOURCE}/ReactCommon/hermes/inspector-modern/chrome/HermesRuntimeAgentDelegate.cpp 
  ${REACT_NATIVE_SOURCE}/ReactCommon/hermes/inspector-modern/chrome/Registration.cpp 
)

list(APPEND inspector_sources
   ../../jsi/jsi/JSIDynamic.cpp
)

list(APPEND inspector_sources
   InspectorProxy.cpp
)

list(APPEND folly_sources
  ${FOLLY_SOURCE}/folly/concurrency/CacheLocality.cpp
  ${FOLLY_SOURCE}/folly/Conv.cpp
  ${FOLLY_SOURCE}/folly/Demangle.cpp
  ${FOLLY_SOURCE}/folly/detail/AsyncTrace.cpp
  ${FOLLY_SOURCE}/folly/detail/AtFork.cpp
  ${FOLLY_SOURCE}/folly/detail/Futex.cpp
  ${FOLLY_SOURCE}/folly/detail/MemoryIdler.cpp
  ${FOLLY_SOURCE}/folly/detail/StaticSingletonManager.cpp
  ${FOLLY_SOURCE}/folly/detail/ThreadLocalDetail.cpp
  ${FOLLY_SOURCE}/folly/detail/UniqueInstance.cpp
  ${FOLLY_SOURCE}/folly/ExceptionWrapper.cpp
  ${FOLLY_SOURCE}/folly/Executor.cpp
  ${FOLLY_SOURCE}/folly/executors/InlineExecutor.cpp
  ${FOLLY_SOURCE}/folly/executors/QueuedImmediateExecutor.cpp
  ${FOLLY_SOURCE}/folly/Format.cpp
  ${FOLLY_SOURCE}/folly/hash/SpookyHashV2.cpp
  ${FOLLY_SOURCE}/folly/io/async/Request.cpp
  ${FOLLY_SOURCE}/folly/lang/Assume.cpp
  ${FOLLY_SOURCE}/folly/lang/CString.cpp
  ${FOLLY_SOURCE}/folly/lang/SafeAssert.cpp
  ${FOLLY_SOURCE}/folly/memory/detail/MallocImpl.cpp
  ${FOLLY_SOURCE}/folly/memory/MallctlHelper.cpp
  ${FOLLY_SOURCE}/folly/portability/SysMembarrier.cpp
  ${FOLLY_SOURCE}/folly/ScopeGuard.cpp
  ${FOLLY_SOURCE}/folly/SharedMutex.cpp
  ${FOLLY_SOURCE}/folly/String.cpp
  ${FOLLY_SOURCE}/folly/synchronization/AsymmetricMemoryBarrier.cpp
  ${FOLLY_SOURCE}/folly/synchronization/Hazptr.cpp
  ${FOLLY_SOURCE}/folly/synchronization/ParkingLot.cpp
  ${FOLLY_SOURCE}/folly/synchronization/SanitizeThread.cpp
  ${FOLLY_SOURCE}/folly/dynamic.cpp
  ${FOLLY_SOURCE}/folly/json.cpp
  ${FOLLY_SOURCE}/folly/json_pointer.cpp
  ${FOLLY_SOURCE}/folly/Unicode.cpp
)

list(APPEND folly_sources
  ./stubs/windows_portability_tinyimp.cpp
  ./stubs/posix_stubs.cpp
)

add_definitions(-DFOLLY_MOBILE)
add_definitions(-DFOLLY_NO_CONFIG)
add_definitions(-DNOMINMAX)
add_definitions(-DFOLLY_EXPORT=)
add_definitions(-D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)

include_directories(${BOOST_INCLUDES})
include_directories(${FOLLY_SOURCE})
include_directories(${REACT_NATIVE_SOURCE}/ReactCommon)
include_directories(${REACT_NATIVE_SOURCE_72}/ReactCommon)
include_directories(stubs)

set(REACT_NATIVE_SOURCE ${REACT_NATIVE_SOURCE} PARENT_SCOPE)
set(REACT_NATIVE_SOURCE_72 ${REACT_NATIVE_SOURCE_72} PARENT_SCOPE)

file(GLOB inspector_headers ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
file(GLOB inspector_public_headers ${PROJECT_SOURCE_DIR}/public/hermes/Public/*.h)

# Restore EH and RTTI (Note: At the time of writing, there is no usage of
# add_hermes_library either after this line in this file or in a sub directory.
# However, the values are restored anyways for the sake of sanity.)
set(HERMES_ENABLE_EH ON)
set(HERMES_ENABLE_RTTI ON)

configure_file(version.rc.in version.rc @ONLY)

add_hermes_library(hermesinspector
  ${inspector_sources}
  ${inspector_headers}
  ${folly_sources}
  ${inspector_public_headers}
  ${CMAKE_CURRENT_BINARY_DIR}/version.rc)

#target_compile_features(hermesinspector PUBLIC cxx_std_20)

target_link_libraries(hermesinspector
  hermesNodeApi
  hermesPublic
  jsi
)
hermes_link_icu(hermesinspector)

# Export the required header directory
target_include_directories(hermesinspector PUBLIC
  ../..
  ${FOLLY_SOURCE}
  ${REACT_NATIVE_SOURCE}/ReactCommon
  ${REACT_NATIVE_SOURCE}/ReactCommon/runtimeexecutor
  ${REACT_NATIVE_SOURCE_72}/ReactCommon)

# Same as above, but for windows. Note that there is no equivalent of -fvisibility=default.
set(compile_flags " /await ")

# Assuming inspector don't use exceptions
set(compile_flags "${compile_flags} /EHs-c-")

# inspector use rtti
set(compile_flags "${compile_flags} /GR")

# Generate PDBs
set(compile_flags "${compile_flags} /Zi")

set(link_flags_debug "/DEBUG")
set(link_flags_release "/DEBUG;/OPT:REF;/OPT:ICF;/INCREMENTAL:NO")
if(CMAKE_VS_PLATFORM_NAME MATCHES "^(x64|x86|Win32)$")
  # CETCOMPAT is not supported by ARM or ARM64
  list(APPEND link_flags_release "/CETCOMPAT")
endif()

set_target_properties(hermesinspector PROPERTIES
  COMPILE_FLAGS "${compile_flags}"
  LINK_OPTIONS "$<IF:$<CONFIG:Debug>,${link_flags_debug},${link_flags_release}>"
  OUTPUT_NAME hermesinspector
)
