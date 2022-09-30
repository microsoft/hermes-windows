include(FetchContent)

FetchContent_Declare(
  reactnative
  GIT_REPOSITORY https://github.com/facebook/react-native.git
  GIT_TAG        v0.70.0-rc.3
  GIT_SHALLOW    1
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

  # Fix for https://github.com/microsoft/react-native-windows/issues/9662 (pushed in FB RN per
  # https://github.com/facebook/react-native/pull/34342). This patch is no longer needed and should be 
  # removed once the GIT_TAG above contains commit ID 60e7eb4d534298cb9888d5ab0c8b6a6b041dc299.
  file(READ ${reactnative_SOURCE_DIR}/ReactCommon/hermes/inspector/chrome/ConnectionDemux.cpp file_text)
  string(REGEX REPLACE 
    "\n([ \t]*)auto conn = conns_\\.at\\(pageId\\);\n[ \t]*conn->disconnect\\(\\);\n"
    "\n\\1auto conn = conns_.at(pageId);\n\\1std::string title = conn->getTitle();\n\\1inspectedContexts_->erase(title);\n\\1conn->disconnect();\n"
    file_text
    "${file_text}"
  )
  file(WRITE ${reactnative_SOURCE_DIR}/ReactCommon/hermes/inspector/chrome/ConnectionDemux.cpp  "${file_text}")

  # Fix for https://github.com/facebook/react-native/issues/34639 (pushed to FB RN per
  # https://github.com/facebook/react-native/pull/34640).
  file(READ ${reactnative_SOURCE_DIR}/ReactCommon/hermes/inspector/chrome/Connection.cpp file_text)
  string(REGEX REPLACE 
    "\n([ \t]*)// TODO\\(jpporto\\): fix test cases sending invalid context id.\n[ \t]*// note.executionContextId = kHermesExecutionContextId;"
    "\n\\1note.executionContextId = kHermesExecutionContextId;"
    file_text
    "${file_text}"
  )
  file(WRITE ${reactnative_SOURCE_DIR}/ReactCommon/hermes/inspector/chrome/Connection.cpp "${file_text}")

  file(GLOB_RECURSE YARN_FILES ${reactnative_SOURCE_DIR}/yarn.lock ${reactnative_SOURCE_DIR}/**/yarn.lock)
  message("Removing unused yarn.lock files: ${YARN_FILES}")
  if(YARN_FILES)
    file(REMOVE ${YARN_FILES})
  endif()
endif()

set(REACT_NATIVE_SOURCE ${reactnative_SOURCE_DIR} PARENT_SCOPE)
