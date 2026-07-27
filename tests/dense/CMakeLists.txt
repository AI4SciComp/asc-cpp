function(_asc_dense_configure_test target)
  target_include_directories(
    "${target}"
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}"
      "${PROJECT_SOURCE_DIR}/tests/compile"
  )
  target_link_libraries("${target}" PRIVATE ASC::dense)
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

add_executable(asc_dense_layout_view_test layout_view_test.cc)
_asc_dense_configure_test(asc_dense_layout_view_test)
asc_register_test(
  NAME asc_cpp.dense.layout_view
  TARGET asc_dense_layout_view_test
  LABELS "dense;milestone-3;runtime"
)

add_executable(asc_dense_array_test array_test.cc)
_asc_dense_configure_test(asc_dense_array_test)
asc_register_test(
  NAME asc_cpp.dense.array
  TARGET asc_dense_array_test
  LABELS "dense;milestone-3;runtime"
)

add_executable(
  asc_dense_evaluate_test
  allocation_counter.cc
  evaluate_test.cc
)
_asc_dense_configure_test(asc_dense_evaluate_test)
asc_register_test(
  NAME asc_cpp.dense.evaluate
  TARGET asc_dense_evaluate_test
  LABELS "dense;milestone-3;runtime"
)

add_executable(asc_dense_linalg_test linalg_test.cc)
_asc_dense_configure_test(asc_dense_linalg_test)
asc_register_test(
  NAME asc_cpp.dense.linalg
  TARGET asc_dense_linalg_test
  LABELS "dense;milestone-3;numerical;runtime"
)

add_executable(
  asc_m3_dense_contract
  "${PROJECT_SOURCE_DIR}/tests/compile/m3_dense_contract.cc"
)
_asc_dense_configure_test(asc_m3_dense_contract)
asc_register_test(
  NAME asc_cpp.dense.compile_contract
  TARGET asc_m3_dense_contract
  LABELS "compile;dense;milestone-3"
)

add_executable(
  asc_m3_dense_multi_tu
  "${PROJECT_SOURCE_DIR}/tests/compile/m3_dense_multi_tu_a.cc"
  "${PROJECT_SOURCE_DIR}/tests/compile/m3_dense_multi_tu_b.cc"
  "${PROJECT_SOURCE_DIR}/tests/compile/m3_dense_multi_tu_main.cc"
)
_asc_dense_configure_test(asc_m3_dense_multi_tu)
asc_register_test(
  NAME asc_cpp.dense.multi_tu
  TARGET asc_m3_dense_multi_tu
  LABELS "compile;dense;milestone-3"
)

set(_asc_dense_negative_sources
  m3_dense_negative_const_mutation.cc
  m3_dense_negative_owner_copy.cc
  m3_dense_negative_unsupported_element.cc
)
foreach(_source IN LISTS _asc_dense_negative_sources)
  get_filename_component(_name "${_source}" NAME_WE)
  add_test(
    NAME "asc_cpp.dense.${_name}"
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
    "asc_cpp.dense.${_name}"
    PROPERTIES
      LABELS "compile;dense;milestone-3;negative"
      TIMEOUT 30
  )
endforeach()

add_executable(
  asc_dense_allocation_free_benchmark
  allocation_counter.cc
  "${PROJECT_SOURCE_DIR}/benchmarks/dense/allocation_free_benchmark.cc"
)
_asc_dense_configure_test(asc_dense_allocation_free_benchmark)
asc_register_test(
  NAME asc_cpp.dense.allocation_free_benchmark
  TARGET asc_dense_allocation_free_benchmark
  LABELS "benchmark;dense;milestone-3;performance"
)

unset(_asc_dense_negative_sources)
