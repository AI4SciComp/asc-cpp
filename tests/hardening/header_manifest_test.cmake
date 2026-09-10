cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS
    ASCCPP_PACKAGE_DIR
    SOURCE_INCLUDE_DIR
    WORK_DIR
    EXPECT_CUDA
    ASC_CPP_TEST_WORKSPACE_ROOT
    ASC_CPP_TEST_WORKSPACE_GUARD
)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/PrepareTestWorkspace.cmake")
asc_cpp_prepare_test_workspace(
  WORK_DIR "${WORK_DIR}"
  ROOT "${ASC_CPP_TEST_WORKSPACE_ROOT}"
  GUARD "${ASC_CPP_TEST_WORKSPACE_GUARD}"
)
if(NOT IS_DIRECTORY "${ASCCPP_PACKAGE_DIR}")
  message(FATAL_ERROR
    "ASCCPP_PACKAGE_DIR does not exist: ${ASCCPP_PACKAGE_DIR}"
  )
endif()
if(NOT IS_DIRECTORY "${SOURCE_INCLUDE_DIR}")
  message(FATAL_ERROR "SOURCE_INCLUDE_DIR does not exist: ${SOURCE_INCLUDE_DIR}")
endif()
if(NOT DEFINED TEST_GENERATOR OR "${TEST_GENERATOR}" STREQUAL "")
  set(TEST_GENERATOR "Unix Makefiles")
endif()

set(_provider_free_entries
  "core|asc/core.h"
  "core|asc/core/array_format.h"
  "core|asc/core/array_io.h"
  "core|asc/core/configuration.h"
  "core|asc/core/contracts.h"
  "core|asc/core/execution.h"
  "core|asc/core/export.h"
  "core|asc/core/extents.h"
  "core|asc/core/io.h"
  "core|asc/core/matrix_market.h"
  "core|asc/core/memory.h"
  "core|asc/core/result.h"
  "core|asc/core/status.h"
  "core|asc/core/types.h"
  "utilities|asc/utilities.h"
  "utilities|asc/utilities/command_line.h"
  "utilities|asc/utilities/export.h"
  "utilities|asc/utilities/timer.h"
  "expression|asc/expression.h"
  "expression|asc/expression/expression.h"
  "expression|asc/expression/writable.h"
  "dense|asc/dense.h"
  "dense|asc/dense/array.h"
  "dense|asc/dense/evaluate.h"
  "dense|asc/dense/export.h"
  "dense|asc/dense/layout.h"
  "dense|asc/dense/matrix_market.h"
  "dense|asc/dense/blas.h"
  "dense|asc/dense/view.h"
  "dense|asc/dense/print.h"
  "dense|asc/dense/io.h"
  "dense|asc/dense/lapack/types.h"
  "dense|asc/dense/lapack/workspace.h"
  "dense|asc/dense/lapack/report.h"
  "dense|asc/dense/lapack/factor_view.h"
  "dense|asc/dense/lapack/structured_view.h"
  "dense|asc/dense/lapack/triangular_band_view.h"
  "dense|asc/dense/lapack/lu.h"
  "dense|asc/dense/lapack/cholesky.h"
  "dense|asc/dense/lapack/qr.h"
  "sparse|asc/sparse.h"
  "sparse|asc/sparse/compressed.h"
  "sparse|asc/sparse/coordinate.h"
  "sparse|asc/sparse/evaluate.h"
  "sparse|asc/sparse/export.h"
  "sparse|asc/sparse/blas.h"
  "sparse|asc/sparse/print.h"
  "sparse|asc/sparse/io.h"
  "sparse|asc/sparse/matrix_market.h"
  "random|asc/random.h"
  "random|asc/random/distribution.h"
  "random|asc/random/engine.h"
  "random|asc/random/export.h"
  "random|asc/random/generator.h"
  "random|asc/random/quasi.h"
  "random|asc/random/seed.h"
  "random_dense|asc/random/dense.h"
  "random_sparse|asc/random/sparse.h"
)
set(_cuda_entries
  "core_cuda|asc/core/providers/cuda.h"
  "core_cuda|asc/core/providers/cuda_export.h"
  "dense_cuda|asc/dense/providers/cuda.h"
  "dense_cuda|asc/dense/providers/cuda_export.h"
  "sparse_cuda|asc/sparse/providers/cuda.h"
  "sparse_cuda|asc/sparse/providers/cuda_export.h"
  "random_cuda|asc/random/providers/cuda.h"
  "random_cuda|asc/random/providers/cuda_export.h"
  "random_dense_cuda|asc/random/providers/dense_cuda.h"
  "random_dense_cuda|asc/random/providers/dense_cuda_export.h"
  "random_sparse_cuda|asc/random/providers/sparse_cuda.h"
  "random_sparse_cuda|asc/random/providers/sparse_cuda_export.h"
)

