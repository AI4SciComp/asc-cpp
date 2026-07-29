cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS EXECUTABLE MODE)
  if(NOT DEFINED "${_required}" OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required.")
  endif()
endforeach()

if(DEFINED WORK_DIR AND NOT WORK_DIR STREQUAL "")
  set(_working_directory "${WORK_DIR}")
else()
  set(_working_directory "${CMAKE_CURRENT_BINARY_DIR}")
endif()
file(MAKE_DIRECTORY "${_working_directory}")

if(MODE STREQUAL "dcheck")
  execute_process(
    COMMAND "${EXECUTABLE}" dcheck-enabled
    RESULT_VARIABLE _dcheck_probe
    OUTPUT_QUIET
    ERROR_QUIET
    WORKING_DIRECTORY "${_working_directory}"
  )
  if("${_dcheck_probe}" STREQUAL "3")
    set(_expect_failure TRUE)
  elseif("${_dcheck_probe}" STREQUAL "0")
    set(_expect_failure FALSE)
  else()
    message(FATAL_ERROR
      "Unable to determine ASC_DCHECK build state: ${_dcheck_probe}"
    )
  endif()
else()
  set(_expect_failure TRUE)
endif()

if(MODE STREQUAL "result-no-allocation")
  execute_process(
    COMMAND "${EXECUTABLE}"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
    WORKING_DIRECTORY "${_working_directory}"
    TIMEOUT 10
  )
else()
  execute_process(
    COMMAND "${EXECUTABLE}" "${MODE}"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
    WORKING_DIRECTORY "${_working_directory}"
    TIMEOUT 10
  )
endif()
if(MODE STREQUAL "file-move-assignment")
  file(REMOVE
    "${_working_directory}/open move-assignment destination.tmp"
    "${_working_directory}/open move-assignment source.tmp"
  )
endif()
if(_expect_failure AND "${_result}" STREQUAL "0")
  message(FATAL_ERROR
    "${MODE} misuse unexpectedly completed successfully.\n"
    "stdout:\n${_stdout}\n"
    "stderr:\n${_stderr}"
  )
endif()
if(NOT _expect_failure AND NOT "${_result}" STREQUAL "0")
  message(FATAL_ERROR
    "Release-disabled ASC_DCHECK unexpectedly failed: ${_result}.\n"
    "stdout:\n${_stdout}\n"
    "stderr:\n${_stderr}"
  )
endif()

if(_expect_failure)
  set(_combined_output "${_stdout}\n${_stderr}")
  if(MODE STREQUAL "result" OR MODE STREQUAL "result-no-allocation")
    set(_expected_pattern "value_\\.has_value")
  elseif(MODE STREQUAL "check")
    set(_expected_pattern "false")
  elseif(MODE STREQUAL "file-move-assignment")
    set(_expected_pattern "is_open")
  else()
    set(_expected_pattern "false")
  endif()
  if(NOT _combined_output MATCHES "${_expected_pattern}")
    message(FATAL_ERROR
      "${MODE} failure did not identify the violated expression.\n"
      "stdout:\n${_stdout}\n"
      "stderr:\n${_stderr}"
    )
  endif()
  if(_combined_output MATCHES "allocation attempted before FatalContract")
    message(FATAL_ERROR
      "${MODE} allocated while entering FatalContract.\n"
      "stdout:\n${_stdout}\n"
      "stderr:\n${_stderr}"
    )
  endif()
endif()
