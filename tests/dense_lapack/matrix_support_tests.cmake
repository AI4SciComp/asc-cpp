add_executable(asc_matrix_support_test matrix_support_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_matrix_support_test)
target_include_directories(asc_matrix_support_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_libraries(asc_matrix_support_test PRIVATE Threads::Threads)
target_link_options(asc_matrix_support_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_operation IN ITEMS ilalc ilalr)
  foreach(_scalar IN ITEMS s d c z)
    foreach(_mode IN ITEMS mathematical fidelity concurrency)
      asc_register_test(NAME "asc_cpp.dense_lapack.matrix_support.${_operation}.${_scalar}.${_mode}"
        TARGET asc_matrix_support_test ARGUMENTS "${_operation}" "${_scalar}" "${_mode}"
        LABELS dense_lapack lapack numerical validation matrix_support TIMEOUT 120)
    endforeach()
  endforeach()
endforeach()
foreach(_kind IN ITEMS fault validation)
  add_executable(asc_matrix_support_${_kind}_test matrix_support_${_kind}_test.cc
    matrix_support_faults.cc allocation_audit.cc
    "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
  _asc_dense_lapack_configure_test(asc_matrix_support_${_kind}_test)
  target_include_directories(asc_matrix_support_${_kind}_test PRIVATE
    "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
  target_link_options(asc_matrix_support_${_kind}_test PRIVATE
    "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free"
    "-Wl,--wrap=ilaslc_,--wrap=ilaslr_,--wrap=iladlc_,--wrap=iladlr_,--wrap=ilaclc_,--wrap=ilaclr_,--wrap=ilazlc_,--wrap=ilazlr_")
  foreach(_operation IN ITEMS ilalc ilalr)
    foreach(_scalar IN ITEMS s d c z)
      asc_register_test(NAME "asc_cpp.dense_lapack.matrix_support.${_operation}.${_kind}.${_scalar}"
        TARGET asc_matrix_support_${_kind}_test ARGUMENTS "${_operation}" "${_scalar}"
        LABELS dense_lapack lapack validation matrix_support TIMEOUT 120)
    endforeach()
  endforeach()
endforeach()
add_executable(asc_matrix_support_abi_probe matrix_support_abi_probe.cc)
_asc_dense_lapack_configure_test(asc_matrix_support_abi_probe)
target_include_directories(asc_matrix_support_abi_probe PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
asc_register_test(NAME asc_cpp.dense_lapack.matrix_support.abi
  TARGET asc_matrix_support_abi_probe LABELS dense_lapack lapack abi matrix_support TIMEOUT 120)
