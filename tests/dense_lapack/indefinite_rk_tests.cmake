add_executable(asc_dense_lapack_indefinite_rk_test
  indefinite_rk_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_rk_test)
target_include_directories(asc_dense_lapack_indefinite_rk_test PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_indefinite_rk_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_scalar IN ITEMS s d c z ch zh)
  foreach(_mode IN ITEMS mathematical fidelity)
    asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_rk.${_scalar}.${_mode}"
      TARGET asc_dense_lapack_indefinite_rk_test ARGUMENTS "${_scalar}" "${_mode}"
      LABELS dense_lapack lapack runtime numerical validation allocation TIMEOUT 120)
  endforeach()
endforeach()

add_executable(asc_dense_lapack_indefinite_rk_range_test
  indefinite_rk_range_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_rk_range_test)
target_include_directories(asc_dense_lapack_indefinite_rk_range_test PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_indefinite_rk_range_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_scalar IN ITEMS s d c z ch zh)
  foreach(_mode IN ITEMS mathematical fidelity)
    asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_rk_range.${_scalar}.${_mode}"
      TARGET asc_dense_lapack_indefinite_rk_range_test ARGUMENTS "${_scalar}" "${_mode}"
      LABELS dense_lapack lapack runtime numerical validation allocation TIMEOUT 120)
  endforeach()
endforeach()

add_executable(asc_dense_lapack_indefinite_rk_abi_probe indefinite_rk_abi_probe.cc)
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_rk_abi_probe)
target_include_directories(asc_dense_lapack_indefinite_rk_abi_probe PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_rk_abi_probe"
  TARGET asc_dense_lapack_indefinite_rk_abi_probe ARGUMENTS active
  LABELS dense_lapack lapack runtime abi TIMEOUT 120)
asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_rk_native_empty"
  TARGET asc_dense_lapack_indefinite_rk_abi_probe ARGUMENTS empty
  LABELS dense_lapack lapack runtime validation numerical TIMEOUT 120)

add_executable(asc_dense_lapack_indefinite_rk_fault_test
  indefinite_rk_fault_test.cc indefinite_rk_faults.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_rk_fault_test)
target_include_directories(asc_dense_lapack_indefinite_rk_fault_test PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_indefinite_rk_fault_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_prefix IN ITEMS ssy dsy csy zsy che zhe)
  foreach(_suffix IN ITEMS tf2 trf)
    target_link_options(asc_dense_lapack_indefinite_rk_fault_test PRIVATE "-Wl,--wrap=${_prefix}${_suffix}_rk_")
  endforeach()
endforeach()
foreach(_scalar IN ITEMS s d c z ch zh)
  asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_rk_fault.${_scalar}"
    TARGET asc_dense_lapack_indefinite_rk_fault_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack runtime validation abi allocation TIMEOUT 120)
endforeach()

add_executable(asc_dense_lapack_indefinite_rk_validation_test
  indefinite_rk_validation_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_rk_validation_test)
target_include_directories(asc_dense_lapack_indefinite_rk_validation_test PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_indefinite_rk_validation_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_scalar IN ITEMS s d c z ch zh)
  asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_rk_validation.${_scalar}"
    TARGET asc_dense_lapack_indefinite_rk_validation_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack runtime validation abi allocation TIMEOUT 120)
endforeach()

add_executable(asc_dense_lapack_indefinite_rk_concurrency_test indefinite_rk_concurrency_test.cc)
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_rk_concurrency_test)
target_link_libraries(asc_dense_lapack_indefinite_rk_concurrency_test PRIVATE Threads::Threads)
foreach(_scalar IN ITEMS s d c z ch zh)
  asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_rk_concurrency.${_scalar}"
    TARGET asc_dense_lapack_indefinite_rk_concurrency_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack runtime numerical concurrency TIMEOUT 120)
endforeach()
