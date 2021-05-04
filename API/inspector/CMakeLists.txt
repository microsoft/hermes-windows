# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

# set(REACT_NATIVE_SOURCE E:/github/react-native-windows/node_modules/react-native)
# set(FOLLY_SOURCE E:/github/hermes-windows/Folly/folly-2020.01.13.00)

set(inspector_sources
  ${REACT_NATIVE_SOURCE}/ReactCommon/hermes/inspector/Inspector.cpp
  ${REACT_NATIVE_SOURCE}/ReactCommon/hermes/inspector/Inspector.cpp
  ${REACT_NATIVE_SOURCE}/ReactCommon/hermes/inspector/InspectorState.cpp
  ${REACT_NATIVE_SOURCE}/ReactCommon/hermes/inspector/RuntimeAdapter.cpp
  ${REACT_NATIVE_SOURCE}/ReactCommon/hermes/inspector/detail/Thread.cpp
  ${REACT_NATIVE_SOURCE}/ReactCommon/hermes/inspector/detail/SerialExecutor.cpp
  ${REACT_NATIVE_SOURCE}/ReactCommon/hermes/inspector/detail/CallbackOStream.cpp
)

list(APPEND inspector_sources
  ${REACT_NATIVE_SOURCE}/ReactCommon/hermes/inspector/chrome/Connection.cpp
  ${REACT_NATIVE_SOURCE}/ReactCommon/hermes/inspector/chrome/ConnectionDemux.cpp
  ${REACT_NATIVE_SOURCE}/ReactCommon/hermes/inspector/chrome/MessageConverters.cpp
  ${REACT_NATIVE_SOURCE}/ReactCommon/hermes/inspector/chrome/MessageTypes.cpp
  ${REACT_NATIVE_SOURCE}/ReactCommon/hermes/inspector/chrome/Registration.cpp
  ${REACT_NATIVE_SOURCE}/ReactCommon/hermes/inspector/chrome/RemoteObjectsTable.cpp
  ${REACT_NATIVE_SOURCE}/ReactCommon/hermes/inspector/chrome/AutoAttachUtils.cpp
)

list(APPEND inspector_sources
  ${REACT_NATIVE_SOURCE}/ReactCommon/jsinspector/InspectorInterfaces.cpp
)

list(APPEND inspector_sources
   ../jsi/jsi/JSIDynamic.cpp
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

include_directories(${REACT_NATIVE_SOURCE}/ReactCommon)
include_directories(${FOLLY_SOURCE})
include_directories(${BOOST_SOURCE}/lib/native/include)
include_directories(stubs)

file(GLOB inspector_headers ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
file(GLOB inspector_public_headers ${PROJECT_SOURCE_DIR}/public/hermes/Public/*.h)

# set(HERMES_LINK_COMPONENTS LLVHSupport)
# set(HERMES_ENABLE_RTTI OFF)

# Restore EH and RTTI (Note: At the time of writing, there is no usage of
# add_hermes_library either after this line in this file or in a sub directory.
# However, the values are restored anyways for the sake of sanity.)
set(HERMES_ENABLE_EH ON)
set(HERMES_ENABLE_RTTI ON)

add_library(hermesinspector SHARED ${inspector_sources} ${inspector_headers} ${folly_sources} ${inspector_public_headers})
target_link_libraries(hermesinspector
#  hermesVMRuntime
  libhermes
)
hermes_link_icu(hermesinspector)

# find_package(folly CONFIG REQUIRED)
# target_link_libraries(hermesinspector PRIVATE Folly::folly Folly::folly_deps Folly::follybenchmark Folly::folly_test_util)
# target_link_libraries(hermesinspector PRIVATE Folly::folly)

# Export the required header directory
target_include_directories(hermesinspector PUBLIC ..)

# Same as above, but for windows. Note that there is no equivalent of -fvisibility=default.
set(compile_flags " /await ")
set(link_flags "/O2 ${OPTIONAL_GC_SECTIONS} ${OPTIONAL_JNI_ONLOAD}")

# Generate PDBs
set(compile_flags "${compile_flags} /Zi")
if("{CMAKE_BUILD_TYPE}" MATCHES "Debug")
  set(link_flags "${link_flags} /DEBUG")
else()
  set(link_flags "${link_flags} /DEBUG /OPT:REF /OPT:ICF")
endif()

set_target_properties(hermesinspector PROPERTIES
  COMPILE_FLAGS "${compile_flags}"
  LINK_FLAGS "${link_flags}"
  # Declare our dependency on std::make_unique
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED 17
  OUTPUT_NAME hermesinspector
)

set_property(TARGET hermesinspector APPEND_STRING PROPERTY
  COMPILE_DEFINITIONS "CREATE_SHARED_LIBRARY")