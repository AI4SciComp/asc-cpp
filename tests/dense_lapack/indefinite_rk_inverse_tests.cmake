add_executable(asc_dense_lapack_indefinite_rk_inverse_test
  indefinite_rk_inverse_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_rk_inverse_test)
target_include_directories(asc_dense_lapack_indefinite_rk_inverse_test PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_indefinite_rk_inverse_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_variant IN ITEMS driver 1 2 3 64)
foreach(_scalar IN ITEMS s d c z ch zh)
foreach(_mode IN ITEMS mathematical fidelity)
  asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_rk_inverse.${_scalar}.${_variant}.${_mode}"
    TARGET asc_dense_lapack_indefinite_rk_inverse_test ARGUMENTS "${_scalar}" "${_variant}" "${_mode}"
    LABELS dense_lapack lapack runtime numerical validation allocation TIMEOUT 120)
endforeach()
endforeach()
endforeach()

add_executable(asc_dense_lapack_indefinite_rk_inverse_range_test
  indefinite_rk_inverse_range_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_rk_inverse_range_test)
target_include_directories(asc_dense_lapack_indefinite_rk_inverse_range_test PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_indefinite_rk_inverse_range_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_variant IN ITEMS driver 1 2 3 64)
foreach(_scalar IN ITEMS s d c z ch zh)
  foreach(_mode IN ITEMS mathematical fidelity)
    asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_rk_inverse_range.${_scalar}.${_variant}.${_mode}"
      TARGET asc_dense_lapack_indefinite_rk_inverse_range_test ARGUMENTS "${_scalar}" "${_mode}" "${_variant}"
      LABELS dense_lapack lapack runtime numerical allocation TIMEOUT 120)
  endforeach()
endforeach()
endforeach()

add_executable(asc_dense_lapack_indefinite_rk_inverse_abi_probe indefinite_rk_inverse_abi_probe.cc)
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_rk_inverse_abi_probe)
target_include_directories(asc_dense_lapack_indefinite_rk_inverse_abi_probe PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_rk_inverse_abi_probe"
  TARGET asc_dense_lapack_indefinite_rk_inverse_abi_probe ARGUMENTS --mathematical
  LABELS dense_lapack lapack abi runtime numerical TIMEOUT 120)
add_executable(asc_dense_lapack_indefinite_rk_inverse_workspace_probe indefinite_rk_inverse_workspace_probe.cc)
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_rk_inverse_workspace_probe)
target_include_directories(asc_dense_lapack_indefinite_rk_inverse_workspace_probe PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_rk_inverse_workspace.contract"
  TARGET asc_dense_lapack_indefinite_rk_inverse_workspace_probe ARGUMENTS --contract all
  LABELS dense_lapack lapack abi runtime validation TIMEOUT 120)
asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_rk_inverse_workspace.fidelity"
  TARGET asc_dense_lapack_indefinite_rk_inverse_workspace_probe ARGUMENTS --source-fidelity all
  LABELS dense_lapack lapack abi runtime validation TIMEOUT 120)

add_executable(asc_dense_lapack_indefinite_rk_inverse_fault_test
 indefinite_rk_inverse_fault_test.cc indefinite_rk_inverse_faults.cc
 allocation_audit.cc "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_rk_inverse_fault_test)
target_include_directories(asc_dense_lapack_indefinite_rk_inverse_fault_test PRIVATE
 "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_indefinite_rk_inverse_fault_test PRIVATE
 "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_symbol IN ITEMS ssytri_3 dsytri_3 csytri_3 zsytri_3 chetri_3 zhetri_3 ssytri_3x dsytri_3x csytri_3x zsytri_3x chetri_3x zhetri_3x)
 target_link_options(asc_dense_lapack_indefinite_rk_inverse_fault_test PRIVATE "-Wl,--wrap=${_symbol}_")
endforeach()
foreach(_variant IN ITEMS driver 1 2 3 64)
 foreach(_scalar IN ITEMS s d c z ch zh)
  asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_rk_inverse_fault.${_scalar}.${_variant}"
   TARGET asc_dense_lapack_indefinite_rk_inverse_fault_test ARGUMENTS "${_scalar}" "${_variant}"
   LABELS dense_lapack lapack runtime validation abi allocation TIMEOUT 120)
 endforeach()
endforeach()

add_executable(asc_dense_lapack_indefinite_rk_inverse_validation_test
 indefinite_rk_inverse_validation_test.cc allocation_audit.cc
 "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_rk_inverse_validation_test)
target_include_directories(asc_dense_lapack_indefinite_rk_inverse_validation_test PRIVATE
 "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_indefinite_rk_inverse_validation_test PRIVATE
 "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_variant IN ITEMS driver 1 2 3 64)
 foreach(_scalar IN ITEMS s d c z ch zh)
  asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_rk_inverse_validation.${_scalar}.${_variant}"
   TARGET asc_dense_lapack_indefinite_rk_inverse_validation_test ARGUMENTS "${_scalar}" "${_variant}"
   LABELS dense_lapack lapack runtime validation abi allocation TIMEOUT 120)
 endforeach()
endforeach()

add_executable(asc_dense_lapack_indefinite_rk_inverse_concurrency_test
 indefinite_rk_inverse_concurrency_test.cc)
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_rk_inverse_concurrency_test)
target_link_libraries(asc_dense_lapack_indefinite_rk_inverse_concurrency_test PRIVATE Threads::Threads)
foreach(_variant IN ITEMS driver 1 2 3 64)
 foreach(_scalar IN ITEMS s d c z ch zh)
  asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_rk_inverse_concurrency.${_scalar}.${_variant}"
   TARGET asc_dense_lapack_indefinite_rk_inverse_concurrency_test ARGUMENTS "${_scalar}" "${_variant}"
   LABELS dense_lapack lapack runtime numerical concurrency TIMEOUT 120)
 endforeach()
endforeach()
