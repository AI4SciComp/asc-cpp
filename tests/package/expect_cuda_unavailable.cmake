cmake_minimum_required(VERSION 3.25)

foreach(_required_variable IN ITEMS
    SOURCE_DIR
    GENERATOR
    WORK_DIR
    ASCCMAKE_DIR
    ASC_CPP_TEST_WORKSPACE_ROOT
    ASC_CPP_TEST_WORKSPACE_GUARD
)
  if(NOT DEFINED "${_required_variable}"
     OR "${${_required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${_required_variable} is required.")
  endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/PrepareTestWorkspace.cmake")
asc_cpp_prepare_test_workspace(
  WORK_DIR "${WORK_DIR}"
  ROOT "${ASC_CPP_TEST_WORKSPACE_ROOT}"
  GUARD "${ASC_CPP_TEST_WORKSPACE_GUARD}"
)

set(_generator_arguments -G "${GENERATOR}")
if(DEFINED GENERATOR_PLATFORM AND NOT GENERATOR_PLATFORM STREQUAL "")
  list(APPEND _generator_arguments -A "${GENERATOR_PLATFORM}")
endif()
if(DEFINED GENERATOR_TOOLSET AND NOT GENERATOR_TOOLSET STREQUAL "")
  list(APPEND _generator_arguments -T "${GENERATOR_TOOLSET}")
endif()
if(DEFINED MAKE_PROGRAM AND NOT MAKE_PROGRAM STREQUAL "")
  list(APPEND
    _generator_arguments
    "-DCMAKE_MAKE_PROGRAM:FILEPATH=${MAKE_PROGRAM}"
  )
endif()

set(_missing_compiler
  "${WORK_DIR}/intentionally absent CUDA compiler/nvcc"
)
execute_process(
  COMMAND
    "${CMAKE_COMMAND}"
    -S "${SOURCE_DIR}"
    -B "${WORK_DIR}/requested CUDA without compiler"
    ${_generator_arguments}
    "-DASCCMake_DIR:PATH=${ASCCMAKE_DIR}"
    "-DBUILD_TESTING:BOOL=OFF"
    "-DASC_CPP_BUILD_TESTING:BOOL=OFF"
    "-DASC_CPP_ENABLE_CUDA:BOOL=ON"
    "-DCMAKE_CUDA_COMPILER:FILEPATH=${_missing_compiler}"
  RESULT_VARIABLE _configure_result
  OUTPUT_VARIABLE _configure_stdout
  ERROR_VARIABLE _configure_stderr
)
if(_configure_result EQUAL 0)
  message(FATAL_ERROR
    "ASC_CPP_ENABLE_CUDA=ON unexpectedly accepted a missing CUDA compiler."
  )
endif()
set(_diagnostic "${_configure_stdout}\n${_configure_stderr}")
if(NOT _diagnostic MATCHES
   "(ASC_CPP_ENABLE_CUDA|CUDA compiler|CMAKE_CUDA_COMPILER|nvcc)")
  message(FATAL_ERROR
    "Missing-CUDA failure lacks an actionable diagnostic.\n${_diagnostic}"
  )
endif()
