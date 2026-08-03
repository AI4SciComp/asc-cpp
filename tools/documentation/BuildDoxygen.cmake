cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS
    ASC_CPP_DOCUMENTATION_SOURCE_DIR
    ASC_CPP_DOCUMENTATION_BUILD_DIR
    ASC_CPP_DOCUMENTATION_VERSION
    ASC_CPP_DOCUMENTATION_DOXYGEN_EXECUTABLE
)
  if(NOT DEFINED "${_required}" OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()
unset(_required)

cmake_path(
  ABSOLUTE_PATH ASC_CPP_DOCUMENTATION_SOURCE_DIR
  NORMALIZE
  OUTPUT_VARIABLE PROJECT_SOURCE_DIR
)
cmake_path(
  ABSOLUTE_PATH ASC_CPP_DOCUMENTATION_BUILD_DIR
  NORMALIZE
  OUTPUT_VARIABLE PROJECT_BINARY_DIR
)
set(PROJECT_VERSION "${ASC_CPP_DOCUMENTATION_VERSION}")
set(_asc_cpp_doxygen_output "${PROJECT_BINARY_DIR}/docs/doxygen")
set(_asc_cpp_doxygen_warning_log
  "${_asc_cpp_doxygen_output}/doxygen-warnings.log"
)
set(_asc_cpp_doxyfile "${PROJECT_BINARY_DIR}/docs/Doxyfile")

file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/docs")
configure_file(
  "${PROJECT_SOURCE_DIR}/docs/Doxyfile.in"
  "${_asc_cpp_doxyfile}"
  @ONLY
)
file(REMOVE_RECURSE "${_asc_cpp_doxygen_output}")
file(MAKE_DIRECTORY "${_asc_cpp_doxygen_output}")
execute_process(
  COMMAND
    "${ASC_CPP_DOCUMENTATION_DOXYGEN_EXECUTABLE}"
    "${_asc_cpp_doxyfile}"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  RESULT_VARIABLE _asc_cpp_doxygen_result
)
if(NOT _asc_cpp_doxygen_result EQUAL 0)
  message(FATAL_ERROR "Doxygen failed with exit code ${_asc_cpp_doxygen_result}")
endif()
file(REMOVE "${_asc_cpp_doxygen_output}/xml/Doxyfile.xml")

unset(_asc_cpp_doxyfile)
unset(_asc_cpp_doxygen_output)
unset(_asc_cpp_doxygen_result)
unset(_asc_cpp_doxygen_warning_log)
