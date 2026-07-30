cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS
    ASCCPP_PACKAGE_DIR
    WORK_DIR
    TRIAL_MODE
    ASC_CPP_TEST_WORKSPACE_ROOT
    ASC_CPP_TEST_WORKSPACE_GUARD
)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()
if(NOT IS_DIRECTORY "${ASCCPP_PACKAGE_DIR}")
  message(FATAL_ERROR
    "ASCCPP_PACKAGE_DIR does not exist: ${ASCCPP_PACKAGE_DIR}"
  )
endif()
if(NOT DEFINED TEST_GENERATOR OR "${TEST_GENERATOR}" STREQUAL "")
  set(TEST_GENERATOR "Unix Makefiles")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/PrepareTestWorkspace.cmake")
asc_cpp_prepare_test_workspace(
  WORK_DIR "${WORK_DIR}"
  ROOT "${ASC_CPP_TEST_WORKSPACE_ROOT}"
  GUARD "${ASC_CPP_TEST_WORKSPACE_GUARD}"
)

set(_audit_repository FALSE)
set(_commit_before "not-audited")
set(_status_before "not-audited")
if(DEFINED ASC_XDE_SOURCE_DIR AND NOT "${ASC_XDE_SOURCE_DIR}" STREQUAL "")
  set(_audit_repository TRUE)
  if(NOT IS_DIRECTORY "${ASC_XDE_SOURCE_DIR}/.git")
    message(FATAL_ERROR
      "ASC_XDE_SOURCE_DIR is not a Git checkout: ${ASC_XDE_SOURCE_DIR}"
    )
  endif()
  if(NOT DEFINED EXPECTED_ASC_XDE_COMMIT
     OR "${EXPECTED_ASC_XDE_COMMIT}" STREQUAL "")
    message(FATAL_ERROR
      "EXPECTED_ASC_XDE_COMMIT is required for a real repository audit"
    )
  endif()
  find_package(Git REQUIRED)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${ASC_XDE_SOURCE_DIR}" rev-parse HEAD
    RESULT_VARIABLE _commit_result
    OUTPUT_VARIABLE _commit_before
    ERROR_VARIABLE _commit_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(NOT _commit_result EQUAL 0)
    message(FATAL_ERROR "Cannot read asc-xde HEAD: ${_commit_error}")
  endif()
  if(NOT _commit_before STREQUAL EXPECTED_ASC_XDE_COMMIT)
    message(FATAL_ERROR
      "asc-xde commit mismatch: expected ${EXPECTED_ASC_XDE_COMMIT}, "
      "got ${_commit_before}"
    )
  endif()
  execute_process(
    COMMAND
      "${GIT_EXECUTABLE}" -C "${ASC_XDE_SOURCE_DIR}"
      status --porcelain=v1 --untracked-files=all
    RESULT_VARIABLE _status_result
    OUTPUT_VARIABLE _status_before
    ERROR_VARIABLE _status_error
  )
  if(NOT _status_result EQUAL 0)
    message(FATAL_ERROR "Cannot read asc-xde status: ${_status_error}")
  endif()
endif()

file(MAKE_DIRECTORY "${WORK_DIR}/source")
configure_file(
  "${CMAKE_CURRENT_LIST_DIR}/asc_xde_trial.cc"
  "${WORK_DIR}/source/asc_xde_trial.cc"
  COPYONLY
)
file(WRITE "${WORK_DIR}/source/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.25)
project(ASCCppM8AscXdeTrial LANGUAGES CXX)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_FIND_USE_PACKAGE_REGISTRY FALSE)
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS dense)

get_property(_imported DIRECTORY PROPERTY IMPORTED_TARGETS)
set(_actual_asc_targets)
foreach(_target IN LISTS _imported)
  if(_target MATCHES "^ASC::")
    list(APPEND _actual_asc_targets "${_target}")
  endif()
endforeach()
list(SORT _actual_asc_targets)
set(_expected_asc_targets ASC::core ASC::dense ASC::expression)
if(NOT "${_actual_asc_targets}" STREQUAL "${_expected_asc_targets}")
  message(FATAL_ERROR
    "The dense-only trial imported '${_actual_asc_targets}', expected "
    "'${_expected_asc_targets}'"
  )
endif()
foreach(_cuda_target IN ITEMS
    CUDA::cudart CUDA::cublas CUDA::cusparse
    ASC::core_cuda ASC::dense_cuda ASC::sparse_cuda ASC::random_cuda
    ASC::random_dense_cuda ASC::random_sparse_cuda)
  if(TARGET "${_cuda_target}")
    message(FATAL_ERROR
      "The CPU-only downstream trial created ${_cuda_target}"
    )
  endif()
endforeach()

add_executable(asc_xde_trial asc_xde_trial.cc)
target_link_libraries(asc_xde_trial PRIVATE ASC::dense)
target_compile_features(asc_xde_trial PRIVATE cxx_std_20)
get_target_property(_asc_dense_type ASC::dense TYPE)
if(WIN32 AND _asc_dense_type STREQUAL "SHARED_LIBRARY")
  add_custom_command(
    TARGET asc_xde_trial
    POST_BUILD
    COMMAND
      "${CMAKE_COMMAND}" -E copy_if_different
      "$<TARGET_RUNTIME_DLLS:asc_xde_trial>"
      "$<TARGET_FILE_DIR:asc_xde_trial>"
    COMMAND_EXPAND_LISTS
  )
