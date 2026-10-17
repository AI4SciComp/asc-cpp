add_executable(asc_dense_lapack_indefinite_packed_solve_test
  indefinite_packed_solve_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_packed_solve_test)
target_include_directories(asc_dense_lapack_indefinite_packed_solve_test PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_indefinite_packed_solve_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_scalar IN ITEMS s d c z ch zh)
  foreach(_mode IN ITEMS mathematical fidelity)
    asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_packed_solve.${_scalar}.${_mode}"
      TARGET asc_dense_lapack_indefinite_packed_solve_test
      ARGUMENTS "${_scalar}" "${_mode}"
      LABELS dense_lapack lapack runtime numerical validation allocation TIMEOUT 120)
  endforeach()
endforeach()

foreach(_scalar IN ITEMS s d c z ch zh)
  foreach(_mode IN ITEMS mathematical fidelity)
    asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_packed_solve_range.${_scalar}.${_mode}"
      TARGET asc_dense_lapack_indefinite_packed_solve_test
      ARGUMENTS "${_scalar}" "range_${_mode}"
      LABELS dense_lapack lapack runtime numerical validation allocation TIMEOUT 120)
  endforeach()
endforeach()

add_executable(asc_dense_lapack_indefinite_packed_solve_counts_test
  indefinite_packed_solve_counts_test.cc)
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_packed_solve_counts_test)
target_include_directories(asc_dense_lapack_indefinite_packed_solve_counts_test PRIVATE "${PROJECT_SOURCE_DIR}")
asc_register_test(NAME asc_cpp.dense_lapack.indefinite_packed_solve_counts
  TARGET asc_dense_lapack_indefinite_packed_solve_counts_test
  LABELS dense_lapack lapack runtime validation TIMEOUT 120)

add_executable(asc_dense_lapack_indefinite_packed_solve_validation_test
  indefinite_packed_solve_validation_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_packed_solve_validation_test)
target_include_directories(asc_dense_lapack_indefinite_packed_solve_validation_test PRIVATE "${PROJECT_SOURCE_DIR}")
target_link_options(asc_dense_lapack_indefinite_packed_solve_validation_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_scalar IN ITEMS s d c z ch zh)
  asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_packed_solve_validation.${_scalar}"
    TARGET asc_dense_lapack_indefinite_packed_solve_validation_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack runtime validation allocation TIMEOUT 120)
endforeach()

add_executable(asc_dense_lapack_indefinite_packed_solve_concurrency_test
  indefinite_packed_solve_concurrency_test.cc)
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_packed_solve_concurrency_test)
target_include_directories(asc_dense_lapack_indefinite_packed_solve_concurrency_test PRIVATE "${PROJECT_SOURCE_DIR}")
target_link_libraries(asc_dense_lapack_indefinite_packed_solve_concurrency_test PRIVATE Threads::Threads)
foreach(_scalar IN ITEMS s d c z ch zh)
  asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_packed_solve_concurrency.${_scalar}"
    TARGET asc_dense_lapack_indefinite_packed_solve_concurrency_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack runtime concurrency TIMEOUT 120)
endforeach()

add_executable(asc_dense_lapack_indefinite_packed_solve_fault_test
  indefinite_packed_solve_fault_test.cc indefinite_packed_solve_faults.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_packed_solve_fault_test)
target_include_directories(asc_dense_lapack_indefinite_packed_solve_fault_test PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_indefinite_packed_solve_fault_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_symbol IN ITEMS ssptrs dsptrs csptrs zsptrs chptrs zhptrs)
  target_link_options(asc_dense_lapack_indefinite_packed_solve_fault_test PRIVATE "-Wl,--wrap=${_symbol}_")
endforeach()
foreach(_scalar IN ITEMS s d c z ch zh)
  asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_packed_solve_fault.${_scalar}"
    TARGET asc_dense_lapack_indefinite_packed_solve_fault_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack runtime validation abi allocation TIMEOUT 120)
endforeach()

add_executable(asc_dense_lapack_indefinite_packed_solve_abi_probe
  indefinite_packed_solve_abi_probe.cc)
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_packed_solve_abi_probe)
target_include_directories(asc_dense_lapack_indefinite_packed_solve_abi_probe PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
asc_register_test(NAME asc_cpp.dense_lapack.indefinite_packed_solve_abi_probe
  TARGET asc_dense_lapack_indefinite_packed_solve_abi_probe
  LABELS dense_lapack lapack runtime abi numerical TIMEOUT 120)
