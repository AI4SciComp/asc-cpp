cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SOURCE_DIR OR SOURCE_DIR STREQUAL "")
  message(FATAL_ERROR "SOURCE_DIR is required.")
endif()

set(_expected_public_files
  include/asc/core.h
  include/asc/core/array_format.h
  include/asc/core/array_io.h
  include/asc/core/configuration.h
  include/asc/core/contracts.h
  include/asc/core/execution.h
  include/asc/core/export.h
  include/asc/core/extents.h
  include/asc/core/io.h
  include/asc/core/matrix_market.h
  include/asc/core/memory.h
  include/asc/core/providers/cuda.h
  include/asc/core/providers/cuda_export.h
  include/asc/core/result.h
  include/asc/core/status.h
  include/asc/core/types.h
  include/asc/dense.h
  include/asc/dense/array.h
  include/asc/dense/evaluate.h
  include/asc/dense/export.h
  include/asc/dense/layout.h
  include/asc/dense/blas.h
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
  include/asc/dense/providers/lapack_positive_tridiagonal_expert.h
  include/asc/dense/providers/lapack_cholesky_packed_robust.h
  include/asc/dense/providers/lapack_qr.h
  include/asc/dense/providers/lapack_cholesky_condition.h
  include/asc/dense/providers/lapack_cholesky_driver.h
  include/asc/dense/providers/lapack_cholesky_equilibration.h
  include/asc/dense/providers/lapack_cholesky_refinement.h
  include/asc/dense/providers/lapack_least_squares.h
  include/asc/dense/providers/lapack_cholesky_band.h
  include/asc/dense/providers/lapack_indefinite.h
  include/asc/dense/providers/lapack_indefinite_rook.h
  include/asc/dense/providers/lapack_indefinite_rook_condition.h
  include/asc/dense/providers/lapack_indefinite_rook_driver.h
  include/asc/dense/providers/lapack_indefinite_rook_inverse.h
  include/asc/dense/providers/lapack_indefinite_inverse.h
  include/asc/dense/providers/lapack_indefinite_block_inverse.h
  include/asc/dense/providers/lapack_indefinite_block_solve.h
  include/asc/dense/providers/lapack_indefinite_rk.h
  include/asc/dense/providers/lapack_indefinite_rk_solve.h
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
  include/asc/expression.h
  include/asc/expression/expression.h
  include/asc/expression/writable.h
  include/asc/random.h
  include/asc/random/distribution.h
  include/asc/random/dense.h
  include/asc/random/engine.h
  include/asc/random/export.h
  include/asc/random/generator.h
  include/asc/random/quasi.h
  include/asc/random/providers/cuda.h
  include/asc/random/providers/cuda_export.h
  include/asc/random/providers/dense_cuda.h
  include/asc/random/providers/dense_cuda_export.h
  include/asc/random/providers/sparse_cuda.h
  include/asc/random/providers/sparse_cuda_export.h
  include/asc/random/seed.h
  include/asc/random/sparse.h
  include/asc/sparse.h
  include/asc/sparse/compressed.h
  include/asc/sparse/coordinate.h
  include/asc/sparse/evaluate.h
  include/asc/sparse/export.h
  include/asc/sparse/blas.h
  include/asc/sparse/print.h
  include/asc/sparse/io.h
  include/asc/sparse/matrix_market.h
  include/asc/sparse/providers/cuda.h
  include/asc/sparse/providers/cuda_export.h
  include/asc/utilities.h
  include/asc/utilities/command_line.h
  include/asc/utilities/export.h
  include/asc/utilities/timer.h
)
set(_expected_compiled_sources
  src/core/array_format.cc
  src/core/array_io.cc
  src/core/array_parse.cc
  src/core/configuration.cc
  src/core/contracts.cc
  src/core/execution.cc
  src/core/io.cc
  src/core/matrix_market.cc
  src/core/memory.cc
  src/core/status.cc
  src/core/cuda/runtime.cc
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
  src/dense/lapack/reference_lu.cc
  src/dense/lapack/reference_lu_expert.cc
  src/dense/lapack/reference_lu_equilibration.cc
  src/dense/lapack/reference_lu_condition.cc
  src/dense/lapack/reference_lu_refinement.cc
  src/dense/lapack/reference_lu_driver.cc
  src/dense/lapack/reference_lu_helpers.cc
  src/dense/lapack/reference_cholesky.cc
  src/dense/lapack/reference_cholesky_packed.cc
  src/dense/lapack/reference_cholesky_packed_solve.cc
  src/dense/lapack/reference_cholesky_packed_inverse.cc
  src/dense/lapack/reference_cholesky_packed_driver.cc
  src/dense/lapack/reference_cholesky_packed_equilibration.cc
  src/dense/lapack/reference_cholesky_packed_condition.cc
  src/dense/lapack/reference_cholesky_packed_refinement.cc
  src/dense/lapack/robust_cholesky_packed_expert.cc
  src/dense/lapack/reference_mixed_general.cc
  src/dense/lapack/reference_mixed_positive.cc
  src/dense/lapack/reference_precision_conversion.cc
  src/dense/lapack/reference_matrix_copy.cc
  src/dense/lapack/reference_matrix_scale.cc
  src/dense/lapack/reference_matrix_set.cc
  src/dense/lapack/reference_dmd.cc
  src/dense/lapack/reference_dmd_qr.cc
  src/dense/lapack/reference_positive_tridiagonal.cc
  src/dense/lapack/reference_positive_tridiagonal_condition.cc
  src/dense/lapack/reference_positive_tridiagonal_refinement.cc
  src/dense/lapack/reference_positive_tridiagonal_driver.cc
  src/dense/lapack/reference_positive_tridiagonal_expert.cc
  src/dense/lapack/reference_qr.cc
  src/dense/lapack/reference_cholesky_condition.cc
  src/dense/lapack/reference_cholesky_driver.cc
  src/dense/lapack/reference_cholesky_equilibration.cc
  src/dense/lapack/reference_cholesky_refinement.cc
  src/dense/lapack/reference_least_squares.cc
  src/dense/lapack/reference_cholesky_band.cc
  src/dense/lapack/reference_indefinite.cc
  src/dense/lapack/reference_indefinite_rook.cc
  src/dense/lapack/reference_indefinite_rook_condition.cc
  src/dense/lapack/reference_indefinite_rook_driver.cc
  src/dense/lapack/reference_indefinite_rook_inverse.cc
  src/dense/lapack/reference_indefinite_inverse.cc
  src/dense/lapack/reference_indefinite_block_inverse.cc
  src/dense/lapack/reference_indefinite_block_solve.cc
  src/dense/lapack/reference_indefinite_rk.cc
  src/dense/lapack/reference_indefinite_rk_solve.cc
  src/dense/lapack/reference_rank_revealing.cc
  src/dense/lapack/reference_sylvester.cc
  src/dense/lapack/reference_indefinite_condition.cc
  src/dense/lapack/reference_indefinite_driver.cc
  src/dense/lapack/reference_indefinite_expert_driver.cc
  src/dense/lapack/reference_indefinite_refinement.cc
  src/dense/lapack/reference_svd_least_squares.cc
  src/dense/lapack/reference_lu_band.cc
  src/dense/lapack/reference_lu_band_condition.cc
  src/dense/lapack/reference_lu_band_driver.cc
  src/dense/lapack/reference_lu_band_equilibration.cc
  src/dense/lapack/reference_lu_band_equilibration_radix.cc
  src/dense/lapack/reference_triangular.cc
  src/dense/lapack/reference_triangular_expert.cc
  src/dense/lapack/reference_packed_triangular.cc
  src/dense/lapack/reference_packed_triangular_expert.cc
  src/dense/lapack/reference_triangular_band.cc
  src/dense/lapack/reference_triangular_band_expert.cc
  src/dense/lapack/reference_lu_band_refinement.cc
  src/dense/lapack/reference_lu_band_solve.cc
  src/dense/lapack/reference_tridiagonal.cc
  src/dense/lapack/reference_tridiagonal_condition.cc
  src/dense/lapack/reference_tridiagonal_driver.cc
  src/dense/lapack/reference_tridiagonal_expert_driver.cc
  src/dense/lapack/reference_tridiagonal_refinement.cc
  src/dense/lapack/reference_cholesky_band_condition.cc
  src/dense/lapack/reference_cholesky_band_driver.cc
  src/dense/lapack/reference_cholesky_band_equilibration.cc
  src/dense/lapack/reference_cholesky_band_expert.cc
  src/dense/lapack/reference_cholesky_band_refinement.cc
  src/dense/cuda/blas_level1.cc
  src/dense/cuda/blas_level1_kernels.cu
  src/dense/cuda/blas_level2.cc
  src/dense/cuda/blas_level3.cc
  src/dense/cuda/blas_level2_kernels.cu
  src/dense/cuda/context.cc
  src/dense/cuda/kernels.cu
  src/dense/cuda/operations.cc
  src/random/distribution.cc
  src/random/engine.cc
  src/random/quasi.cc
  src/random/seed.cc
  src/random/sobol_table_generated.cc
  src/random/cuda/dense.cc
  src/random/cuda/dense_kernels.cu
  src/random/cuda/raw.cc
  src/random/cuda/raw_kernels.cu
  src/random/cuda/sparse.cc
  src/random/cuda/sparse_kernels.cu
  src/sparse/reference_blas.cc
  src/sparse/array_io.cc
  src/sparse/matrix_market.cc
  src/sparse/standard_blas.cc
  src/sparse/cuda/context.cc
  src/sparse/cuda/kernels.cu
  src/sparse/cuda/operations.cc
  src/utilities/command_line.cc
  src/utilities/timer.cc
)

