# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

include(CMakePrintHelpers)

# Set the react-native source directory to the external folder
set(REACT_NATIVE_SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/react-native)

# Check if react-native exists in external directory
if(NOT EXISTS ${REACT_NATIVE_SOURCE_DIR}/packages/react-native/ReactCommon/hermes)
  message(FATAL_ERROR
    "React Native source not found at ${REACT_NATIVE_SOURCE_DIR}/packages/react-native/ReactCommon/hermes. "
    "Please ensure React Native is extracted to external/react-native/")
endif()

# The override file should already be in place in the external directory
# Verify it exists
set(OVERRIDE_FILE ${REACT_NATIVE_SOURCE_DIR}/packages/react-native/ReactCommon/hermes/inspector-modern/chrome/HermesRuntimeAgentDelegate.cpp)
if(NOT EXISTS ${OVERRIDE_FILE})
  message(STATUS "Copying override file to external React Native")
  file(
    COPY ${CMAKE_CURRENT_SOURCE_DIR}/overrides/HermesRuntimeAgentDelegate.cpp
    DESTINATION ${REACT_NATIVE_SOURCE_DIR}/packages/react-native/ReactCommon/hermes/inspector-modern/chrome)
endif()

set(REACT_NATIVE_SOURCE ${REACT_NATIVE_SOURCE_DIR}/packages/react-native PARENT_SCOPE)