endif()
]=])

set(_configure_command
  "${CMAKE_COMMAND}"
  -S "${WORK_DIR}/source"
  -B "${WORK_DIR}/build"
  -G "${TEST_GENERATOR}"
  "-DASCCpp_DIR=${ASCCPP_PACKAGE_DIR}"
  -DCMAKE_DISABLE_FIND_PACKAGE_CUDAToolkit=TRUE
  -DCMAKE_FIND_USE_PACKAGE_REGISTRY=FALSE
)
if(DEFINED CXX_COMPILER AND NOT "${CXX_COMPILER}" STREQUAL "")
  list(APPEND _configure_command "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}")
endif()
if(DEFINED CMAKE_PREFIX_PATH_ARGUMENT
   AND NOT "${CMAKE_PREFIX_PATH_ARGUMENT}" STREQUAL "")
  list(APPEND _configure_command
    "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH_ARGUMENT}"
  )
endif()
execute_process(
  COMMAND ${_configure_command}
  RESULT_VARIABLE _configure_result
  OUTPUT_VARIABLE _configure_stdout
  ERROR_VARIABLE _configure_stderr
)
if(NOT _configure_result EQUAL 0)
  message(FATAL_ERROR
    "asc-xde-shaped trial configure failed (${_configure_result}).\n"
    "stdout:\n${_configure_stdout}\n"
    "stderr:\n${_configure_stderr}"
  )
endif()

set(_build_command
  "${CMAKE_COMMAND}" --build "${WORK_DIR}/build" --parallel 2
)
if(DEFINED TEST_CONFIGURATION AND NOT "${TEST_CONFIGURATION}" STREQUAL "")
  list(APPEND _build_command --config "${TEST_CONFIGURATION}")
endif()
execute_process(
  COMMAND ${_build_command}
  RESULT_VARIABLE _build_result
  OUTPUT_VARIABLE _build_stdout
  ERROR_VARIABLE _build_stderr
)
if(NOT _build_result EQUAL 0)
  message(FATAL_ERROR
    "asc-xde-shaped trial build failed (${_build_result}).\n"
    "stdout:\n${_build_stdout}\n"
    "stderr:\n${_build_stderr}"
  )
endif()

set(_executable "${WORK_DIR}/build/asc_xde_trial")
if(DEFINED TEST_CONFIGURATION AND NOT "${TEST_CONFIGURATION}" STREQUAL "")
  set(_configured_executable
    "${WORK_DIR}/build/${TEST_CONFIGURATION}/asc_xde_trial"
  )
  if(EXISTS "${_configured_executable}")
    set(_executable "${_configured_executable}")
  endif()
endif()
execute_process(
  COMMAND "${_executable}"
  RESULT_VARIABLE _run_result
  OUTPUT_VARIABLE _run_stdout
  ERROR_VARIABLE _run_stderr
)
if(NOT _run_result EQUAL 0)
  message(FATAL_ERROR
    "asc-xde-shaped trial runtime failed (${_run_result}).\n"
    "stdout:\n${_run_stdout}\n"
    "stderr:\n${_run_stderr}"
  )
endif()

set(_commit_after "${_commit_before}")
set(_status_after "${_status_before}")
if(_audit_repository)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${ASC_XDE_SOURCE_DIR}" rev-parse HEAD
    RESULT_VARIABLE _commit_after_result
    OUTPUT_VARIABLE _commit_after
    ERROR_VARIABLE _commit_after_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  execute_process(
    COMMAND
      "${GIT_EXECUTABLE}" -C "${ASC_XDE_SOURCE_DIR}"
      status --porcelain=v1 --untracked-files=all
    RESULT_VARIABLE _status_after_result
    OUTPUT_VARIABLE _status_after
    ERROR_VARIABLE _status_after_error
  )
  if(NOT _commit_after_result EQUAL 0 OR NOT _status_after_result EQUAL 0)
    message(FATAL_ERROR
      "Cannot re-read asc-xde state after the trial: "
      "${_commit_after_error}${_status_after_error}"
    )
  endif()
  if(NOT _commit_after STREQUAL _commit_before
     OR NOT "${_status_after}" STREQUAL "${_status_before}")
    message(FATAL_ERROR
      "The real asc-xde repository changed during the isolated trial"
    )
  endif()
endif()

file(WRITE "${WORK_DIR}/trial-record.txt"
  "trial=asc-xde-shaped ASCCpp public-package trial\n"
  "mode=${TRIAL_MODE}\n"
  "component=dense\n"
  "target_closure=ASC::core;ASC::dense;ASC::expression\n"
  "package_dir=${ASCCPP_PACKAGE_DIR}\n"
  "asc_xde_commit=${_commit_before}\n"
  "asc_xde_status_before=${_status_before}\n"
  "asc_xde_status_after=${_status_after}\n"
  "real_repository_audit=${_audit_repository}\n"
  "real_downstream_write=no\n"
  "cuda_discovery=disabled\n"
  "runtime_result=pass\n"
)
message(STATUS "asc-xde-shaped ${TRIAL_MODE} trial passed")
