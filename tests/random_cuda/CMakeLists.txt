add_executable(
  asc_random_cuda_test
  random_cuda_test.cc
)
target_include_directories(
  asc_random_cuda_test
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(
  asc_random_cuda_test
  PRIVATE ASC::random_cuda
)
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
  LABELS "random;gpu;milestone-7;runtime"
)
set_tests_properties(
  asc_cpp.random_cuda.runtime
  PROPERTIES SKIP_RETURN_CODE 77
)

add_executable(
  asc_random_cuda_benchmark
  "${PROJECT_SOURCE_DIR}/benchmarks/random_cuda/random_cuda_benchmark.cc"
)
target_include_directories(
  asc_random_cuda_benchmark
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(
  asc_random_cuda_benchmark
  PRIVATE
    ASC::random_cuda
    ASC::random_dense_cuda
    ASC::random_sparse_cuda
)
asc_target_enable_cxx20(TARGET asc_random_cuda_benchmark)
if(ASC_CPP_WARNINGS_AS_ERRORS)
  asc_target_enable_warnings(TARGET asc_random_cuda_benchmark AS_ERRORS)
else()
  asc_target_enable_warnings(TARGET asc_random_cuda_benchmark)
endif()
asc_register_test(
  NAME asc_cpp.random_cuda.benchmark
  TARGET asc_random_cuda_benchmark
  LABELS "benchmark;random;gpu;milestone-7;performance"
)
set_tests_properties(
  asc_cpp.random_cuda.benchmark
  PROPERTIES SKIP_RETURN_CODE 77
)

add_executable(
  asc_m7_random_cuda_header
  "${PROJECT_SOURCE_DIR}/tests/compile/m7_random_cuda_header.cc"
)
target_link_libraries(
  asc_m7_random_cuda_header
  PRIVATE ASC::random_cuda
)
asc_target_enable_cxx20(TARGET asc_m7_random_cuda_header)
if(ASC_CPP_WARNINGS_AS_ERRORS)
  asc_target_enable_warnings(TARGET asc_m7_random_cuda_header AS_ERRORS)
else()
  asc_target_enable_warnings(TARGET asc_m7_random_cuda_header)
endif()
asc_register_test(
  NAME asc_cpp.random_cuda.header
  TARGET asc_m7_random_cuda_header
  LABELS "compile;random;gpu;milestone-7"
)

add_executable(
  asc_m7_cuda_contracts
  "${PROJECT_SOURCE_DIR}/tests/compile/m7_cuda_contracts.cc"
)
target_link_libraries(
  asc_m7_cuda_contracts
  PRIVATE
    ASC::sparse_cuda
    ASC::random_cuda
    ASC::random_dense_cuda
    ASC::random_sparse_cuda
)
asc_target_enable_cxx20(TARGET asc_m7_cuda_contracts)
if(ASC_CPP_WARNINGS_AS_ERRORS)
  asc_target_enable_warnings(TARGET asc_m7_cuda_contracts AS_ERRORS)
else()
  asc_target_enable_warnings(TARGET asc_m7_cuda_contracts)
endif()
asc_register_test(
  NAME asc_cpp.random_cuda.compile_contracts
  TARGET asc_m7_cuda_contracts
  LABELS "compile;gpu;milestone-7"
)

set(_asc_m7_negative_sources
  m7_cuda_negative_copy.cc
  m7_negative_dense_const_destination.cc
  m7_negative_dense_integral.cc
  m7_negative_random_result_copy.cc
  m7_negative_sparse_integral.cc
  m7_negative_sparse_random_result_copy.cc
)
foreach(_source IN LISTS _asc_m7_negative_sources)
  get_filename_component(_name "${_source}" NAME_WE)
  add_test(
    NAME "asc_cpp.random_cuda.${_name}"
    COMMAND
      "${CMAKE_COMMAND}"
      "-DCXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}"
      "-DCXX_COMPILER_ID:STRING=${CMAKE_CXX_COMPILER_ID}"
      "-DCXX_COMPILER_FRONTEND_VARIANT:STRING=${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}"
      "-DINCLUDE_DIR:PATH=${PROJECT_SOURCE_DIR}/include"
      "-DSOURCE:FILEPATH=${PROJECT_SOURCE_DIR}/tests/compile/${_source}"
      "-DOUTPUT_DIR:PATH=${CMAKE_CURRENT_BINARY_DIR}/negative"
      "-DTEST_NAME:STRING=${_name}"
      -P "${PROJECT_SOURCE_DIR}/tests/expression/expect_compile_failure.cmake"
  )
  set_tests_properties(
    "asc_cpp.random_cuda.${_name}"
    PROPERTIES
      LABELS "compile;gpu;milestone-7;negative"
      TIMEOUT 30
  )
endforeach()

unset(_asc_m7_negative_sources)
unset(_name)
unset(_source)
