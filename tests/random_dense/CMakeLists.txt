find_package(Threads REQUIRED)

function(_asc_random_storage_configure_test target)
  target_include_directories(
    "${target}"
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}"
      "${PROJECT_SOURCE_DIR}/tests/compile"
      "${PROJECT_SOURCE_DIR}/benchmarks/random_storage"
  )
  target_link_libraries(
    "${target}"
    PRIVATE
      ${ARGN}
      Threads::Threads
  )
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

add_executable(
  asc_random_dense_test
  allocation_counter.cc
  random_dense_test.cc
)
_asc_random_storage_configure_test(
  asc_random_dense_test
  ASC::random_dense
)
asc_register_test(
  NAME asc_cpp.random_dense.runtime
  TARGET asc_random_dense_test
  LABELS "random;random_dense;milestone-5;runtime"
)

add_executable(
  asc_m5_random_contract
  "${PROJECT_SOURCE_DIR}/tests/compile/m5_random_contract.cc"
)
_asc_random_storage_configure_test(
  asc_m5_random_contract
  ASC::random_dense
  ASC::random_sparse
)
asc_register_test(
  NAME asc_cpp.random_storage.compile_contract
  TARGET asc_m5_random_contract
  LABELS "compile;random;random_dense;random_sparse;milestone-5"
)

add_executable(
  asc_m5_random_multi_tu
  "${PROJECT_SOURCE_DIR}/tests/compile/m5_random_multi_tu_a.cc"
  "${PROJECT_SOURCE_DIR}/tests/compile/m5_random_multi_tu_b.cc"
  "${PROJECT_SOURCE_DIR}/tests/compile/m5_random_multi_tu_main.cc"
)
_asc_random_storage_configure_test(
  asc_m5_random_multi_tu
  ASC::random_dense
  ASC::random_sparse
)
asc_register_test(
  NAME asc_cpp.random_storage.multi_tu
  TARGET asc_m5_random_multi_tu
  LABELS "compile;random;random_dense;random_sparse;milestone-5"
)

set(_asc_m5_random_negative_sources
  m5_random_negative_const_dense.cc
  m5_random_negative_missing_dense_facet_header.cc
  m5_random_negative_missing_sparse_facet_header.cc
  m5_random_negative_result_copy.cc
  m5_random_negative_unsupported_dense_scalar.cc
  m5_random_negative_unsupported_sparse_scalar.cc
)
foreach(_source IN LISTS _asc_m5_random_negative_sources)
  get_filename_component(_name "${_source}" NAME_WE)
  add_test(
    NAME "asc_cpp.random_storage.${_name}"
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
    "asc_cpp.random_storage.${_name}"
    PROPERTIES
      LABELS "compile;random;random_dense;random_sparse;milestone-5;negative"
      TIMEOUT 30
  )
endforeach()

add_executable(
  asc_random_storage_benchmark
  "${PROJECT_SOURCE_DIR}/benchmarks/random_storage/allocation_counter.cc"
  "${PROJECT_SOURCE_DIR}/benchmarks/random_storage/random_storage_benchmark.cc"
)
_asc_random_storage_configure_test(
  asc_random_storage_benchmark
  ASC::random_dense
  ASC::random_sparse
)
asc_register_test(
  NAME asc_cpp.random_storage.benchmark
  TARGET asc_random_storage_benchmark
  LABELS "benchmark;performance;random;random_dense;random_sparse;milestone-5"
)

unset(_asc_m5_random_negative_sources)
unset(_name)
unset(_source)
