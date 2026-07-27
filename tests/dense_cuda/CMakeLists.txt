find_package(Threads REQUIRED)

function(_asc_dense_cuda_configure_test target)
  target_include_directories(
    "${target}"
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}"
      "${PROJECT_SOURCE_DIR}/src"
      "${PROJECT_SOURCE_DIR}/tests/compile"
  )
  target_link_libraries("${target}" PRIVATE ASC::dense_cuda)
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

foreach(_name IN ITEMS storage_evaluate linalg)
  set(_target "asc_dense_cuda_${_name}_test")
  add_executable("${_target}" "dense_cuda_${_name}_test.cc")
  _asc_dense_cuda_configure_test("${_target}")
  asc_register_test(
    NAME "asc_cpp.dense_cuda.${_name}"
    TARGET "${_target}"
    LABELS "dense_cuda;gpu;milestone-6;numerical;runtime"
  )
endforeach()

add_executable(
  asc_dense_cuda_concurrency_test
  dense_cuda_concurrency_test.cc
)
_asc_dense_cuda_configure_test(asc_dense_cuda_concurrency_test)
target_link_libraries(
  asc_dense_cuda_concurrency_test
  PRIVATE
    CUDA::cudart
    Threads::Threads
)
asc_register_test(
  NAME asc_cpp.dense_cuda.concurrency
  TARGET asc_dense_cuda_concurrency_test
  LABELS "concurrency;dense_cuda;gpu;milestone-6;runtime"
)

add_executable(
  asc_m6_provider_contract
  "${PROJECT_SOURCE_DIR}/tests/compile/m6_provider_contract.cc"
)
_asc_dense_cuda_configure_test(asc_m6_provider_contract)
asc_register_test(
  NAME asc_cpp.dense_cuda.compile_contract
  TARGET asc_m6_provider_contract
  LABELS "compile;dense_cuda;gpu;milestone-6"
)

add_executable(
  asc_m6_provider_multi_tu
  "${PROJECT_SOURCE_DIR}/tests/compile/m6_provider_multi_tu_a.cc"
  "${PROJECT_SOURCE_DIR}/tests/compile/m6_provider_multi_tu_b.cc"
  "${PROJECT_SOURCE_DIR}/tests/compile/m6_provider_multi_tu_main.cc"
)
_asc_dense_cuda_configure_test(asc_m6_provider_multi_tu)
asc_register_test(
  NAME asc_cpp.dense_cuda.multi_tu
  TARGET asc_m6_provider_multi_tu
  LABELS "compile;dense_cuda;gpu;milestone-6"
)

set(_asc_m6_negative_sources
  m6_negative_resource_copy.cc
  m6_negative_resource_move.cc
  m6_negative_dense_context_copy.cc
  m6_negative_array_copy.cc
  m6_negative_unsupported_element.cc
  m6_negative_event_copy.cc
)
foreach(_source IN LISTS _asc_m6_negative_sources)
  get_filename_component(_name "${_source}" NAME_WE)
  add_test(
    NAME "asc_cpp.dense_cuda.${_name}"
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
    "asc_cpp.dense_cuda.${_name}"
    PROPERTIES
      LABELS "compile;gpu;milestone-6;negative"
      TIMEOUT 30
  )
endforeach()

add_executable(
  asc_dense_cuda_benchmark
  "${PROJECT_SOURCE_DIR}/benchmarks/dense_cuda/dense_cuda_benchmark.cc"
)
_asc_dense_cuda_configure_test(asc_dense_cuda_benchmark)
asc_register_test(
  NAME asc_cpp.dense_cuda.benchmark
  TARGET asc_dense_cuda_benchmark
  LABELS "benchmark;gpu;milestone-6;performance"
)

unset(_asc_m6_negative_sources)