set(_lapack_entries
  "dense_lapack|asc/dense/providers/lapack.h"
  "dense_lapack|asc/dense/providers/lapack_export.h"
  "dense_lapack|asc/dense/providers/lapack_lu.h"
  "dense_lapack|asc/dense/providers/lapack_lu_equilibration.h"
  "dense_lapack|asc/dense/providers/lapack_lu_condition.h"
  "dense_lapack|asc/dense/providers/lapack_lu_refinement.h"
  "dense_lapack|asc/dense/providers/lapack_lu_driver.h"
  "dense_lapack|asc/dense/providers/lapack_lu_helpers.h"
  "dense_lapack|asc/dense/providers/lapack_cholesky.h"
  "dense_lapack|asc/dense/providers/lapack_cholesky_packed.h"
  "dense_lapack|asc/dense/providers/lapack_cholesky_packed_solve.h"
  "dense_lapack|asc/dense/providers/lapack_cholesky_packed_inverse.h"
  "dense_lapack|asc/dense/providers/lapack_cholesky_packed_driver.h"
  "dense_lapack|asc/dense/providers/lapack_cholesky_packed_equilibration.h"
  "dense_lapack|asc/dense/providers/lapack_cholesky_packed_condition.h"
  "dense_lapack|asc/dense/providers/lapack_cholesky_packed_refinement.h"
  "dense_lapack|asc/dense/providers/lapack_positive_tridiagonal.h"
  "dense_lapack|asc/dense/providers/lapack_cholesky_packed_robust.h"
  "dense_lapack|asc/dense/providers/lapack_qr.h"
  "dense_lapack|asc/dense/providers/lapack_cholesky_condition.h"
  "dense_lapack|asc/dense/providers/lapack_cholesky_driver.h"
  "dense_lapack|asc/dense/providers/lapack_cholesky_equilibration.h"
  "dense_lapack|asc/dense/providers/lapack_cholesky_refinement.h"
  "dense_lapack|asc/dense/providers/lapack_least_squares.h"
  "dense_lapack|asc/dense/providers/lapack_cholesky_band.h"
  "dense_lapack|asc/dense/providers/lapack_indefinite.h"
  "dense_lapack|asc/dense/providers/lapack_rank_revealing.h"
  "dense_lapack|asc/dense/providers/lapack_sylvester.h"
  "dense_lapack|asc/dense/providers/lapack_indefinite_condition.h"
  "dense_lapack|asc/dense/providers/lapack_indefinite_driver.h"
  "dense_lapack|asc/dense/providers/lapack_indefinite_refinement.h"
  "dense_lapack|asc/dense/providers/lapack_svd_least_squares.h"
  "dense_lapack|asc/dense/providers/lapack_lu_band.h"
  "dense_lapack|asc/dense/providers/lapack_general_band.h"
  "dense_lapack|asc/dense/providers/lapack_lu_band_condition.h"
  "dense_lapack|asc/dense/providers/lapack_lu_band_driver.h"
  "dense_lapack|asc/dense/providers/lapack_lu_band_equilibration.h"
  "dense_lapack|asc/dense/providers/lapack_lu_band_equilibration_radix.h"
  "dense_lapack|asc/dense/providers/lapack_triangular.h"
  "dense_lapack|asc/dense/providers/lapack_triangular_condition.h"
  "dense_lapack|asc/dense/providers/lapack_triangular_error_bounds.h"
  "dense_lapack|asc/dense/providers/lapack_triangular_packed.h"
  "dense_lapack|asc/dense/providers/lapack_triangular_band.h"
  "dense_lapack|asc/dense/providers/lapack_triangular_band_condition.h"
  "dense_lapack|asc/dense/providers/lapack_triangular_band_error_bounds.h"
  "dense_lapack|asc/dense/providers/lapack_triangular_packed_condition.h"
  "dense_lapack|asc/dense/providers/lapack_triangular_packed_error_bounds.h"
  "dense_lapack|asc/dense/providers/lapack_lu_band_expert.h"
  "dense_lapack|asc/dense/providers/lapack_lu_band_refinement.h"
  "dense_lapack|asc/dense/providers/lapack_tridiagonal.h"
  "dense_lapack|asc/dense/providers/lapack_tridiagonal_condition.h"
  "dense_lapack|asc/dense/providers/lapack_tridiagonal_driver.h"
  "dense_lapack|asc/dense/providers/lapack_tridiagonal_refinement.h"
  "dense_lapack|asc/dense/providers/lapack_cholesky_band_condition.h"
  "dense_lapack|asc/dense/providers/lapack_cholesky_band_driver.h"
  "dense_lapack|asc/dense/providers/lapack_cholesky_band_equilibration.h"
  "dense_lapack|asc/dense/providers/lapack_cholesky_band_expert.h"
  "dense_lapack|asc/dense/providers/lapack_cholesky_band_refinement.h")
