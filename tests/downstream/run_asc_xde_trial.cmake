cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS
    PACKAGE_DIR
    WORK_DIR
    GENERATOR
    ASC_XDE_REPOSITORY
)
  if(NOT DEFINED "${_required}" OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required.")
  endif()
endforeach()

set(_approved_commit "abcb29b51f22f40afd7f174707b7ccf83c32d4bf")
function(_run_git output_variable)
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" -E env GIT_OPTIONAL_LOCKS=0
      git -C "${ASC_XDE_REPOSITORY}" ${ARGN}
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(NOT _result EQUAL 0)
    message(FATAL_ERROR
      "Read-only asc-xde inspection failed.\n"
      "stdout:\n${_stdout}\n"
      "stderr:\n${_stderr}"
    )
  endif()
  set("${output_variable}" "${_stdout}" PARENT_SCOPE)
endfunction()

_run_git(_head_before rev-parse HEAD)
_run_git(_status_before status --porcelain=v1 --untracked-files=all)
if(NOT _head_before STREQUAL _approved_commit)
  message(FATAL_ERROR
    "asc-xde HEAD is '${_head_before}', expected '${_approved_commit}'."
  )
endif()
if(NOT _status_before STREQUAL "")
  message(FATAL_ERROR
    "The real asc-xde repository must be clean before the trial: "
    "'${_status_before}'."
  )
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
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

set(_build_dir "${WORK_DIR}/asc-xde trial build with spaces")
execute_process(
  COMMAND
    "${CMAKE_COMMAND}"
    -S "${CMAKE_CURRENT_LIST_DIR}/asc_xde_trial"
    -B "${_build_dir}"
    ${_generator_arguments}
    "-DASCCpp_DIR:PATH=${PACKAGE_DIR}"
    "-DASC_XDE_TRIAL_BASELINE:STRING=${_approved_commit}"
    "-DCMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY:BOOL=ON"
    "-DCMAKE_FIND_USE_PACKAGE_REGISTRY:BOOL=OFF"
    "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY:BOOL=OFF"
  RESULT_VARIABLE _configure_result
  OUTPUT_VARIABLE _configure_stdout
  ERROR_VARIABLE _configure_stderr
)
if(NOT _configure_result EQUAL 0)
  message(FATAL_ERROR
    "asc-xde trial configuration failed.\n"
    "stdout:\n${_configure_stdout}\n"
    "stderr:\n${_configure_stderr}"
  )
endif()

set(_build_command "${CMAKE_COMMAND}" --build "${_build_dir}")
if(DEFINED CONFIG AND NOT CONFIG STREQUAL "")
  list(APPEND _build_command --config "${CONFIG}")
endif()
execute_process(
  COMMAND ${_build_command}
  RESULT_VARIABLE _build_result
  OUTPUT_VARIABLE _build_stdout
  ERROR_VARIABLE _build_stderr
)
if(NOT _build_result EQUAL 0)
  message(FATAL_ERROR
    "asc-xde trial build failed.\n"
    "stdout:\n${_build_stdout}\n"
    "stderr:\n${_build_stderr}"
  )
endif()

set(_ctest_command
  "${CMAKE_CTEST_COMMAND}" --test-dir "${_build_dir}" --output-on-failure
)
if(DEFINED CONFIG AND NOT CONFIG STREQUAL "")
  list(APPEND _ctest_command -C "${CONFIG}")
endif()
execute_process(
  COMMAND ${_ctest_command}
  RESULT_VARIABLE _test_result
  OUTPUT_VARIABLE _test_stdout
  ERROR_VARIABLE _test_stderr
)
if(NOT _test_result EQUAL 0)
  message(FATAL_ERROR
    "asc-xde trial runtime failed.\n"
    "stdout:\n${_test_stdout}\n"
    "stderr:\n${_test_stderr}"
  )
endif()

_run_git(_head_after rev-parse HEAD)
_run_git(_status_after status --porcelain=v1 --untracked-files=all)
if(NOT _head_after STREQUAL _head_before
   OR NOT _status_after STREQUAL _status_before)
  message(FATAL_ERROR
    "The real asc-xde repository changed during the trial.\n"
    "before: ${_head_before}; '${_status_before}'\n"
    "after:  ${_head_after}; '${_status_after}'"
  )
endif()

message(STATUS
  "M8 asc-xde trial passed at ${_approved_commit}; real repository unchanged."
)
