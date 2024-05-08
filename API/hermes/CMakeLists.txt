# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

# compileJS uses neither exceptions nor RTTI
add_hermes_library(compileJS CompileJS.cpp LINK_LIBS hermesPublic hermesHBCBackend)

if(HERMES_ENABLE_DEBUGGER)
  if(HERMES_ENABLE_TOOLS AND NOT WIN32)
    add_subdirectory(inspector/chrome/cli)
  endif()

  # Some inspector sources need to be built without RTTI because they consume
  # internal data structures compiled without RTTI.
  set(INSPECTOR_NO_EH_RTTI_SOURCES
    inspector/chrome/JSONValueInterfaces.cpp
    inspector/chrome/MessageConverters.cpp
    inspector/chrome/MessageInterfaces.cpp
    inspector/chrome/MessageTypes.cpp
  )

  if (NOT HERMES_ENABLE_EH_RTTI)
    if (GCC_COMPATIBLE)
      set_property(SOURCE ${INSPECTOR_NO_EH_RTTI_SOURCES} APPEND_STRING
        PROPERTY COMPILE_FLAGS "-fno-exceptions -fno-rtti")
    elseif (MSVC)
      set_property(SOURCE ${INSPECTOR_NO_EH_RTTI_SOURCES} APPEND_STRING
        PROPERTY COMPILE_FLAGS "/EHs-c- /GR-")
    endif ()
  endif ()

  set(CDP_API_SOURCES
    cdp/CDPAgent.cpp
    cdp/ConsoleMessage.cpp
    cdp/DebuggerDomainAgent.cpp
    cdp/ProfilerDomainAgent.cpp
    cdp/RemoteObjectsTable.cpp
    cdp/RuntimeDomainAgent.cpp
  )

  set(INSPECTOR_API_SOURCES
    inspector/chrome/CallbackOStream.cpp
    inspector/chrome/CDPHandler.cpp
    inspector/chrome/RemoteObjectConverters.cpp
    inspector/RuntimeAdapter.cpp
    ${CDP_API_SOURCES}
    ${INSPECTOR_NO_EH_RTTI_SOURCES}
  )

  # Even in lean builds, the inspector needs the parser to parse JSON.
  set(INSPECTOR_DEPS hermesParser)
endif()

add_hermes_library(synthTraceParser SynthTraceParser.cpp LINK_LIBS hermesSupport hermesParser synthTrace)

# All remaining targets in this file use both exceptions and RTTI.
set(HERMES_ENABLE_EH_RTTI ON)

add_subdirectory(cdp)

# List the files that define exported functions explicitly, and they can link
# against the internal functionality they need.
set(api_sources
  hermes.cpp
  DebuggerAPI.cpp
  AsyncDebuggerAPI.cpp
  RuntimeTaskRunner.cpp
  ${INSPECTOR_API_SOURCES}
  )

set(api_sources2
  ${api_sources}
  hermes_napi.cpp
  MurmurHash.cpp
  )

if(WIN32)
  list(APPEND api_sources2 hermes_win.cpp)
endif()