file(
  GLOB_RECURSE _public_files
  LIST_DIRECTORIES FALSE
  RELATIVE "${SOURCE_DIR}"
  "${SOURCE_DIR}/include/asc/*"
)
file(
  GLOB_RECURSE _compiled_sources
  LIST_DIRECTORIES FALSE
  RELATIVE "${SOURCE_DIR}"
  "${SOURCE_DIR}/src/*.cc"
  "${SOURCE_DIR}/src/*.cu"
  "${SOURCE_DIR}/src/*.cpp"
  "${SOURCE_DIR}/src/*.cxx"
)
list(SORT _public_files)
list(SORT _compiled_sources)
list(SORT _expected_public_files)
list(SORT _expected_compiled_sources)
if(NOT _public_files STREQUAL _expected_public_files)
  message(FATAL_ERROR
    "release hardening public-file set differs from the frozen contract.\n"
    "Expected: ${_expected_public_files}\n"
    "Actual:   ${_public_files}"
  )
endif()
if(NOT _compiled_sources STREQUAL _expected_compiled_sources)
  message(FATAL_ERROR
    "Approved compiled-source set differs from the frozen contract.\n"
    "Expected: ${_expected_compiled_sources}\n"
    "Actual:   ${_compiled_sources}"
  )
endif()

