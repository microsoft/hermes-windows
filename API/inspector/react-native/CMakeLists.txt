include(FetchContent)

FetchContent_Declare(
  reactnative
  GIT_REPOSITORY https://github.com/facebook/react-native.git
  GIT_TAG        v0.66.4
  GIT_SHALLOW    1
  GIT_PROGRESS   1
)

FetchContent_GetProperties(reactnative)

if(NOT reactnative_POPULATED)
  FetchContent_Populate(reactnative)

  file(READ ${reactnative_SOURCE_DIR}/ReactCommon/jsinspector/InspectorInterfaces.h file_text)
  string(REPLACE
    "extern IInspector &getInspectorInstance"
    "extern __declspec(dllexport) IInspector& __cdecl getInspectorInstance"
    file_text
    "${file_text}"
  )
  file(WRITE ${reactnative_SOURCE_DIR}/ReactCommon/jsinspector/InspectorInterfaces.h "${file_text}")

  file(READ ${reactnative_SOURCE_DIR}/ReactCommon/hermes/inspector/chrome/Registration.h file_text)
  string(REPLACE
    "extern void enableDebugging"
    "extern __declspec(dllexport) void __cdecl enableDebugging"
    file_text
    "${file_text}"
  )
  string(REPLACE
    "extern void disableDebugging"
    "extern __declspec(dllexport) void __cdecl disableDebugging"
    file_text
    "${file_text}"
  )
  file(WRITE ${reactnative_SOURCE_DIR}/ReactCommon/hermes/inspector/chrome/Registration.h "${file_text}")

  file(GLOB_RECURSE YARN_FILES ${reactnative_SOURCE_DIR}/yarn.lock ${reactnative_SOURCE_DIR}/**/yarn.lock)
  message("Removing unused yarn.lock files: ${YARN_FILES}")
  if(YARN_FILES)
    file(REMOVE ${YARN_FILES})
  endif()
endif()

set(REACT_NATIVE_SOURCE ${reactnative_SOURCE_DIR} PARENT_SCOPE)
