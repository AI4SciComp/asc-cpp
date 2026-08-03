add_executable(asc_random_cuda_test random_cuda_test.cc)
target_link_libraries(asc_random_cuda_test PRIVATE ASC::random_cuda)
asc_target_enable_cxx20(TARGET asc_random_cuda_test)
if(ASC_CPP_WARNINGS_AS_ERRORS)
  asc_target_enable_warnings(TARGET asc_random_cuda_test AS_ERRORS)
else()
  asc_target_enable_warnings(TARGET asc_random_cuda_test)
endif()
if(ASC_CPP_SANITIZER_ARGUMENTS)
  asc_target_enable_sanitizers(
    TARGET asc_random_cuda_test
    ${ASC_CPP_SANITIZER_ARGUMENTS}
  )
endif()
asc_register_test(
  NAME asc_cpp.random_cuda.runtime
  TARGET asc_random_cuda_test
  LABELS "cuda;gpu;cuda-sparse-random;parity;random_cuda;runtime"
)
set_tests_properties(
  asc_cpp.random_cuda.runtime
  PROPERTIES SKIP_RETURN_CODE 77
)

add_executable(
  asc_random_cuda_benchmark
  "${PROJECT_SOURCE_DIR}/benchmarks/random_cuda/random_cuda_benchmark.cc"
)
target_link_libraries(
  asc_random_cuda_benchmark
  PRIVATE
    ASC::random_dense_cuda
    ASC::random_sparse_cuda
    CUDA::cudart
)
asc_target_enable_cxx20(TARGET asc_random_cuda_benchmark)
if(ASC_CPP_WARNINGS_AS_ERRORS)
  asc_target_enable_warnings(TARGET asc_random_cuda_benchmark AS_ERRORS)
else()
  asc_target_enable_warnings(TARGET asc_random_cuda_benchmark)
endif()
if(ASC_CPP_SANITIZER_ARGUMENTS)
  asc_target_enable_sanitizers(
    TARGET asc_random_cuda_benchmark
    ${ASC_CPP_SANITIZER_ARGUMENTS}
  )
endif()
asc_register_test(
  NAME asc_cpp.random_cuda.benchmark
  TARGET asc_random_cuda_benchmark
  LABELS "benchmark;cuda;gpu;cuda-sparse-random;performance;random_cuda"
)
set_tests_properties(
  asc_cpp.random_cuda.benchmark
  PROPERTIES
    SKIP_RETURN_CODE 77
    TIMEOUT 180
)
