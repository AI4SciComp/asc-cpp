function(_asc_sparse_configure_test target)
  target_link_libraries("${target}" PRIVATE ASC::sparse)
  target_include_directories("${target}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
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
    coordinate_test.cc
    compressed_conversion_test.cc
)
  get_filename_component(_name "${_source}" NAME_WE)
  set(_target "asc_sparse_${_name}")
  add_executable("${_target}" "${_source}")
  _asc_sparse_configure_test("${_target}")
  asc_register_test(
    NAME "asc_cpp.sparse.${_name}"
    TARGET "${_target}"
    LABELS "sparse;runtime"
  )
endforeach()

foreach(_source IN ITEMS
    evaluate_test.cc
    blas_test.cc
    standard_blas_test.cc
    print_test.cc
)
  get_filename_component(_name "${_source}" NAME_WE)
  set(_target "asc_sparse_${_name}")
  add_executable("${_target}" "${_source}" allocation_probe.cc)
  _asc_sparse_configure_test("${_target}")
  asc_register_test(
    NAME "asc_cpp.sparse.${_name}"
    TARGET "${_target}"
    LABELS "sparse;runtime"
  )
endforeach()

set_property(
  TEST asc_cpp.sparse.standard_blas_test
  APPEND PROPERTY LABELS sparse-blas
)

add_executable(
  asc_sparse_multi_tu
  "${PROJECT_SOURCE_DIR}/tests/compile/m4_multi_tu_a.cc"
  "${PROJECT_SOURCE_DIR}/tests/compile/m4_multi_tu_b.cc"
  "${PROJECT_SOURCE_DIR}/tests/compile/m4_multi_tu_main.cc"
)
target_include_directories(
  asc_sparse_multi_tu
  PRIVATE "${PROJECT_SOURCE_DIR}/tests/compile"
)
_asc_sparse_configure_test(asc_sparse_multi_tu)
asc_register_test(
  NAME asc_cpp.sparse.multi_tu
  TARGET asc_sparse_multi_tu
  LABELS "compile;sparse"
)

foreach(_negative_source IN ITEMS
    m4_negative_builder_copy.cc
    m4_negative_coordinate_owner_copy.cc
    m4_negative_const_value_mutation.cc
    m4_negative_structure_mutation.cc
    m4_negative_unsupported_element.cc
    m4_negative_unsupported_spmv_scalar.cc
    m4_negative_non_extents_builder.cc
    m4_negative_unadapted_writable.cc
    m4_negative_wrong_rank_spmv.cc
    m4_negative_invalid_compressed_format.cc
    m4_negative_same_format_conversion.cc
    m10_negative_unsupported_sparse_blas_scalar.cc
)
  get_filename_component(_negative_name "${_negative_source}" NAME_WE)
  try_compile(
    _negative_compiled
    "${CMAKE_CURRENT_BINARY_DIR}/negative/${_negative_name}"
    SOURCES "${PROJECT_SOURCE_DIR}/tests/compile/${_negative_source}"
    CMAKE_FLAGS
      "-DCMAKE_CXX_EXTENSIONS:BOOL=OFF"
      "-DCMAKE_CXX_STANDARD:STRING=20"
      "-DCMAKE_CXX_STANDARD_REQUIRED:BOOL=ON"
      "-DINCLUDE_DIRECTORIES:STRING=${PROJECT_SOURCE_DIR}/include"
    OUTPUT_VARIABLE _negative_output
  )
  if(_negative_compiled)
    message(FATAL_ERROR
      "Expected ${_negative_source} to fail compilation.\n"
      "${_negative_output}"
    )
  endif()
endforeach()

add_executable(
  asc_sparse_benchmark
  "${PROJECT_SOURCE_DIR}/benchmarks/sparse/sparse_benchmark.cc"
  allocation_probe.cc
)
_asc_sparse_configure_test(asc_sparse_benchmark)
asc_register_test(
  NAME asc_cpp.sparse.benchmark
  TARGET asc_sparse_benchmark
  LABELS "benchmark;sparse;performance"
)
set_tests_properties(
  asc_cpp.sparse.benchmark
  PROPERTIES TIMEOUT 60
)
set_property(
  TEST asc_cpp.sparse.benchmark
  APPEND PROPERTY LABELS sparse-blas
)

unset(_name)
unset(_negative_compiled)
unset(_negative_name)
unset(_negative_output)
unset(_negative_source)
unset(_source)
unset(_target)
