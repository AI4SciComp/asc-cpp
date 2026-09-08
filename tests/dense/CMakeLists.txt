function(_asc_dense_configure_test target)
  target_link_libraries("${target}" PRIVATE ASC::dense)
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
    layout_view_test.cc
    array_evaluate_test.cc
    complex_array_test.cc
    blas_test.cc
    blas_level1_test.cc
    blas_level2_test.cc
    blas_level3_test.cc
    lapack_foundations_test.cc
    lapack_structured_view_test.cc
    lapack_triangular_band_view_test.cc
    lapack_lu_test.cc
    lapack_cholesky_test.cc
    lapack_qr_test.cc
    print_test.cc
    print_acceptance_test.cc
    print_modes_test.cc
    io_test.cc
    io_fuzz_test.cc
    io_stream_progress_test.cc
    matrix_market_test.cc
    resource_failure_test.cc
)
  get_filename_component(_name "${_source}" NAME_WE)
  set(_target "asc_dense_${_name}")
  add_executable("${_target}" "${_source}" allocation_probe.cc)
  _asc_dense_configure_test("${_target}")
  asc_register_test(
    NAME "asc_cpp.dense.${_name}"
    TARGET "${_target}"
    LABELS "dense;runtime"
  )
endforeach()

target_include_directories(
  asc_dense_complex_array_test PRIVATE "${PROJECT_SOURCE_DIR}/tests"
)
target_include_directories(
  asc_dense_print_acceptance_test PRIVATE "${PROJECT_SOURCE_DIR}/tests"
)
target_include_directories(
  asc_dense_print_modes_test PRIVATE "${PROJECT_SOURCE_DIR}/tests"
)
target_include_directories(
  asc_dense_lapack_lu_test PRIVATE "${PROJECT_SOURCE_DIR}/tests"
)
target_include_directories(
  asc_dense_lapack_cholesky_test PRIVATE "${PROJECT_SOURCE_DIR}/tests"
)
target_include_directories(
  asc_dense_lapack_qr_test PRIVATE "${PROJECT_SOURCE_DIR}/tests"
)

foreach(_source IN ITEMS
    native_mode_acceptance_test.cc
    native_alias_acceptance_test.cc)
  get_filename_component(_name "${_source}" NAME_WE)
  set(_target "asc_dense_${_name}")
  add_executable("${_target}" "${_source}" allocation_probe.cc)
  _asc_dense_configure_test("${_target}")
  target_include_directories("${_target}" PRIVATE "${PROJECT_SOURCE_DIR}/tests")
  asc_register_test(
    NAME "asc_cpp.dense.${_name}"
    TARGET "${_target}"
    LABELS "dense;runtime;native-lapack")
endforeach()

find_package(Threads REQUIRED)
add_executable(asc_dense_native_concurrency_test native_concurrency_test.cc)
_asc_dense_configure_test(asc_dense_native_concurrency_test)
target_link_libraries(asc_dense_native_concurrency_test PRIVATE Threads::Threads)
asc_register_test(
  NAME asc_cpp.dense.native_concurrency_test
  TARGET asc_dense_native_concurrency_test
  LABELS "dense;runtime;native-lapack;concurrency")

add_executable(asc_dense_matrix_market_interop_test
  matrix_market_interop_test.cc)
_asc_dense_configure_test(asc_dense_matrix_market_interop_test)
set(_matrix_market_directory "${CMAKE_CURRENT_BINARY_DIR}/matrix-market-interop")
file(MAKE_DIRECTORY "${_matrix_market_directory}")
foreach(_mode IN ITEMS write read)
  asc_register_test(NAME "asc_cpp.dense.matrix_market_interop.${_mode}"
    TARGET asc_dense_matrix_market_interop_test
    ARGUMENTS "${_mode}" "${_matrix_market_directory}"
    LABELS dense runtime matrix-market)
endforeach()
set_tests_properties(asc_cpp.dense.matrix_market_interop.write PROPERTIES
  FIXTURES_SETUP asc_dense_matrix_market_files)
