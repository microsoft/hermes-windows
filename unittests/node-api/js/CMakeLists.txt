set(testJSFiles
  2_function_arguments/test.js
  3_callbacks/test.js
  4_object_factory/test.js
  5_function_factory/test.js
  6_object_wrap/test.js
  7_factory_wrap/test.js
  8_passing_wrapped/test.js
  common/assert.js
  common/common.js
  test_array/test.js
  test_bigint/test.js
  test_constructor/test.js
  test_constructor/test2.js
  test_conversions/test.js
  test_dataview/test.js
  test_date/test.js
  test_error/test.js
  test_exception/test.js
  test_exception/testFinalizerException.js
  test_finalizer/test.js
  test_function/test.js
  test_general/test.js
  test_general/testEnvCleanup.js
  test_general/testFinalizer.js
  test_general/testGlobals.js
  test_general/testInstanceOf.js
  test_general/testNapiRun.js
  test_general/testNapiStatus.js
  test_handle_scope/test.js
  test_instance_data/test.js
  test_new_target/test.js
  test_number/test.js
  test_object/test.js
  test_object/test_null.js
  test_promise/test.js
  test_properties/test.js
  test_reference/test.js
  test_reference_double_free/test.js
  test_string/test.js
  test_symbol/test1.js
  test_symbol/test2.js
  test_symbol/test3.js
  test_typedarray/test.js
)

if(WIN32)
  # "npx" interrupts current shell script execution without the "call"
  set(npx "call" "npx")
  set(yarn "call" "yarn")
else()
  set(npx "npx")
  set(yarn "yarn")
endif()

# copy JS Tools package files
add_custom_command(
  OUTPUT
    ${CMAKE_CURRENT_BINARY_DIR}/babel.config.js
    ${CMAKE_CURRENT_BINARY_DIR}/package.json
    ${CMAKE_CURRENT_BINARY_DIR}/yarn.lock
  COMMAND ${CMAKE_COMMAND} -E copy_if_different 
    ${CMAKE_CURRENT_SOURCE_DIR}/babel.config.js
    ${CMAKE_CURRENT_SOURCE_DIR}/package.json
    ${CMAKE_CURRENT_SOURCE_DIR}/yarn.lock
    ${CMAKE_CURRENT_BINARY_DIR}
)
add_custom_target(copyNodeApiJSToolsFiles)

# run "yarn install"
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/node_modules.sha1
  DEPENDS
    ${CMAKE_CURRENT_BINARY_DIR}/package.json
    ${CMAKE_CURRENT_BINARY_DIR}/yarn.lock
  COMMAND ${yarn} install --frozen-lockfile
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_custom_target(installNodeApiTestJsTools
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/node_modules.sha1
)
add_dependencies(installNodeApiTestJsTools copyNodeApiJSToolsFiles)

# TODO: replace "metro-react-native-babel-preset" with a modern alternative.
# add the Babel transform commands for each test JS file
foreach(testJSFile ${testJSFiles})
  # create target directory
  get_filename_component(testJSDir ${testJSFile} DIRECTORY)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${testJSDir})

  # generate Hermes-compatible JavaScript code
  add_custom_command(
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/${testJSFile}
            ${CMAKE_CURRENT_BINARY_DIR}/${testJSFile}.map
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../js-native-api/${testJSFile}
    COMMAND ${npx}
      "babel"
      "--retain-lines"
      "--source-maps"
        "true"
      "--out-file"
        "${CMAKE_CURRENT_BINARY_DIR}/${testJSFile}"
      "--source-map-target"
        "${CMAKE_CURRENT_BINARY_DIR}/${testJSFile}.map"
      "${CMAKE_CURRENT_SOURCE_DIR}/../js-native-api/${testJSFile}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )

  # build a list of all outputs
  list(APPEND transformedJSFiles
    ${CMAKE_CURRENT_BINARY_DIR}/${testJSFile}
    ${CMAKE_CURRENT_BINARY_DIR}/${testJSFile}.map
  )
endforeach()

# run the Babel transforms for all required output files
add_custom_target(transformJSFiles
  DEPENDS ${transformedJSFiles}
)
add_dependencies(transformJSFiles installNodeApiTestJsTools)

# copy Babel runtime support files
add_custom_target(copyBabelFiles
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_BINARY_DIR}/node_modules/@babel/runtime/helpers
    ${CMAKE_CURRENT_BINARY_DIR}/@babel/runtime/helpers
)
add_dependencies(copyBabelFiles installNodeApiTestJsTools)