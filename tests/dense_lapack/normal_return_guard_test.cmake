if(NOT DEFINED PROBE OR NOT EXISTS "${PROBE}")
  message(FATAL_ERROR "A built normal-return probe is required")
endif()

foreach(_mode IN ITEMS return exit provider_stop)
  execute_process(COMMAND "${PROBE}" "${_mode}"
    RESULT_VARIABLE _result OUTPUT_VARIABLE _output ERROR_VARIABLE _error
    TIMEOUT 10)
  if(_mode STREQUAL "return")
    if(NOT _result STREQUAL "0" OR NOT _error STREQUAL "")
      message(FATAL_ERROR "Normal return failed: ${_result}\n${_output}${_error}")
    endif()
  else()
    if(NOT _result STREQUAL "93" OR
        NOT _error MATCHES "LAPACK test did not return from main")
      message(FATAL_ERROR
        "${_mode} escaped the return guard: ${_result}\n${_output}${_error}")
    endif()
    if(_mode STREQUAL "provider_stop" AND NOT _output MATCHES "DGETRF")
      message(FATAL_ERROR "The pinned DGETRF error handler was not observed")
    endif()
  endif()
  message(STATUS "Normal-return control ${_mode}: ${_result}")
endforeach()