foreach(_file IN LISTS _public_files)
  if(NOT _file MATCHES "\\.h$")
    message(FATAL_ERROR "Public headers must end in .h: ${_file}")
  endif()
  if(_file MATCHES "(-inl|_impl)\\.h$")
    message(FATAL_ERROR "Public implementation header is forbidden: ${_file}")
  endif()
  file(READ "${SOURCE_DIR}/${_file}" _contents)
  if(_contents MATCHES "#[ \t]*pragma[ \t]+once")
    message(FATAL_ERROR "#pragma once is prohibited: ${_file}")
  endif()
  if(_contents MATCHES "asc::detail|namespace[ \t]+detail")
    message(FATAL_ERROR "Forbidden detail namespace spelling: ${_file}")
  endif()
  if(_contents MATCHES
     "#[ \t]*include[ \t]*[<\"](cuda|hip|sycl|CL|mkl|cublas|cusparse|rocblas|rocsparse|lapack[a-z0-9_]*)[/\\.>]")
    message(FATAL_ERROR "Provider SDK include leaked into ${_file}.")
  endif()
endforeach()

set(_all_production_files ${_public_files} ${_compiled_sources})
foreach(_file IN LISTS _all_production_files)
  file(READ "${SOURCE_DIR}/${_file}" _contents)
  if(_file MATCHES "^(include/asc/core|src/core)")
    set(_forbidden_modules "utilities|expression|dense|sparse|random|array|linalg")
  elseif(_file MATCHES "^(include/asc/utilities|src/utilities)")
    set(_forbidden_modules "expression|dense|sparse|random|array|linalg")
  elseif(_file MATCHES "^include/asc/expression")
    set(_forbidden_modules "utilities|dense|sparse|random|array|linalg")
  elseif(_file MATCHES "^(include/asc/dense|src/dense)")
    set(_forbidden_modules "utilities|sparse|random|array|linalg")
  elseif(_file MATCHES "^(include/asc/sparse|src/sparse)")
    set(_forbidden_modules "utilities|dense|random|array|linalg")
  elseif(_file STREQUAL "include/asc/random/dense.h"
         OR _file MATCHES
            "^(include/asc/random/providers/dense_cuda|src/random/cuda/dense)")
    set(_forbidden_modules "utilities|sparse|array|linalg")
  elseif(_file STREQUAL "include/asc/random/sparse.h"
         OR _file MATCHES
            "^(include/asc/random/providers/sparse_cuda|src/random/cuda/sparse)")
    set(_forbidden_modules "utilities|dense|array|linalg")
  elseif(_file MATCHES "^(include/asc/random|src/random)")
    set(_forbidden_modules "utilities|expression|dense|sparse|array|linalg")
  else()
    continue()
  endif()
  if(_contents MATCHES
     "#[ \t]*include[ \t]*[<\"]asc/(${_forbidden_modules})(/|\\.h)")
    message(FATAL_ERROR "Forbidden module include in ${_file}.")
  endif()
endforeach()

set(_retired_paths
  include/asc/array.h
  include/asc/array
  include/asc/linalg.h
  include/asc/linalg
  include/asc/dense/linalg.h
  include/asc/sparse/linalg.h
  include/asc/cpp.h
  src/array
  src/linalg
  src/dense/linalg.cc
  src/sparse/reference_linalg.cc
)
foreach(_path IN LISTS _retired_paths)
  if(EXISTS "${SOURCE_DIR}/${_path}")
    message(FATAL_ERROR "Retired production path was restored: ${_path}")
  endif()
endforeach()
