include(FetchContent)

FetchContent_Declare(
  reactnative
  URL https://github.com/facebook/react-native/archive/a0ed073650cbdc3e1e08c11d18acf73e0952393a.zip
  URL_HASH SHA256=8111f994cbef91300ed57f9bbdd8a0d20fe27bab1ad9a6bfcd637c67a4a29af5
  DOWNLOAD_EXTRACT_TIMESTAMP true
)

FetchContent_GetProperties(reactnative)

if(NOT reactnative_POPULATED)
  FetchContent_Populate(reactnative)

  file(GLOB_RECURSE YARN_FILES ${reactnative_SOURCE_DIR}/yarn.lock ${reactnative_SOURCE_DIR}/**/yarn.lock)
  message("Removing unused yarn.lock files: ${YARN_FILES}")
  if(YARN_FILES)
    file(REMOVE ${YARN_FILES})
  endif()

  file(GLOB_RECURSE GEMFILES ${reactnative_SOURCE_DIR}/Gemfile.lock ${reactnative_SOURCE_DIR}/**/Gemfile.lock)
  message("Removing unused Gemfile.lock files: ${GEMFILES}")
  if(GEMFILES)
    file(REMOVE ${GEMFILES})
  endif()

  file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/overrides/HermesRuntimeAgentDelegate.cpp
    DESTINATION ${reactnative_SOURCE_DIR}/packages/react-native/ReactCommon/hermes/inspector-modern/chrome)
endif()

set(REACT_NATIVE_SOURCE ${reactnative_SOURCE_DIR}/packages/react-native PARENT_SCOPE)
