# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

# This file centralizes all Windows-specific build configuration logic.

# The version that we put inside of DLL files
if(NOT DEFINED HERMES_FILE_VERSION)
  set(HERMES_FILE_VERSION "${PROJECT_VERSION}.0")
endif()

# Make the HERMES_FILE_VERSION accessible for version printing in C++.
if(HERMES_FILE_VERSION)
  add_definitions(-DHERMES_FILE_VERSION="${HERMES_FILE_VERSION}")
endif()

# The file version convertible to number representation.
# We must replace dots with commas.
string(REPLACE "." "," HERMES_FILE_VERSION_BIN ${HERMES_FILE_VERSION})

# Make the HERMES_FILE_VERSION_BIN accessible for converting to a number.
if(HERMES_FILE_VERSION_BIN)
  add_definitions(-DHERMES_FILE_VERSION_BIN=${HERMES_FILE_VERSION_BIN})
endif()

# Handle HERMES_WINDOWS_FORCE_NATIVE_BUILD option to override cross-compilation detection
option(HERMES_WINDOWS_FORCE_NATIVE_BUILD "Force CMake to treat this as a native build (not cross-compilation)" ON)
if(HERMES_WINDOWS_FORCE_NATIVE_BUILD)
  set(CMAKE_CROSSCOMPILING FALSE)
endif()

# See if we target UWP
if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
  set(HERMES_WINDOWS_TARGET_UWP ON)
else()
  set(HERMES_WINDOWS_TARGET_UWP OFF)
endif()

# Set default for HERMES_MSVC_USE_PLATFORM_UNICODE_WINGLOB
if(HERMES_WINDOWS_TARGET_UWP)
  set(DEFAULT_HERMES_MSVC_USE_PLATFORM_UNICODE_WINGLOB OFF)
else()
  set(DEFAULT_HERMES_MSVC_USE_PLATFORM_UNICODE_WINGLOB ON)
endif()

option(HERMES_MSVC_USE_PLATFORM_UNICODE_WINGLOB "Use platform Unicode WINGLOB" ${DEFAULT_HERMES_MSVC_USE_PLATFORM_UNICODE_WINGLOB})
if (HERMES_MSVC_USE_PLATFORM_UNICODE_WINGLOB)
  add_definitions(-DUSE_PLATFORM_UNICODE_WINGLOB)
endif()

# Enable source linking
if ("${CMAKE_C_COMPILER_ID}" MATCHES "MSVC")
  # NOTE: Dependencies are not properly setup here.
  #       Currently, CMake does not know to re-link if SOURCE_LINK_JSON changes
  #       Currently, CMake does not re-generate SOURCE_LINK_JSON if git's HEAD changes
  if ("${CMAKE_C_COMPILER_VERSION}" VERSION_GREATER_EQUAL "19.20")
    include(SourceLink)
    file(TO_NATIVE_PATH "${PROJECT_BINARY_DIR}/source_link.json" SOURCE_LINK_JSON)
    source_link(${PROJECT_SOURCE_DIR} ${SOURCE_LINK_JSON})
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SOURCELINK:${SOURCE_LINK_JSON}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SOURCELINK:${SOURCE_LINK_JSON}")
  else()
    message(WARNING "Disabling SourceLink due to old version of MSVC. Please update to VS2019!")
  endif()
endif()

# Security flags.
# Note: Security warnings need to be fixed / baselined to be sdl clean - 4146, 4244 and 4267 (currently disabled)
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DYNAMICBASE /guard:cf")
# set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /DYNAMICBASE /guard:cf")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /guard:cf /Qspectre /sdl /ZH:SHA_256")
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /guard:cf /Qspectre /sdl /ZH:SHA_256")

