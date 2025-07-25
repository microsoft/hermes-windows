# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

set(NO_EH_RTTI_SOURCES
  CDPJSONHelpers.cpp
  ../../API/hermes/cdp/JSONValueInterfaces.cpp
  ../../API/hermes/cdp/MessageInterfaces.cpp
  ../../API/hermes/cdp/MessageTypes.cpp
  )
set(APITestsSources
  APITest.cpp
  APITestFactory.cpp
  ${HERMES_JSI_DIR}/jsi/test/testlib.cpp
  AsyncDebuggerAPITest.cpp
  CDPAgentTest.cpp
  ConsoleMessageTest.cpp
  DomainStateTest.cpp
  SynchronizedCallbackTest.cpp
  DebuggerTest.cpp
  SegmentTest.cpp
  HeapSnapshotAPITest.cpp
  SynthTraceTest.cpp
  SynthTraceParserTest.cpp
  SynthTraceSerializationTest.cpp
  TimerStatsTest.cpp
  ${NO_EH_RTTI_SOURCES}
  )
set(APISegmentTestCompileSources
  SegmentTestCompile.cpp
  )

if (NOT HERMES_ENABLE_EH_RTTI)
  if (GCC_COMPATIBLE)
    set_property(SOURCE ${NO_EH_RTTI_SOURCES} APPEND_STRING
      PROPERTY COMPILE_FLAGS "-fno-exceptions -fno-rtti")
  elseif (MSVC)
    set_property(SOURCE ${NO_EH_RTTI_SOURCES} APPEND_STRING
      PROPERTY COMPILE_FLAGS "/EHs-c- /GR-")
  endif ()
endif ()

set(HERMES_LINK_COMPONENTS LLVHSupport)

# Build SegmentTestCompile without EH and RTTI
add_hermes_library(SegmentTestCompile STATIC ${APISegmentTestCompileSources} LINK_LIBS hermesHBCBackend)

# Turn on EH and RTTI for APITests
set(HERMES_ENABLE_EH_RTTI ON)
# For some reason (bug?) add_unittest() is clearing LLVM_REQUIRES_RTTI, so
# we need to set this one.
set(LLVM_ENABLE_RTTI ON)

add_hermes_unittest(APITests ${APITestsSources})

target_link_libraries(APITests libhermes compileJS SegmentTestCompile traceInterpreter
                               timerStats hermesABIRuntimeWrapper hermesabi
                               hermesSandboxRuntime hermesSupport cdpInternal hermesNodeApiJsiRuntime)

add_hermes_unittest(APILeanTests APILeanTest.cpp)
target_link_libraries(APILeanTests libhermes_lean jsi)

# copy hermes.dll next to the APITests.exe
add_dependencies(APITests libshared)
add_custom_command(TARGET APITests POST_BUILD
  VERBATIM
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  $<TARGET_FILE:libshared>
  $<TARGET_FILE_DIR:APITests>)