set(_all_entries ${_provider_free_entries} ${_cuda_entries} ${_lapack_entries})
set(_all_headers)
foreach(_entry IN LISTS _all_entries)
  string(REGEX REPLACE "^[^|]+\\|" "" _header "${_entry}")
  list(APPEND _all_headers "${_header}")
endforeach()
list(SORT _all_headers)
list(REMOVE_DUPLICATES _all_headers)
list(LENGTH _all_headers _all_header_count)
if(NOT _all_header_count EQUAL 128)
  message(FATAL_ERROR
    "Independent source-header oracle must contain 128 headers; got "
    "${_all_header_count}"
  )
endif()

file(GLOB_RECURSE _source_headers
  RELATIVE "${SOURCE_INCLUDE_DIR}"
  "${SOURCE_INCLUDE_DIR}/asc/*.h"
)
list(SORT _source_headers)
if(NOT "${_source_headers}" STREQUAL "${_all_headers}")
  message(FATAL_ERROR
    "Source public-header tree differs from the frozen 128-header oracle.\n"
    "Expected: ${_all_headers}\n"
    "Actual: ${_source_headers}"
  )
endif()

set(_enabled_entries ${_provider_free_entries})
set(_enabled_components
  core utilities expression dense sparse random random_dense random_sparse
)
if(EXPECT_LAPACK)
  list(APPEND _enabled_entries ${_lapack_entries})
  list(APPEND _enabled_components dense_lapack)
endif()
if(EXPECT_CUDA)
  list(APPEND _enabled_entries ${_cuda_entries})
  list(APPEND _enabled_components
    core_cuda dense_cuda sparse_cuda random_cuda random_dense_cuda
    random_sparse_cuda
  )
endif()
list(SORT _enabled_entries)