# Configure Clang compiler flags
function(hermes_windows_configure_clang_flags)
    # Basic Windows definitions
    set(CLANG_CXX_FLAGS "-DWIN32 -D_WINDOWS -D_CRT_RAND_S")
    
    # Debug information
    set(CLANG_CXX_FLAGS "${CLANG_CXX_FLAGS} -g -gcodeview")
    
    # Optimization flags for function/data sections (enables /OPT:REF)
    set(CLANG_CXX_FLAGS "${CLANG_CXX_FLAGS} -ffunction-sections -fdata-sections")
    
    # Security flags
    set(CLANG_CXX_FLAGS "${CLANG_CXX_FLAGS} -fstack-protector-all")
   
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CLANG_CXX_FLAGS}" PARENT_SCOPE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CLANG_CXX_FLAGS}" PARENT_SCOPE)
endfunction()

# Configure MSVC compiler flags
function(hermes_windows_configure_msvc_flags)
    # Debug information
    set(MSVC_CXX_FLAGS "/Zi")
    
    # Security flags
    set(MSVC_CXX_FLAGS "${MSVC_CXX_FLAGS} /GS /guard:cf /Qspectre /sdl /ZH:SHA_256")
    
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MSVC_CXX_FLAGS}" PARENT_SCOPE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MSVC_CXX_FLAGS}" PARENT_SCOPE)
endfunction()

# Configure lld-link (Clang) linker flags
function(hermes_windows_configure_lld_flags)
  # Debug information
  list(APPEND HERMES_EXTRA_LINKER_FLAGS "LINKER:/DEBUG:FULL")
  list(APPEND HERMES_EXTRA_LINKER_FLAGS "LINKER:/DEBUGTYPE:CV,FIXUP")

  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    # Security flags
    list(APPEND HERMES_EXTRA_LINKER_FLAGS "LINKER:/guard:cf")
    if(NOT HERMES_MSVC_ARM64)
      list(APPEND HERMES_EXTRA_LINKER_FLAGS "LINKER:/CETCOMPAT")
    endif()

    # Optimization flags
    list(APPEND HERMES_EXTRA_LINKER_FLAGS "LINKER:/OPT:REF")
    # Note: ICF is disabled due to Hermes code using pointers to functions that are optimized away
    list(APPEND HERMES_EXTRA_LINKER_FLAGS "LINKER:/OPT:NOICF")

    # Deterministic builds
    list(APPEND HERMES_EXTRA_LINKER_FLAGS "LINKER:/BREPRO")
  endif()

  # UWP-specific flags
  if(HERMES_WINDOWS_TARGET_UWP)
    list(APPEND HERMES_EXTRA_LINKER_FLAGS "LINKER:/APPCONTAINER;LINKER:-lwindowsapp")
  endif()

  set(HERMES_EXTRA_LINKER_FLAGS "${HERMES_EXTRA_LINKER_FLAGS}" PARENT_SCOPE)
endfunction()

# Configure MSVC linker flags
function(hermes_windows_configure_msvc_linker_flags)
  # Debug information
  list(APPEND MSVC_DEBUG_LINKER_FLAGS "LINKER:/DEBUG:FULL")
  list(APPEND MSVC_DEBUG_LINKER_FLAGS "LINKER:/DEBUGTYPE:CV$<COMMA>FIXUP")

  # UWP-specific flags
  if(HERMES_WINDOWS_TARGET_UWP)
    list(APPEND MSVC_DEBUG_LINKER_FLAGS "LINKER:/APPCONTAINER")
  endif()

  list(APPEND MSVC_RELEASE_LINKER_FLAGS "${MSVC_DEBUG_LINKER_FLAGS}")

  # Security flags
  list(APPEND MSVC_RELEASE_LINKER_FLAGS "/ZH:SHA_256")
  list(APPEND MSVC_RELEASE_LINKER_FLAGS "LINKER:/guard:cf")
  if(NOT HERMES_MSVC_ARM64)
    list(APPEND MSVC_RELEASE_LINKER_FLAGS "LINKER:/CETCOMPAT")
  endif()

  # Optimization flags
  list(APPEND MSVC_RELEASE_LINKER_FLAGS "LINKER:/OPT:REF")
  # Note: ICF is disabled due to Hermes code using pointers to functions that are optimized away
  list(APPEND MSVC_RELEASE_LINKER_FLAGS "LINKER:/OPT:NOICF")

  # Deterministic builds
  list(APPEND MSVC_RELEASE_LINKER_FLAGS "LINKER:/BREPRO")

  set(HERMES_EXTRA_LINKER_FLAGS "$<IF:$<CONFIG:Release>,${MSVC_RELEASE_LINKER_FLAGS},${MSVC_DEBUG_LINKER_FLAGS}>" PARENT_SCOPE)
