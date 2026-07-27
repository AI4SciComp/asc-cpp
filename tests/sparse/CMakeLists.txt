function(_asc_sparse_configure_test target)
  target_include_directories(
    "${target}"
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}"
      "${PROJECT_SOURCE_DIR}/tests/compile"
  )
  target_link_libraries("${target}" PRIVATE ASC::sparse)
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

foreach(_test_name IN ITEMS
    coordinate
    compressed
    conversion
    evaluate
    linalg
)
  set(_target "asc_sparse_${_test_name}_test")
  add_executable(
    "${_target}"
    allocation_counter.cc
    "${_test_name}_test.cc"
  )
  _asc_sparse_configure_test("${_target}")
  if(_test_name STREQUAL "linalg")
    set(_labels "sparse;milestone-4;numerical;runtime")
  else()
    set(_labels "sparse;milestone-4;runtime")
  endif()
  asc_register_test(
    NAME "asc_cpp.sparse.${_test_name}"
    TARGET "${_target}"
    LABELS "${_labels}"
  )
endforeach()

add_executable(
  asc_m4_sparse_contract
  "${PROJECT_SOURCE_DIR}/tests/compile/m4_sparse_contract.cc"
)
_asc_sparse_configure_test(asc_m4_sparse_contract)
asc_register_test(
  NAME asc_cpp.sparse.compile_contract
  TARGET asc_m4_sparse_contract
  LABELS "compile;sparse;milestone-4"
)

add_executable(
  asc_m4_sparse_multi_tu
  "${PROJECT_SOURCE_DIR}/tests/compile/m4_sparse_multi_tu_a.cc"
  "${PROJECT_SOURCE_DIR}/tests/compile/m4_sparse_multi_tu_b.cc"
  "${PROJECT_SOURCE_DIR}/tests/compile/m4_sparse_multi_tu_main.cc"
)
_asc_sparse_configure_test(asc_m4_sparse_multi_tu)
asc_register_test(
  NAME asc_cpp.sparse.multi_tu
  TARGET asc_m4_sparse_multi_tu
  LABELS "compile;sparse;milestone-4"
)

set(_asc_sparse_negative_sources
  m4_sparse_negative_const_mutation.cc
  m4_sparse_negative_invalid_extents.cc
  m4_sparse_negative_missing_writable.cc
  m4_sparse_negative_owner_copy.cc
  m4_sparse_negative_spmv_integral.cc
  m4_sparse_negative_unsupported_element.cc
)
foreach(_source IN LISTS _asc_sparse_negative_sources)
  get_filename_component(_name "${_source}" NAME_WE)
  add_test(
    NAME "asc_cpp.sparse.${_name}"
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
    "asc_cpp.sparse.${_name}"
    PROPERTIES
      LABELS "compile;sparse;milestone-4;negative"
      TIMEOUT 30
  )
endforeach()

add_executable(
  asc_sparse_allocation_free_benchmark
  allocation_counter.cc
  "${PROJECT_SOURCE_DIR}/benchmarks/sparse/allocation_free_benchmark.cc"
)
_asc_sparse_configure_test(asc_sparse_allocation_free_benchmark)
asc_register_test(
  NAME asc_cpp.sparse.allocation_free_benchmark
  TARGET asc_sparse_allocation_free_benchmark
  LABELS "benchmark;sparse;milestone-4;performance"
)

unset(_asc_sparse_negative_sources)
unset(_labels)
unset(_target)
unset(_test_name)