set_tests_properties(asc_cpp.dense.matrix_market_interop.read PROPERTIES
  FIXTURES_REQUIRED asc_dense_matrix_market_files)

set_property(
  TEST asc_cpp.dense.blas_level2_test
  APPEND PROPERTY LABELS "blas-level2;hardening"
)

set_property(
  TEST asc_cpp.dense.blas_level3_test
  APPEND PROPERTY LABELS "blas-level3"
)

add_executable(
  asc_dense_multi_tu
  "${PROJECT_SOURCE_DIR}/tests/compile/m3_multi_tu_a.cc"
  "${PROJECT_SOURCE_DIR}/tests/compile/m3_multi_tu_b.cc"
  "${PROJECT_SOURCE_DIR}/tests/compile/m3_multi_tu_main.cc"
)
target_include_directories(
  asc_dense_multi_tu
  PRIVATE "${PROJECT_SOURCE_DIR}/tests/compile"
)
_asc_dense_configure_test(asc_dense_multi_tu)
asc_register_test(
  NAME asc_cpp.dense.multi_tu
  TARGET asc_dense_multi_tu
  LABELS "compile;dense"
)

foreach(_negative_source IN ITEMS
    m3_negative_owner_copy.cc
    m3_negative_const_view_mutation.cc
    m3_negative_unsupported_owner_element.cc
    m3_negative_unsupported_blas_scalar.cc
    m3_negative_wrong_rank_blas.cc
    m3_negative_removed_transpose_alias.cc
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

function(_asc_dense_check_complex_negative source)
  # A static try-compile checks the declaration itself. Missing main or
  # unresolved library symbols must not make an accepted operation look invalid.
  set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
  get_filename_component(_name "${source}" NAME_WE)
  try_compile(
    _compiled
    "${CMAKE_CURRENT_BINARY_DIR}/negative/${_name}"
    SOURCES "${PROJECT_SOURCE_DIR}/tests/compile/${source}"
    CMAKE_FLAGS
      "-DCMAKE_CXX_EXTENSIONS:BOOL=OFF"
      "-DCMAKE_CXX_STANDARD:STRING=20"
      "-DCMAKE_CXX_STANDARD_REQUIRED:BOOL=ON"
      "-DINCLUDE_DIRECTORIES:STRING=${PROJECT_SOURCE_DIR}/include"
    OUTPUT_VARIABLE _output
  )
  if(_compiled)
    message(FATAL_ERROR "Expected ${source} to fail compilation.\n${_output}")
  endif()
endfunction()

foreach(_source IN ITEMS
    p01_negative_bool_owner.cc
    p01_negative_class_owner.cc
    p01_negative_complex_min.cc
    p01_negative_complex_max.cc
    p01_negative_complex_precision_conversion.cc
)
  _asc_dense_check_complex_negative("${_source}")
endforeach()

add_executable(
  asc_dense_benchmark
  "${PROJECT_SOURCE_DIR}/benchmarks/dense/dense_benchmark.cc"
  allocation_probe.cc
)
_asc_dense_configure_test(asc_dense_benchmark)
asc_register_test(
  NAME asc_cpp.dense.benchmark
  TARGET asc_dense_benchmark
  LABELS "benchmark;blas-level3;dense;performance"
)
set_tests_properties(
  asc_cpp.dense.benchmark
  PROPERTIES TIMEOUT 60
)

unset(_name)
unset(_negative_compiled)
unset(_negative_name)
unset(_negative_output)
unset(_negative_source)
unset(_source)
unset(_target)

# Linux page protection observes descriptor reads on a real live array.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(asc_dense_lapack_triangular_band_read_observation_test
    lapack_triangular_band_read_observation_test.cc allocation_probe.cc)
  _asc_dense_configure_test(asc_dense_lapack_triangular_band_read_observation_test)
  asc_register_test(NAME asc_cpp.dense.lapack_triangular_band_read_observation_test
    TARGET asc_dense_lapack_triangular_band_read_observation_test
    LABELS dense runtime validation allocation TIMEOUT 120)
endif()