endfunction()

# Configure cross-compilation settings
function(hermes_windows_configure_cross_compilation target_platform)
  if(NOT target_platform)
    message(FATAL_ERROR "Target platform must be specified for cross-compilation")
  endif()
  
  if(Clang)
    # Map platform names to target triples
    set(target_triple "")
    if(target_platform STREQUAL "x86")
      set(target_triple "i686-pc-windows-msvc")
    elseif(target_platform STREQUAL "arm64")
      set(target_triple "aarch64-pc-windows-msvc")
    elseif(target_platform STREQUAL "arm64ec")
      set(target_triple "arm64ec-pc-windows-msvc")
    else()
      message(FATAL_ERROR "Unsupported target platform: ${target_platform}")
    endif()
    
    message(STATUS "Cross-compiling for ${target_platform} (${target_triple})")
    
    # Set the target triple for cross-compilation
    set(CMAKE_C_COMPILER_TARGET ${target_triple} PARENT_SCOPE)
    set(CMAKE_CXX_COMPILER_TARGET ${target_triple} PARENT_SCOPE)
  endif()

  # Configure additional cross-compilation settings
  if(target_platform STREQUAL "arm64" OR target_platform STREQUAL "arm64ec")
      set(HERMES_MSVC_ARM64 ON PARENT_SCOPE)
  endif()
  
  # ARM64EC-specific compiler flags
  if(target_platform STREQUAL "arm64ec")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -arm64EC" PARENT_SCOPE)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -arm64EC" PARENT_SCOPE)
  endif()
endfunction()

# Main configuration function
function(hermes_windows_configure_build)
  message(STATUS "Configuring Hermes Windows build...")
  message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")
  message(STATUS "  Target platform: ${HERMES_WINDOWS_TARGET_PLATFORM}")
  
  # Configure cross-compilation if needed
  if(NOT HERMES_WINDOWS_TARGET_PLATFORM)
    set(HERMES_WINDOWS_TARGET_PLATFORM "x64")
  endif()
  if(NOT HERMES_WINDOWS_TARGET_PLATFORM STREQUAL "x64")
    hermes_windows_configure_cross_compilation(${HERMES_WINDOWS_TARGET_PLATFORM})
  endif()
  
  # Configure compiler flags
  if(Clang)
    hermes_windows_configure_clang_flags()
  else()
    hermes_windows_configure_msvc_flags()
  endif()
  
  # Configure linker flags
  if(Clang)
    hermes_windows_configure_lld_flags()
  else()
    hermes_windows_configure_msvc_linker_flags()
  endif()
  
  hermes_windows_show_configuration()

  message(STATUS "Hermes Windows build configuration complete")
endfunction()

# Utility function to display current configuration
function(hermes_windows_show_configuration)
  message(STATUS "=== Hermes Windows Configuration ===")
  message(STATUS "C compiler target: ${CMAKE_C_COMPILER_TARGET}")
  message(STATUS "CXX compiler target: ${CMAKE_CXX_COMPILER_TARGET}")
  message(STATUS "C Flags: ${CMAKE_C_FLAGS}")
  message(STATUS "CXX Flags: ${CMAKE_CXX_FLAGS}")
  message(STATUS "Extra Linker Flags: ${HERMES_EXTRA_LINKER_FLAGS}")
  message(STATUS "=====================================")
endfunction()
