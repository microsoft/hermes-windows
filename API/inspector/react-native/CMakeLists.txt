include(FetchContent)

FetchContent_Declare(
  reactnative
  URL https://github.com/facebook/react-native/archive/a0800ffc7a676555aa9e769fc8fd6d3162de0ea6.zip
  URL_HASH SHA256=80a3b89d92a48bd1d39533a6643e4f1390f2ce11db1377a50bd10d557e423857
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
endif()

set(REACT_NATIVE_SOURCE ${reactnative_SOURCE_DIR} PARENT_SCOPE)
