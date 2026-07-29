add_executable(
  asc_random_dense_cuda_test
  random_dense_cuda_test.cc
)
target_link_libraries(
  asc_random_dense_cuda_test
  PRIVATE ASC::random_dense_cuda
)
asc_target_enable_cxx20(TARGET asc_random_dense_cuda_test)
if(ASC_CPP_WARNINGS_AS_ERRORS)
  asc_target_enable_warnings(TARGET asc_random_dense_cuda_test AS_ERRORS)
else()
  asc_target_enable_warnings(TARGET asc_random_dense_cuda_test)
endif()
if(ASC_CPP_SANITIZER_ARGUMENTS)
  asc_target_enable_sanitizers(
    TARGET asc_random_dense_cuda_test
    ${ASC_CPP_SANITIZER_ARGUMENTS}
  )
endif()
asc_register_test(
  NAME asc_cpp.random_dense_cuda.runtime
  TARGET asc_random_dense_cuda_test
  LABELS "cuda;gpu;milestone-7;parity;random_dense_cuda;runtime"
)
set_tests_properties(
  asc_cpp.random_dense_cuda.runtime
  PROPERTIES SKIP_RETURN_CODE 77
)
