add_executable(asc_dense_lapack_indefinite_block_solve_test
  indefinite_block_solve_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_block_solve_test)
target_include_directories(asc_dense_lapack_indefinite_block_solve_test PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_indefinite_block_solve_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_scalar IN ITEMS s d c z ch zh)
  foreach(_mode IN ITEMS mathematical fidelity)
    asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_block_solve.${_scalar}.${_mode}"
      TARGET asc_dense_lapack_indefinite_block_solve_test ARGUMENTS "${_scalar}" "${_mode}"
      LABELS dense_lapack lapack runtime numerical validation allocation TIMEOUT 120)
  endforeach()
endforeach()

add_executable(asc_dense_lapack_indefinite_block_solve_abi_probe
  indefinite_block_solve_abi_probe.cc)
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_block_solve_abi_probe)
target_include_directories(asc_dense_lapack_indefinite_block_solve_abi_probe PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_block_solve_abi_probe"
  TARGET asc_dense_lapack_indefinite_block_solve_abi_probe
  LABELS dense_lapack lapack abi runtime TIMEOUT 120)

add_executable(asc_dense_lapack_indefinite_block_solve_range_test
  indefinite_block_solve_range_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_block_solve_range_test)
target_include_directories(asc_dense_lapack_indefinite_block_solve_range_test PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_indefinite_block_solve_range_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_scalar IN ITEMS s d c z ch zh)
  foreach(_mode IN ITEMS mathematical fidelity)
    asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_block_solve_range.${_scalar}.${_mode}"
      TARGET asc_dense_lapack_indefinite_block_solve_range_test ARGUMENTS "${_scalar}" "${_mode}"
      LABELS dense_lapack lapack runtime numerical allocation TIMEOUT 120)
  endforeach()
endforeach()

add_executable(asc_dense_lapack_indefinite_block_solve_fault_test
  indefinite_block_solve_fault_test.cc indefinite_block_solve_faults.cc
  allocation_audit.cc "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_block_solve_fault_test)
target_include_directories(asc_dense_lapack_indefinite_block_solve_fault_test PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_indefinite_block_solve_fault_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_symbol IN ITEMS ssytrs2 dsytrs2 csytrs2 zsytrs2 chetrs2 zhetrs2)
  target_link_options(asc_dense_lapack_indefinite_block_solve_fault_test PRIVATE "-Wl,--wrap=${_symbol}_")
endforeach()
foreach(_scalar IN ITEMS s d c z ch zh)
  asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_block_solve_fault.${_scalar}"
    TARGET asc_dense_lapack_indefinite_block_solve_fault_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack runtime validation abi allocation TIMEOUT 120)
endforeach()

add_executable(asc_dense_lapack_indefinite_block_solve_validation_test
  indefinite_block_solve_validation_test.cc
  allocation_audit.cc "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_block_solve_validation_test)
target_include_directories(asc_dense_lapack_indefinite_block_solve_validation_test PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_indefinite_block_solve_validation_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_scalar IN ITEMS s d c z ch zh)
  asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_block_solve_validation.${_scalar}"
    TARGET asc_dense_lapack_indefinite_block_solve_validation_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack runtime validation abi allocation TIMEOUT 120)
endforeach()

add_executable(asc_dense_lapack_indefinite_block_solve_concurrency_test
  indefinite_block_solve_concurrency_test.cc)
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_block_solve_concurrency_test)
target_link_libraries(asc_dense_lapack_indefinite_block_solve_concurrency_test PRIVATE Threads::Threads)
foreach(_scalar IN ITEMS s d c z ch zh)
  asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_block_solve_concurrency.${_scalar}"
    TARGET asc_dense_lapack_indefinite_block_solve_concurrency_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack runtime numerical concurrency TIMEOUT 120)
endforeach()
