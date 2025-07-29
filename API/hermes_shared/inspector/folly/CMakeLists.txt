# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

include(CMakePrintHelpers)

# Set the folly source directory to the external folder
set(FOLLY_SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/folly/src)

# Check if folly exists in external directory
if(NOT EXISTS ${FOLLY_SOURCE_DIR}/folly)
  message(FATAL_ERROR
    "Folly source not found at ${FOLLY_SOURCE_DIR}/folly. "
    "Please ensure folly v2020.01.13.00 is extracted to external/folly/")
endif()

# Apply the MSVC compatibility patch
set(BUILTINS_FILE ${FOLLY_SOURCE_DIR}/folly/portability/Builtins.h)
if(EXISTS ${BUILTINS_FILE})
  file(READ ${BUILTINS_FILE} file_text)

  # Check if patch is already applied
  string(FIND "${file_text}" "_MSC_VER >= 1928" patch_applied)
  if(patch_applied EQUAL -1)
    message(STATUS "Applying MSVC compatibility patch to folly/portability/Builtins.h")
    string(REPLACE
        "#if !defined(_MSC_VER) || (_MSC_VER < 1923)"
        "#if !defined(_MSC_VER) || _MSC_VER < 1923 || _MSC_VER >= 1928"
        file_text
        "${file_text}"
    )
    file(WRITE ${BUILTINS_FILE} "${file_text}")
  else()
    message(STATUS "MSVC compatibility patch already applied to folly")
  endif()
endif()

# Set the folly source directory for parent scope
set(FOLLY_SOURCE ${FOLLY_SOURCE_DIR} PARENT_SCOPE)