file(MAKE_DIRECTORY "${WORK_DIR}/source")
string(JOIN " " _component_arguments ${_enabled_components})
set(EXPECTED_ENTRIES "${_enabled_entries}")
set(COMPONENT_ARGUMENTS "${_component_arguments}")
set(ENABLED_COMPONENTS "${_enabled_components}")
set(_probe_template [=[
cmake_minimum_required(VERSION 3.25)
project(ASCCppM8HeaderManifestProbe LANGUAGES NONE)
set(CMAKE_FIND_USE_PACKAGE_REGISTRY FALSE)
string(REPLACE "|" ";" ASC_CPP_LAPACK_RUNTIME_LIBRARIES
  "${ASC_TEST_LAPACK_RUNTIMES}")
find_package(
  ASCCpp 0.9 CONFIG REQUIRED
  COMPONENTS @COMPONENT_ARGUMENTS@
)
set(_expected_entries "@EXPECTED_ENTRIES@")
set(_enabled_components "@ENABLED_COMPONENTS@")
set(_actual_entries)
foreach(_component IN LISTS _enabled_components)
  if(NOT TARGET "ASC::${_component}")
    message(FATAL_ERROR "Missing imported target ASC::${_component}")
  endif()
  get_target_property(_header_sets "ASC::${_component}"
    INTERFACE_HEADER_SETS
  )
  if(NOT "HEADERS" IN_LIST _header_sets)
    message(FATAL_ERROR
      "ASC::${_component} does not export its HEADERS file set"
    )
  endif()
  get_target_property(_headers "ASC::${_component}" HEADER_SET)
  if(NOT _headers)
    message(FATAL_ERROR "ASC::${_component} has an empty HEADERS file set")
  endif()
  foreach(_header IN LISTS _headers)
    file(TO_CMAKE_PATH "${_header}" _normalized)
    string(REGEX REPLACE "^.*(/asc/)" "asc/" _relative "${_normalized}")
    if(NOT _relative MATCHES "^asc/")
      message(FATAL_ERROR
        "Cannot normalize exported header '${_header}' for ASC::${_component}"
      )
    endif()
    list(APPEND _actual_entries "${_component}|${_relative}")
  endforeach()
endforeach()
list(SORT _actual_entries)
if(NOT "${_actual_entries}" STREQUAL "${_expected_entries}")
  message(FATAL_ERROR
    "Exported target header file sets differ from the frozen oracle.\n"
    "Expected: ${_expected_entries}\n"
    "Actual: ${_actual_entries}"
  )
endif()
file(WRITE "${CMAKE_BINARY_DIR}/header-manifest.txt"
  "${_actual_entries}\n"
)
]=])
string(CONFIGURE "${_probe_template}" _probe @ONLY)
file(WRITE "${WORK_DIR}/source/CMakeLists.txt" "${_probe}")

set(_configure_command
  "${CMAKE_COMMAND}"
  -S "${WORK_DIR}/source"
  -B "${WORK_DIR}/build"
  -G "${TEST_GENERATOR}"
  "-DASCCpp_DIR=${ASCCPP_PACKAGE_DIR}"
  -DCMAKE_FIND_USE_PACKAGE_REGISTRY=FALSE
  "-DASC_CPP_LAPACK_ROOT:PATH=${LAPACK_ROOT}"
  "-DASC_TEST_LAPACK_RUNTIMES:STRING=${LAPACK_RUNTIMES}"
)
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
    "Header file-set probe configure failed (${_configure_result}).\n"
    "stdout:\n${_configure_stdout}\n"
    "stderr:\n${_configure_stderr}"
  )
endif()

if(DEFINED INSTALL_INCLUDE_DIR AND NOT "${INSTALL_INCLUDE_DIR}" STREQUAL "")
  if(NOT IS_DIRECTORY "${INSTALL_INCLUDE_DIR}")
    message(FATAL_ERROR
      "INSTALL_INCLUDE_DIR does not exist: ${INSTALL_INCLUDE_DIR}"
    )
  endif()
  set(_expected_installed)
  foreach(_entry IN LISTS _enabled_entries)
    string(REGEX REPLACE "^[^|]+\\|" "" _header "${_entry}")
    list(APPEND _expected_installed "${_header}")
  endforeach()
  list(SORT _expected_installed)
  list(REMOVE_DUPLICATES _expected_installed)

  file(GLOB_RECURSE _installed_headers
    RELATIVE "${INSTALL_INCLUDE_DIR}"
    "${INSTALL_INCLUDE_DIR}/asc/*.h"
  )
  list(SORT _installed_headers)
  if(NOT "${_installed_headers}" STREQUAL "${_expected_installed}")
    message(FATAL_ERROR
      "Installed physical header tree differs from enabled target file sets.\n"
      "Expected: ${_expected_installed}\n"
      "Actual: ${_installed_headers}"
    )
  endif()
  foreach(_header IN LISTS _expected_installed)
    execute_process(
      COMMAND
        "${CMAKE_COMMAND}" -E compare_files
        "${SOURCE_INCLUDE_DIR}/${_header}"
        "${INSTALL_INCLUDE_DIR}/${_header}"
      RESULT_VARIABLE _compare_result
    )
    if(NOT _compare_result EQUAL 0)
      message(FATAL_ERROR
        "Installed header is not byte-equivalent to source: ${_header}"
      )
    endif()
  endforeach()
endif()

message(STATUS
  "Verified source and exported header manifest "
  "(CUDA enabled: ${EXPECT_CUDA})"
)