file(GLOB api_headers ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
file(GLOB api_public_headers ${PROJECT_SOURCE_DIR}/public/hermes/Public/*.h)

if(HERMES_THREAD_SAFETY_ANALYSIS)
  set(TSA_SOURCES
    AsyncDebuggerAPI.cpp
    ${CDP_API_SOURCES}
  )
  if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set_property(SOURCE ${TSA_SOURCES} APPEND_STRING PROPERTY
      COMPILE_FLAGS "-Wthread-safety -Werror=thread-safety-analysis -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS")
  endif()
endif()

add_hermes_library(synthTrace hermes_tracing.cpp SynthTrace.cpp TracingRuntime.cpp
  LINK_LIBS libhermes hermesPlatform)

add_hermes_library(timerStats TimerStats.cpp LINK_LIBS jsi hermesSupport)

add_hermes_library(traceInterpreter TraceInterpreter.cpp
  LINK_LIBS libhermes hermesInstrumentation synthTrace synthTraceParser)

add_library(libhermes2 ${api_sources})
target_link_libraries(libhermes2 PUBLIC jsi PRIVATE hermesVMRuntime ${INSPECTOR_DEPS})
target_link_options(libhermes2 PRIVATE ${HERMES_EXTRA_LINKER_FLAGS})
target_include_directories(libhermes2 PUBLIC .. ../../public ${HERMES_JSI_DIR})

add_library(libhermes SHARED ${api_sources2})
target_link_libraries(libhermes PUBLIC jsi PRIVATE hermesVMRuntime hermesNapi hermesinspector ${INSPECTOR_DEPS})
target_link_options(libhermes PRIVATE ${HERMES_EXTRA_LINKER_FLAGS})

if (WIN32)
  configure_file(version.rc.in version.rc @ONLY)
  target_sources(libhermes PRIVATE  ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()

# Export the required header directory
target_include_directories(libhermes PUBLIC
  ..
  ../../public
  ../node-api
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

set_target_properties(libhermes PROPERTIES
  COMPILE_FLAGS "${compile_flags}"
  LINK_OPTIONS "$<IF:$<CONFIG:Debug>,${link_flags_debug},${link_flags_release}>"
  # Avoid becoming liblibhermes (and there's already a target called 'hermes')
  OUTPUT_NAME hermes
)

# Create a lean version of libhermes in the same way.
add_library(libhermes_lean ${api_sources})
target_link_libraries(libhermes_lean PUBLIC jsi PRIVATE hermesVMRuntimeLean ${INSPECTOR_DEPS})
target_link_options(libhermes_lean PRIVATE ${HERMES_EXTRA_LINKER_FLAGS})
target_include_directories(libhermes_lean PUBLIC
  ..
  ../../public
  ../node-api
  ${HERMES_JSI_DIR}
  ${REACT_NATIVE_SOURCE}/ReactCommon
  ${REACT_NATIVE_SOURCE}/ReactCommon/runtimeexecutor
  ${REACT_NATIVE_SOURCE_72}/ReactCommon)
set_target_properties(libhermes_lean PROPERTIES OUTPUT_NAME hermes_lean)

if(APPLE AND HERMES_BUILD_APPLE_FRAMEWORK)
  set_target_properties(libhermes PROPERTIES
    FRAMEWORK TRUE
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION}
    FRAMEWORK_VERSION ${PROJECT_VERSION_MAJOR}
    MACOSX_FRAMEWORK_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_FRAMEWORK_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_FRAMEWORK_IDENTIFIER dev.hermesengine.${HERMES_APPLE_TARGET_PLATFORM}
  )
  # Install headers into `Headers` while keeping required directory structure
  set_source_files_properties(${api_headers} PROPERTIES
    MACOSX_PACKAGE_LOCATION Headers
  )
  set_source_files_properties(${api_public_headers} PROPERTIES
    MACOSX_PACKAGE_LOCATION Headers/Public
  )
  if(HERMES_ENABLE_BITCODE)
    target_compile_options(libhermes PUBLIC "-fembed-bitcode")
    target_link_options(libhermes PUBLIC "-fembed-bitcode")
  endif()
  # Define the deployment target in the frameworks metadata
  if(HERMES_APPLE_TARGET_PLATFORM MATCHES "iphone")
    add_custom_command(TARGET libhermes POST_BUILD
      COMMAND /usr/libexec/PlistBuddy -c "Add :MinimumOSVersion string ${CMAKE_OSX_DEPLOYMENT_TARGET}" $<TARGET_FILE_DIR:libhermes>/Info.plist
    )
  elseif(HERMES_APPLE_TARGET_PLATFORM MATCHES "catalyst")
    add_custom_command(TARGET libhermes POST_BUILD
      COMMAND /usr/libexec/PlistBuddy -c "Add :LSMinimumSystemVersion string ${CMAKE_OSX_DEPLOYMENT_TARGET}" $<TARGET_FILE_DIR:libhermes>/Resources/Info.plist
    )
  elseif(HERMES_APPLE_TARGET_PLATFORM MATCHES "macos")
    add_custom_command(TARGET libhermes POST_BUILD
      COMMAND /usr/libexec/PlistBuddy -c "Add :LSMinimumSystemVersion string ${CMAKE_OSX_DEPLOYMENT_TARGET}" $<TARGET_FILE_DIR:libhermes>/Resources/Info.plist
    )
  endif()
endif()

install(TARGETS libhermes
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  FRAMEWORK DESTINATION Library/Frameworks/${HERMES_APPLE_TARGET_PLATFORM}
)
# Install headers into `include` while keeping required directory structure
install(DIRECTORY "${PROJECT_SOURCE_DIR}/API/hermes" DESTINATION include
  FILES_MATCHING PATTERN "*.h"
  PATTERN "synthtest" EXCLUDE)

# Create debug symbols (dSYM) bundle for Apple platform dylibs/frameworks
# Largely inspired by https://github.com/llvm/llvm-project/blob/6701993027f8af172d7ba697884459261b00e3c6/llvm/cmake/modules/AddLLVM.cmake#L1934-L1986
if(HERMES_BUILD_APPLE_DSYM)
  if(CMAKE_CXX_FLAGS MATCHES "-flto")
    set(lto_object ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/libhermes-lto.o)
    set_property(TARGET libhermes APPEND_STRING PROPERTY LINK_FLAGS " -Wl,-object_path_lto,${lto_object}")
  endif()

  get_target_property(DSYM_PATH libhermes LOCATION)
  if(HERMES_BUILD_APPLE_FRAMEWORK)
    get_filename_component(DSYM_PATH ${DSYM_PATH} DIRECTORY)
  endif()
  set(DSYM_PATH "${DSYM_PATH}.dSYM")

  if(NOT CMAKE_DSYMUTIL)
    set(CMAKE_DSYMUTIL xcrun dsymutil)
  endif()
  add_custom_command(TARGET libhermes POST_BUILD
    COMMAND ${CMAKE_DSYMUTIL} $<TARGET_FILE:libhermes> --out ${DSYM_PATH}
    BYPRODUCTS ${DSYM_PATH}
  )

  if(HERMES_BUILD_APPLE_FRAMEWORK)
    install(DIRECTORY ${DSYM_PATH} DESTINATION Library/Frameworks/${HERMES_APPLE_TARGET_PLATFORM})
  else()
    install(DIRECTORY ${DSYM_PATH} DESTINATION lib)
  endif()
endif()
