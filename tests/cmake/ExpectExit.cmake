cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS PROGRAM EXPECTED_EXIT ENVIRONMENT)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()

execute_process(
  COMMAND
    "${CMAKE_COMMAND}" -E env "${ENVIRONMENT}" "${PROGRAM}"
  RESULT_VARIABLE _result
)
if(NOT _result MATCHES "^-?[0-9]+$" OR NOT _result EQUAL EXPECTED_EXIT)
  message(FATAL_ERROR
    "Expected '${PROGRAM}' to exit ${EXPECTED_EXIT}, got '${_result}'"
  )
endif()
