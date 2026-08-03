add_executable(asc_sparse_cuda_test sparse_cuda_test.cc)
target_link_libraries(
  asc_sparse_cuda_test
  PRIVATE
    ASC::sparse_cuda
    ASC::random_sparse_cuda
)
asc_target_enable_cxx20(TARGET asc_sparse_cuda_test)
if(ASC_CPP_WARNINGS_AS_ERRORS)
  asc_target_enable_warnings(TARGET asc_sparse_cuda_test AS_ERRORS)
else()
  asc_target_enable_warnings(TARGET asc_sparse_cuda_test)
endif()
if(ASC_CPP_SANITIZER_ARGUMENTS)
  asc_target_enable_sanitizers(
    TARGET asc_sparse_cuda_test
    ${ASC_CPP_SANITIZER_ARGUMENTS}
  )
endif()
asc_register_test(
  NAME asc_cpp.sparse_cuda.runtime
  TARGET asc_sparse_cuda_test
  LABELS "cuda;gpu;cuda-sparse-random;parity;runtime;sparse_cuda"
)
set_tests_properties(
  asc_cpp.sparse_cuda.runtime
  PROPERTIES SKIP_RETURN_CODE 77
)

add_executable(asc_sparse_cuda_standard_blas_test standard_blas_cuda_test.cc)
target_link_libraries(
  asc_sparse_cuda_standard_blas_test
  PRIVATE ASC::sparse_cuda
)
asc_target_enable_cxx20(TARGET asc_sparse_cuda_standard_blas_test)
if(ASC_CPP_WARNINGS_AS_ERRORS)
  asc_target_enable_warnings(TARGET asc_sparse_cuda_standard_blas_test AS_ERRORS)
else()
  asc_target_enable_warnings(TARGET asc_sparse_cuda_standard_blas_test)
endif()
asc_register_test(
  NAME asc_cpp.sparse_cuda.standard_blas
  TARGET asc_sparse_cuda_standard_blas_test
  LABELS "cuda;gpu;sparse-blas;parity;runtime;sparse_cuda"
)
set_tests_properties(
  asc_cpp.sparse_cuda.standard_blas
  PROPERTIES SKIP_RETURN_CODE 77
)

add_executable(
  asc_sparse_cuda_benchmark
  "${PROJECT_SOURCE_DIR}/benchmarks/sparse_cuda/sparse_cuda_benchmark.cc"
)
target_link_libraries(
  asc_sparse_cuda_benchmark
  PRIVATE
    ASC::sparse_cuda
    CUDA::cudart
    CUDA::cusparse
)
asc_target_enable_cxx20(TARGET asc_sparse_cuda_benchmark)
if(ASC_CPP_WARNINGS_AS_ERRORS)
  asc_target_enable_warnings(TARGET asc_sparse_cuda_benchmark AS_ERRORS)
else()
  asc_target_enable_warnings(TARGET asc_sparse_cuda_benchmark)
endif()
if(ASC_CPP_SANITIZER_ARGUMENTS)
  asc_target_enable_sanitizers(
    TARGET asc_sparse_cuda_benchmark
    ${ASC_CPP_SANITIZER_ARGUMENTS}
  )
endif()
asc_register_test(
  NAME asc_cpp.sparse_cuda.benchmark
  TARGET asc_sparse_cuda_benchmark
  LABELS
    "benchmark;cuda;gpu;cuda-sparse-random;sparse-blas;performance;sparse_cuda"
)
set_tests_properties(
  asc_cpp.sparse_cuda.benchmark
  PROPERTIES
    SKIP_RETURN_CODE 77
    TIMEOUT 180
)
