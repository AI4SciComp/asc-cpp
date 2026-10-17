add_executable(asc_dense_lapack_indefinite_aasen_two_stage_driver_abi_probe
  indefinite_aasen_two_stage_driver_abi_probe.cc)
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_aasen_two_stage_driver_abi_probe)
target_include_directories(asc_dense_lapack_indefinite_aasen_two_stage_driver_abi_probe PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
asc_register_test(NAME asc_cpp.dense_lapack.indefinite_aasen_two_stage_driver_abi_probe
  TARGET asc_dense_lapack_indefinite_aasen_two_stage_driver_abi_probe
  LABELS dense_lapack lapack runtime abi numerical TIMEOUT 120)
asc_register_test(NAME asc_cpp.dense_lapack.indefinite_aasen_two_stage_driver_workspace.contract
  TARGET asc_dense_lapack_indefinite_aasen_two_stage_driver_abi_probe ARGUMENTS workspace_contract
  LABELS dense_lapack lapack runtime workspace numerical TIMEOUT 120)

add_executable(asc_dense_lapack_indefinite_aasen_two_stage_driver_test
  indefinite_aasen_two_stage_driver_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_aasen_two_stage_driver_test)
target_include_directories(asc_dense_lapack_indefinite_aasen_two_stage_driver_test PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_indefinite_aasen_two_stage_driver_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_scalar IN ITEMS s d c z ch zh)
  foreach(_mode IN ITEMS mathematical fidelity)
    asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_aasen_two_stage_driver.${_scalar}.${_mode}"
      TARGET asc_dense_lapack_indefinite_aasen_two_stage_driver_test ARGUMENTS "${_scalar}" "${_mode}"
      LABELS dense_lapack lapack runtime numerical validation allocation TIMEOUT 120)
  endforeach()
endforeach()

add_executable(asc_dense_lapack_indefinite_aasen_two_stage_driver_range_test
  indefinite_aasen_two_stage_driver_range_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_aasen_two_stage_driver_range_test)
target_include_directories(asc_dense_lapack_indefinite_aasen_two_stage_driver_range_test PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_indefinite_aasen_two_stage_driver_range_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_scalar IN ITEMS s d c z ch zh)
  foreach(_mode IN ITEMS mathematical fidelity)
    asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_aasen_two_stage_driver_range.${_scalar}.${_mode}"
      TARGET asc_dense_lapack_indefinite_aasen_two_stage_driver_range_test ARGUMENTS "${_scalar}" "${_mode}"
      LABELS dense_lapack lapack runtime numerical validation allocation TIMEOUT 120)
  endforeach()
endforeach()

add_executable(asc_dense_lapack_indefinite_aasen_two_stage_driver_fault_test
  indefinite_aasen_two_stage_driver_fault_test.cc indefinite_aasen_two_stage_driver_faults.cc
  allocation_audit.cc "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_aasen_two_stage_driver_fault_test)
target_include_directories(asc_dense_lapack_indefinite_aasen_two_stage_driver_fault_test PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_indefinite_aasen_two_stage_driver_fault_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_symbol IN ITEMS ssysv_aa_2stage dsysv_aa_2stage csysv_aa_2stage
    zsysv_aa_2stage chesv_aa_2stage zhesv_aa_2stage)
  target_link_options(asc_dense_lapack_indefinite_aasen_two_stage_driver_fault_test PRIVATE "-Wl,--wrap=${_symbol}_")
endforeach()
foreach(_scalar IN ITEMS s d c z ch zh)
  asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_aasen_two_stage_driver_fault.${_scalar}"
    TARGET asc_dense_lapack_indefinite_aasen_two_stage_driver_fault_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack runtime validation abi allocation TIMEOUT 120)
endforeach()

add_executable(asc_dense_lapack_indefinite_aasen_two_stage_driver_validation_test
  indefinite_aasen_two_stage_driver_validation_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_aasen_two_stage_driver_validation_test)
target_include_directories(asc_dense_lapack_indefinite_aasen_two_stage_driver_validation_test PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_indefinite_aasen_two_stage_driver_validation_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_scalar IN ITEMS s d c z ch zh)
  asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_aasen_two_stage_driver_validation.${_scalar}"
    TARGET asc_dense_lapack_indefinite_aasen_two_stage_driver_validation_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack runtime validation abi allocation TIMEOUT 120)
endforeach()

add_executable(asc_dense_lapack_indefinite_aasen_two_stage_driver_concurrency_test
  indefinite_aasen_two_stage_driver_concurrency_test.cc)
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_aasen_two_stage_driver_concurrency_test)
target_include_directories(asc_dense_lapack_indefinite_aasen_two_stage_driver_concurrency_test PRIVATE "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_libraries(asc_dense_lapack_indefinite_aasen_two_stage_driver_concurrency_test PRIVATE Threads::Threads)
foreach(_scalar IN ITEMS s d c z ch zh)
  asc_register_test(NAME "asc_cpp.dense_lapack.indefinite_aasen_two_stage_driver_concurrency.${_scalar}"
    TARGET asc_dense_lapack_indefinite_aasen_two_stage_driver_concurrency_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack runtime concurrency TIMEOUT 120)
endforeach()
