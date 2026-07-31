function(_asc_dense_cuda_configure_test target)
  target_link_libraries("${target}" PRIVATE ASC::dense_cuda)
  target_include_directories("${target}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  if(ASC_CPP_INTERNAL_ENABLE_CUDA_RUNTIME_TEST_HOOKS)
    target_compile_definitions(
      "${target}"
      PRIVATE ASC_CPP_CUDA_RUNTIME_TEST_HOOKS
    )
  endif()
  asc_target_enable_cxx20(TARGET "${target}")
  if(ASC_CPP_WARNINGS_AS_ERRORS)
    asc_target_enable_warnings(TARGET "${target}" AS_ERRORS)
  else()
    asc_target_enable_warnings(TARGET "${target}")
  endif()
  if(ASC_CPP_SANITIZER_ARGUMENTS)
    asc_target_enable_sanitizers(
      TARGET "${target}"
      ${ASC_CPP_SANITIZER_ARGUMENTS}
    )
  endif()
endfunction()

foreach(_source IN ITEMS
    dense_cuda_concurrency_test.cc
    dense_cuda_evaluate_test.cc
    dense_cuda_blas_test.cc
    dense_cuda_blas_level1_test.cc
    dense_cuda_blas_level2_test.cc
    dense_cuda_move_assignment_test.cc
    dense_cuda_owner_test.cc
    dense_cuda_scalar_release_regression_test.cc
)
  get_filename_component(_name "${_source}" NAME_WE)
  set(_target "asc_dense_cuda_${_name}")
  add_executable("${_target}" "${_source}")
  _asc_dense_cuda_configure_test("${_target}")
  asc_register_test(
    NAME "asc_cpp.dense_cuda.${_name}"
    TARGET "${_target}"
    LABELS "dense_cuda;cuda;gpu;milestone-6;runtime;parity"
  )
  set_tests_properties(
    "asc_cpp.dense_cuda.${_name}"
    PROPERTIES SKIP_RETURN_CODE 77
  )
  add_test(
    NAME "asc_cpp.dense_cuda.${_name}.forced_no_device"
    COMMAND "$<TARGET_FILE:${_target}>"
  )
  set_tests_properties(
    "asc_cpp.dense_cuda.${_name}.forced_no_device"
    PROPERTIES
      ENVIRONMENT "ASC_CPP_TEST_FORCE_NO_CUDA_DEVICE=1"
      LABELS "dense_cuda;cuda;gpu;milestone-8;no-device"
      SKIP_RETURN_CODE 77
  )
endforeach()

set_property(
  TEST
    asc_cpp.dense_cuda.dense_cuda_blas_level2_test
    asc_cpp.dense_cuda.dense_cuda_blas_level2_test.forced_no_device
  APPEND PROPERTY LABELS "blas-level2;milestone-8"
)

target_link_libraries(
  asc_dense_cuda_dense_cuda_concurrency_test
  PRIVATE Threads::Threads
)

add_executable(
  asc_dense_cuda_benchmark
  "${PROJECT_SOURCE_DIR}/benchmarks/dense_cuda/benchmark.cc"
)
_asc_dense_cuda_configure_test(asc_dense_cuda_benchmark)
target_link_libraries(
  asc_dense_cuda_benchmark
  PRIVATE CUDA::cudart CUDA::cublas
)
asc_register_test(
  NAME asc_cpp.dense_cuda.benchmark
  TARGET asc_dense_cuda_benchmark
  LABELS "benchmark;cuda;dense_cuda;gpu;milestone-6;performance"
)
set_tests_properties(
  asc_cpp.dense_cuda.benchmark
  PROPERTIES
    SKIP_RETURN_CODE 77
    TIMEOUT 120
)
add_test(
  NAME asc_cpp.dense_cuda.benchmark.forced_no_device
  COMMAND "$<TARGET_FILE:asc_dense_cuda_benchmark>"
)
set_tests_properties(
  asc_cpp.dense_cuda.benchmark.forced_no_device
  PROPERTIES
    ENVIRONMENT "ASC_CPP_TEST_FORCE_NO_CUDA_DEVICE=1"
    LABELS "benchmark;dense_cuda;cuda;gpu;milestone-8;no-device"
    SKIP_RETURN_CODE 77
    TIMEOUT 120
)

add_test(
  NAME asc_cpp.dense_cuda.benchmark.forced_enumeration_failure
  COMMAND
    "${CMAKE_COMMAND}"
    "-DPROGRAM:FILEPATH=$<TARGET_FILE:asc_dense_cuda_benchmark>"
    "-DEXPECTED_EXIT:STRING=2"
    "-DENVIRONMENT:STRING=ASC_CPP_TEST_FORCE_CUDA_ENUMERATION_FAILURE=1"
    -P "${PROJECT_SOURCE_DIR}/tests/cmake/ExpectExit.cmake"
)
set_tests_properties(
  asc_cpp.dense_cuda.benchmark.forced_enumeration_failure
  PROPERTIES
    LABELS "benchmark;dense_cuda;cuda;gpu;milestone-8;provider-failure"
    TIMEOUT 120
)

unset(_name)
unset(_source)
unset(_target)
