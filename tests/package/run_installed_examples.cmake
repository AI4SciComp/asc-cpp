cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS
    ASCCPP_PACKAGE_DIR
    CONFIG
    CXX_COMPILER
    EXAMPLES_SOURCE_DIR
    GENERATOR
    WORK_DIR
    ASC_CPP_TEST_WORKSPACE_ROOT
    ASC_CPP_TEST_WORKSPACE_GUARD
)
  if(NOT DEFINED "${_required}" OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/PrepareTestWorkspace.cmake")
asc_cpp_prepare_test_workspace(
  WORK_DIR "${WORK_DIR}"
  ROOT "${ASC_CPP_TEST_WORKSPACE_ROOT}"
  GUARD "${ASC_CPP_TEST_WORKSPACE_GUARD}"
)

function(_run description)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
  )
  if(NOT _result EQUAL 0)
    message(FATAL_ERROR
      "${description} failed (${_result}).\nstdout:\n${_stdout}\nstderr:\n${_stderr}"
    )
  endif()
  message(STATUS "${description}\n${_stdout}${_stderr}")
endfunction()

set(_generator_arguments -G "${GENERATOR}")
if(DEFINED GENERATOR_PLATFORM AND NOT GENERATOR_PLATFORM STREQUAL "")
  list(APPEND _generator_arguments -A "${GENERATOR_PLATFORM}")
endif()
if(DEFINED GENERATOR_TOOLSET AND NOT GENERATOR_TOOLSET STREQUAL "")
  list(APPEND _generator_arguments -T "${GENERATOR_TOOLSET}")
endif()
if(DEFINED MAKE_PROGRAM AND NOT MAKE_PROGRAM STREQUAL "")
  list(APPEND _generator_arguments
    "-DCMAKE_MAKE_PROGRAM:FILEPATH=${MAKE_PROGRAM}"
  )
endif()

set(_build_dir "${WORK_DIR}/build with spaces")
_run(
  "installed examples configure"
  "${CMAKE_COMMAND}"
  -S "${EXAMPLES_SOURCE_DIR}"
  -B "${_build_dir}"
  ${_generator_arguments}
  "-DASCCpp_DIR:PATH=${ASCCPP_PACKAGE_DIR}"
  "-DCMAKE_CXX_COMPILER:FILEPATH=${CXX_COMPILER}"
  "-DBUILD_TESTING:BOOL=ON"
  "-DASC_CPP_EXAMPLES_ENABLE_CUDA:BOOL=OFF"
  "-DCMAKE_FIND_USE_PACKAGE_REGISTRY:BOOL=OFF"
  "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY:BOOL=OFF"
)
_run(
  "installed examples build"
  "${CMAKE_COMMAND}" --build "${_build_dir}" --config "${CONFIG}"
)
_run(
  "installed examples runtime"
  "${CMAKE_CTEST_COMMAND}" --test-dir "${_build_dir}" -C "${CONFIG}"
  --no-tests=error --output-on-failure
)
