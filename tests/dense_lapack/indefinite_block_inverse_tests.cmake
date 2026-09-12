add_executable(asc_dense_lapack_indefinite_block_inverse_test
  indefinite_block_inverse_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_block_inverse_test)
target_include_directories(asc_dense_lapack_indefinite_block_inverse_test PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_indefinite_block_inverse_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_variant IN ITEMS driver 1 2 3 64)
foreach(_scalar IN ITEMS s d c z ch zh)
foreach(_mode IN ITEMS mathematical fidelity)
  asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_block_inverse.${_scalar}.${_variant}.${_mode}"
    TARGET asc_dense_lapack_indefinite_block_inverse_test ARGUMENTS "${_scalar}" "${_variant}" "${_mode}"
    LABELS dense_lapack lapack runtime numerical validation allocation TIMEOUT 120)
endforeach()
endforeach()
endforeach()

add_executable(asc_dense_lapack_indefinite_block_inverse_abi_probe
  indefinite_block_inverse_abi_probe.cc)
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_block_inverse_abi_probe)
target_include_directories(asc_dense_lapack_indefinite_block_inverse_abi_probe PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
foreach(_variant IN ITEMS driver 1 2 3 64)
asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_block_inverse_abi_probe.${_variant}"
  TARGET asc_dense_lapack_indefinite_block_inverse_abi_probe ARGUMENTS "${_variant}"
  LABELS dense_lapack lapack abi runtime TIMEOUT 120)
endforeach()

add_executable(asc_dense_lapack_indefinite_block_inverse_range_test
  indefinite_block_inverse_range_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_block_inverse_range_test)
target_include_directories(asc_dense_lapack_indefinite_block_inverse_range_test PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_indefinite_block_inverse_range_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_variant IN ITEMS driver 1 2 3 64)
foreach(_scalar IN ITEMS s d c z ch zh)
  foreach(_mode IN ITEMS mathematical fidelity)
    asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_block_inverse_range.${_scalar}.${_variant}.${_mode}"
      TARGET asc_dense_lapack_indefinite_block_inverse_range_test ARGUMENTS "${_scalar}" "${_mode}" "${_variant}"
      LABELS dense_lapack lapack runtime numerical allocation TIMEOUT 120)
  endforeach()
endforeach()
endforeach()

add_executable(asc_dense_lapack_indefinite_block_inverse_fault_test
  indefinite_block_inverse_fault_test.cc indefinite_block_inverse_faults.cc
  allocation_audit.cc "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_block_inverse_fault_test)
target_include_directories(asc_dense_lapack_indefinite_block_inverse_fault_test PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_indefinite_block_inverse_fault_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_symbol IN ITEMS ssytri2 dsytri2 csytri2 zsytri2 chetri2 zhetri2 ssytri2x dsytri2x csytri2x zsytri2x chetri2x zhetri2x)
  target_link_options(asc_dense_lapack_indefinite_block_inverse_fault_test PRIVATE "-Wl,--wrap=${_symbol}_")
endforeach()
foreach(_variant IN ITEMS driver 1 2 3 64)
foreach(_scalar IN ITEMS s d c z ch zh)
  asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_block_inverse_fault.${_scalar}.${_variant}"
    TARGET asc_dense_lapack_indefinite_block_inverse_fault_test ARGUMENTS "${_scalar}" "${_variant}"
    LABELS dense_lapack lapack runtime validation abi allocation TIMEOUT 120)
endforeach()
endforeach()

add_executable(asc_dense_lapack_indefinite_block_inverse_validation_test
  indefinite_block_inverse_validation_test.cc
  allocation_audit.cc "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_block_inverse_validation_test)
target_include_directories(asc_dense_lapack_indefinite_block_inverse_validation_test PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_indefinite_block_inverse_validation_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_variant IN ITEMS driver 1 2 3 64)
foreach(_scalar IN ITEMS s d c z ch zh)
  asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_block_inverse_validation.${_scalar}.${_variant}"
    TARGET asc_dense_lapack_indefinite_block_inverse_validation_test ARGUMENTS "${_scalar}" "${_variant}"
    LABELS dense_lapack lapack runtime validation abi allocation TIMEOUT 120)
endforeach()
endforeach()

add_executable(asc_dense_lapack_indefinite_block_inverse_concurrency_test
  indefinite_block_inverse_concurrency_test.cc)
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_block_inverse_concurrency_test)
target_link_libraries(asc_dense_lapack_indefinite_block_inverse_concurrency_test PRIVATE Threads::Threads)
foreach(_variant IN ITEMS driver 1 2 3 64)
foreach(_scalar IN ITEMS s d c z ch zh)
  asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_block_inverse_concurrency.${_scalar}.${_variant}"
    TARGET asc_dense_lapack_indefinite_block_inverse_concurrency_test ARGUMENTS "${_scalar}" "${_variant}"
    LABELS dense_lapack lapack runtime numerical concurrency TIMEOUT 120)
endforeach()
endforeach()
