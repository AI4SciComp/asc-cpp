cmake_minimum_required(VERSION 3.25)

if(POLICY CMP0159)
  cmake_policy(SET CMP0159 NEW)
endif()

if(NOT DEFINED SOURCE_DIR OR SOURCE_DIR STREQUAL "")
  message(FATAL_ERROR "SOURCE_DIR is required.")
endif()

set(_expected_public_files
  include/asc/dense.h
  include/asc/dense/array.h
  include/asc/dense/evaluate.h
  include/asc/dense/export.h
  include/asc/dense/layout.h
  include/asc/dense/blas.h
  include/asc/dense/view.h
  include/asc/dense/print.h
  include/asc/dense/io.h
  include/asc/dense/matrix_market.h
  include/asc/dense/lapack/types.h
  include/asc/dense/lapack/workspace.h
  include/asc/dense/lapack/report.h
  include/asc/dense/lapack/factor_view.h
  include/asc/dense/lapack/structured_view.h
  include/asc/dense/lapack/triangular_band_view.h
  include/asc/dense/lapack/lu.h
  include/asc/dense/lapack/cholesky.h
  include/asc/dense/lapack/qr.h
)
set(_expected_source_files
  src/dense/blas.cc
  src/dense/array_io.cc
  src/dense/matrix_market.cc
  src/dense/blas_level1.cc
  src/dense/blas_level2.cc
  src/dense/blas_level3.cc
  src/dense/lapack_foundations.cc
  src/dense/lapack_lu.cc
  src/dense/lapack_cholesky.cc
  src/dense/lapack_qr.cc
)

set(_observed_public_files)
if(EXISTS "${SOURCE_DIR}/include/asc/dense.h")
  list(APPEND _observed_public_files include/asc/dense.h)
endif()
file(
  GLOB_RECURSE _dense_headers
  LIST_DIRECTORIES FALSE
  RELATIVE "${SOURCE_DIR}"
  "${SOURCE_DIR}/include/asc/dense/*"
)
list(APPEND _observed_public_files ${_dense_headers})
# The separately audited Dense CUDA and reference LAPACK facets are not part of this
# provider-free Dense dependency check.
list(REMOVE_ITEM
  _observed_public_files
  include/asc/dense/providers/cuda.h
  include/asc/dense/providers/cuda_export.h
  include/asc/dense/providers/lapack.h
  include/asc/dense/providers/lapack_export.h
  include/asc/dense/providers/lapack_lu.h
  include/asc/dense/providers/lapack_lu_equilibration.h
  include/asc/dense/providers/lapack_lu_condition.h
  include/asc/dense/providers/lapack_lu_refinement.h
  include/asc/dense/providers/lapack_lu_driver.h
  include/asc/dense/providers/lapack_lu_helpers.h
  include/asc/dense/providers/lapack_cholesky.h
  include/asc/dense/providers/lapack_cholesky_packed.h
  include/asc/dense/providers/lapack_cholesky_packed_solve.h
  include/asc/dense/providers/lapack_cholesky_packed_inverse.h
  include/asc/dense/providers/lapack_cholesky_packed_driver.h
  include/asc/dense/providers/lapack_cholesky_packed_equilibration.h
  include/asc/dense/providers/lapack_cholesky_packed_condition.h
  include/asc/dense/providers/lapack_cholesky_packed_refinement.h
  include/asc/dense/providers/lapack_mixed_general.h
  include/asc/dense/providers/lapack_mixed_positive.h
  include/asc/dense/providers/lapack_precision_conversion.h
  include/asc/dense/providers/lapack_matrix_copy.h
  include/asc/dense/providers/lapack_matrix_scale.h
  include/asc/dense/providers/lapack_matrix_set.h
  include/asc/dense/providers/lapack_dmd.h
  include/asc/dense/providers/lapack_dmd_qr.h
  include/asc/dense/providers/lapack_positive_tridiagonal.h
  include/asc/dense/providers/lapack_positive_tridiagonal_condition.h
  include/asc/dense/providers/lapack_positive_tridiagonal_refinement.h
  include/asc/dense/providers/lapack_positive_tridiagonal_driver.h
  include/asc/dense/providers/lapack_cholesky_packed_robust.h
  include/asc/dense/providers/lapack_qr.h
  include/asc/dense/providers/lapack_cholesky_condition.h
  include/asc/dense/providers/lapack_cholesky_driver.h
  include/asc/dense/providers/lapack_cholesky_equilibration.h
  include/asc/dense/providers/lapack_cholesky_refinement.h
  include/asc/dense/providers/lapack_least_squares.h
  include/asc/dense/providers/lapack_cholesky_band.h
  include/asc/dense/providers/lapack_indefinite.h
  include/asc/dense/providers/lapack_rank_revealing.h
  include/asc/dense/providers/lapack_sylvester.h
  include/asc/dense/providers/lapack_indefinite_condition.h
  include/asc/dense/providers/lapack_indefinite_driver.h
  include/asc/dense/providers/lapack_indefinite_refinement.h
  include/asc/dense/providers/lapack_svd_least_squares.h
  include/asc/dense/providers/lapack_lu_band.h
  include/asc/dense/providers/lapack_general_band.h
  include/asc/dense/providers/lapack_lu_band_condition.h
  include/asc/dense/providers/lapack_lu_band_driver.h
  include/asc/dense/providers/lapack_lu_band_equilibration.h
  include/asc/dense/providers/lapack_lu_band_equilibration_radix.h
  include/asc/dense/providers/lapack_triangular.h
  include/asc/dense/providers/lapack_triangular_condition.h
  include/asc/dense/providers/lapack_triangular_error_bounds.h
  include/asc/dense/providers/lapack_triangular_packed.h
  include/asc/dense/providers/lapack_triangular_band.h
  include/asc/dense/providers/lapack_triangular_band_condition.h
  include/asc/dense/providers/lapack_triangular_band_error_bounds.h
  include/asc/dense/providers/lapack_triangular_packed_condition.h
  include/asc/dense/providers/lapack_triangular_packed_error_bounds.h
  include/asc/dense/providers/lapack_lu_band_expert.h
  include/asc/dense/providers/lapack_lu_band_refinement.h
  include/asc/dense/providers/lapack_tridiagonal.h
  include/asc/dense/providers/lapack_tridiagonal_condition.h
  include/asc/dense/providers/lapack_tridiagonal_driver.h
  include/asc/dense/providers/lapack_tridiagonal_refinement.h
  include/asc/dense/providers/lapack_cholesky_band_condition.h
  include/asc/dense/providers/lapack_cholesky_band_driver.h
  include/asc/dense/providers/lapack_cholesky_band_equilibration.h
  include/asc/dense/providers/lapack_cholesky_band_expert.h
  include/asc/dense/providers/lapack_cholesky_band_refinement.h
)

