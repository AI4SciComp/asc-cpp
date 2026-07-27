add_executable(
  asc_random_sparse_cuda_test
  random_sparse_cuda_test.cc
)
target_include_directories(
  asc_random_sparse_cuda_test
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/tests/random_cuda"
)
target_link_libraries(
  asc_random_sparse_cuda_test
  PRIVATE ASC::random_sparse_cuda
)
asc_target_enable_cxx20(TARGET asc_random_sparse_cuda_test)
if(ASC_CPP_WARNINGS_AS_ERRORS)
  asc_target_enable_warnings(TARGET asc_random_sparse_cuda_test AS_ERRORS)
else()
  asc_target_enable_warnings(TARGET asc_random_sparse_cuda_test)
endif()
if(ASC_CPP_SANITIZER_ARGUMENTS)
  asc_target_enable_sanitizers(
    TARGET asc_random_sparse_cuda_test
    ${ASC_CPP_SANITIZER_ARGUMENTS}
  )
endif()
asc_register_test(
  NAME asc_cpp.random_sparse_cuda.runtime
  TARGET asc_random_sparse_cuda_test
  LABELS "random;sparse;gpu;milestone-7;runtime"
)
set_tests_properties(
  asc_cpp.random_sparse_cuda.runtime
  PROPERTIES SKIP_RETURN_CODE 77
)

add_executable(
  asc_m7_random_sparse_cuda_header
  "${PROJECT_SOURCE_DIR}/tests/compile/m7_random_sparse_cuda_header.cc"
)
target_link_libraries(
  asc_m7_random_sparse_cuda_header
  PRIVATE ASC::random_sparse_cuda
)
asc_target_enable_cxx20(TARGET asc_m7_random_sparse_cuda_header)
if(ASC_CPP_WARNINGS_AS_ERRORS)
  asc_target_enable_warnings(
    TARGET asc_m7_random_sparse_cuda_header
    AS_ERRORS
  )
else()
  asc_target_enable_warnings(TARGET asc_m7_random_sparse_cuda_header)
endif()
asc_register_test(
  NAME asc_cpp.random_sparse_cuda.header
  TARGET asc_m7_random_sparse_cuda_header
  LABELS "compile;random;sparse;gpu;milestone-7"
)
