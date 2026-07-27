cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS SOURCE_DIR ASCCMAKE_DIR GENERATOR WORK_DIR)
  if(NOT DEFINED "${_required}" OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required.")
  endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
set(_missing_compiler
  "${WORK_DIR}/deliberately absent CUDA compiler/asc-nvcc"
)
set(_command
  "${CMAKE_COMMAND}"
  -S "${SOURCE_DIR}"
  -B "${WORK_DIR}/build"
  -G "${GENERATOR}"
  "-DASCCMake_DIR:PATH=${ASCCMAKE_DIR}"
  "-DASC_CPP_BUILD_TESTING:BOOL=OFF"
  "-DASC_CPP_ENABLE_CUDA:BOOL=ON"
  "-DBUILD_TESTING:BOOL=OFF"
  "-DCMAKE_CUDA_COMPILER:FILEPATH=${_missing_compiler}"
)
if(DEFINED GENERATOR_PLATFORM AND NOT GENERATOR_PLATFORM STREQUAL "")
  list(APPEND _command -A "${GENERATOR_PLATFORM}")
endif()
if(DEFINED GENERATOR_TOOLSET AND NOT GENERATOR_TOOLSET STREQUAL "")
  list(APPEND _command -T "${GENERATOR_TOOLSET}")
endif()
if(DEFINED MAKE_PROGRAM AND NOT MAKE_PROGRAM STREQUAL "")
  list(APPEND _command "-DCMAKE_MAKE_PROGRAM:FILEPATH=${MAKE_PROGRAM}")
endif()

execute_process(
  COMMAND ${_command}
  RESULT_VARIABLE _result
  OUTPUT_VARIABLE _stdout
  ERROR_VARIABLE _stderr
)
if(_result EQUAL 0)
  message(FATAL_ERROR
    "CUDA-enabled configuration unexpectedly accepted a missing compiler."
  )
endif()
set(_output "${_stdout}\n${_stderr}")
if(NOT _output MATCHES "[Cc][Uu][Dd][Aa]")
  message(FATAL_ERROR
    "CUDA-unavailable failure did not identify CUDA.\n${_output}"
  )
endif()