file(
  GLOB_RECURSE _observed_source_files
  LIST_DIRECTORIES FALSE
  RELATIVE "${SOURCE_DIR}"
  "${SOURCE_DIR}/src/dense/*"
)
list(REMOVE_ITEM _observed_source_files src/dense/CMakeLists.txt)
list(FILTER
  _observed_source_files
  EXCLUDE
  REGEX "^src/dense/(cuda|lapack)/"
)

list(SORT _expected_public_files)
list(SORT _expected_source_files)
list(SORT _observed_public_files)
list(SORT _observed_source_files)
if(NOT _observed_public_files STREQUAL _expected_public_files)
  message(FATAL_ERROR
    "Provider-free Dense public inventory differs from the frozen contract.\n"
    "Expected: ${_expected_public_files}\n"
    "Observed: ${_observed_public_files}"
  )
endif()
if(NOT _observed_source_files STREQUAL _expected_source_files)
  message(FATAL_ERROR
    "Provider-free Dense source inventory differs from the frozen contract.\n"
    "Expected: ${_expected_source_files}\n"
    "Observed: ${_observed_source_files}"
  )
endif()

set(_provider_pattern
  "(^|/)(cuda|cublas|cusolver|cusparse|curand|hip|rocm|sycl|mkl|blas|lapack)(/|\\.|_)"
)
set(_all_files ${_observed_public_files} ${_observed_source_files})
# These exact ASC-owned headers are provider-neutral contracts, not foreign
# SDK headers. Keep the foreign include guard active for every other path.
set(_asc_lapack_contract_headers
  asc/dense/lapack/types.h
  asc/dense/lapack/workspace.h
  asc/dense/lapack/report.h
  asc/dense/lapack/factor_view.h
  asc/dense/lapack/structured_view.h
  asc/dense/lapack/triangular_band_view.h
  asc/dense/lapack/lu.h
  asc/dense/lapack/cholesky.h
  asc/dense/lapack/qr.h
)
foreach(_relative_file IN LISTS _all_files)
  set(_path "${SOURCE_DIR}/${_relative_file}")
  file(STRINGS "${_path}" _includes REGEX "^[ \t]*#[ \t]*include")
  foreach(_include IN LISTS _includes)
    if(_include MATCHES
       "^[ \t]*#[ \t]*include[ \t]*[<\"]([^>\"]+)[>\"]")
      set(_included_path "${CMAKE_MATCH_1}")
      if(_included_path MATCHES "^asc/([^/.\"]+)")
        set(_included_module "${CMAKE_MATCH_1}")
        if(NOT _included_module STREQUAL "core"
           AND NOT _included_module STREQUAL "expression"
           AND NOT _included_module STREQUAL "dense")
          message(FATAL_ERROR
            "Forbidden module include in ${_relative_file}: "
            "${_included_path}"
          )
        endif()
      endif()
      string(TOLOWER "${_included_path}" _included_path_lower)
      if(NOT _included_path_lower STREQUAL "asc/dense/blas.h"
         AND NOT _included_path_lower IN_LIST _asc_lapack_contract_headers
         AND _included_path_lower MATCHES "${_provider_pattern}")
        message(FATAL_ERROR
          "Provider SDK include leaked into ${_relative_file}: "
          "${_included_path}"
        )
      endif()
    endif()
  endforeach()

  file(READ "${_path}" _contents)
  if(_contents MATCHES "#[ \t]*pragma[ \t]+once")
    message(FATAL_ERROR "#pragma once is forbidden: ${_relative_file}")
  endif()
  if(_contents MATCHES "asc::detail"
     OR _contents MATCHES "namespace[ \t\r\n]+detail")
    message(FATAL_ERROR
      "Forbidden detail namespace spelling in ${_relative_file}."
    )
  endif()
  if(_relative_file MATCHES "^include/"
     AND (_contents MATCHES "(^|[^A-Za-z0-9_])throw[ \t\r\n(]"
          OR _contents MATCHES "(^|[^A-Za-z0-9_])catch[ \t\r\n]*\\("))
    message(FATAL_ERROR
      "Public exceptions are forbidden: ${_relative_file}"
    )
  endif()
endforeach()
