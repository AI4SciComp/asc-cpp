if(NOT DEFINED ASC_CPP_BINARY_DIR OR
   NOT DEFINED ASC_CPP_CONSUMER_SOURCE_DIR OR
   NOT DEFINED ASC_CPP_TEST_ROOT OR
   NOT DEFINED ASC_CPP_GENERATOR)
  message(FATAL_ERROR "package_test.cmake is missing required inputs")
endif()

file(REMOVE_RECURSE "${ASC_CPP_TEST_ROOT}")
set(_install_original "${ASC_CPP_TEST_ROOT}/original-prefix")
set(_install_relocated "${ASC_CPP_TEST_ROOT}/relocated-prefix")

execute_process(
  COMMAND "${CMAKE_COMMAND}" --install "${ASC_CPP_BINARY_DIR}"
          --prefix "${_install_original}"
  RESULT_VARIABLE _install_result
  OUTPUT_VARIABLE _install_output
  ERROR_VARIABLE _install_error
)
if(NOT _install_result EQUAL 0)
  message(FATAL_ERROR
    "ASCCpp install failed:\n${_install_output}\n${_install_error}"
  )
endif()

file(RENAME "${_install_original}" "${_install_relocated}")

foreach(_component IN ITEMS core utilities array linalg random cpp)
  set(_build "${ASC_CPP_TEST_ROOT}/build-${_component}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      -S "${ASC_CPP_CONSUMER_SOURCE_DIR}"
      -B "${_build}"
      -G "${ASC_CPP_GENERATOR}"
      "-DCMAKE_PREFIX_PATH=${_install_relocated}"
      "-DASC_TEST_COMPONENT=${_component}"
    RESULT_VARIABLE _configure_result
    OUTPUT_VARIABLE _configure_output
    ERROR_VARIABLE _configure_error
  )
  if(NOT _configure_result EQUAL 0)
    message(FATAL_ERROR
      "Consumer configure failed for ${_component}:\n${_configure_output}\n${_configure_error}"
    )
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${_build}"
    RESULT_VARIABLE _build_result
    OUTPUT_VARIABLE _build_output
    ERROR_VARIABLE _build_error
  )
  if(NOT _build_result EQUAL 0)
    message(FATAL_ERROR
      "Consumer build failed for ${_component}:\n${_build_output}\n${_build_error}"
    )
  endif()
  execute_process(
    COMMAND "${_build}/asc_cpp_consumer"
    RESULT_VARIABLE _run_result
  )
  if(NOT _run_result EQUAL 0)
    message(FATAL_ERROR "Consumer execution failed for ${_component}")
  endif()
endforeach()

set(_unknown_build "${ASC_CPP_TEST_ROOT}/build-unknown")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -S "${ASC_CPP_CONSUMER_SOURCE_DIR}"
    -B "${_unknown_build}"
    -G "${ASC_CPP_GENERATOR}"
    "-DCMAKE_PREFIX_PATH=${_install_relocated}"
    -DASC_TEST_COMPONENT=not_a_component
  RESULT_VARIABLE _unknown_result
  OUTPUT_VARIABLE _unknown_output
  ERROR_VARIABLE _unknown_error
)
if(_unknown_result EQUAL 0)
  message(FATAL_ERROR "Unknown ASCCpp component was incorrectly accepted")
endif()
string(CONCAT _unknown_diagnostic "${_unknown_output}" "${_unknown_error}")
if(NOT _unknown_diagnostic MATCHES "Unsupported ASCCpp component")
  message(FATAL_ERROR
    "Unknown component failed without the expected diagnostic:\n${_unknown_diagnostic}"
  )
endif()
