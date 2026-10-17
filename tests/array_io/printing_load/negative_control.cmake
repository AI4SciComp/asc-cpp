cmake_minimum_required(VERSION 3.25)
execute_process(COMMAND "${EXECUTABLE}" RESULT_VARIABLE _result
  OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
if(NOT "${_result}" STREQUAL "1")
  message(FATAL_ERROR
    "Uninstrumented diagnostic must reject missing observations, exit=${_result}.\n${_stdout}${_stderr}")
endif()
message(STATUS "Uninstrumented diagnostic exited 1 as required; missing observation cannot pass.")
