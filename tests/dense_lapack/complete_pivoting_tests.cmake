add_executable(asc_complete_pivoting_test
  complete_pivoting_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_complete_pivoting_test)
target_include_directories(asc_complete_pivoting_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_libraries(asc_complete_pivoting_test PRIVATE Threads::Threads)
target_link_options(asc_complete_pivoting_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_scalar IN ITEMS s d c z)
  foreach(_mode IN ITEMS mathematical fidelity validation concurrency)
    asc_register_test(NAME "asc_cpp.dense_lapack.complete_pivoting.${_scalar}.${_mode}"
      TARGET asc_complete_pivoting_test ARGUMENTS "${_scalar}" "${_mode}"
      LABELS dense_lapack lapack numerical validation complete_pivoting
      TIMEOUT 600)
  endforeach()
endforeach()
add_executable(asc_complete_pivoting_fault_test
  complete_pivoting_fault_test.cc complete_pivoting_faults.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_complete_pivoting_fault_test)
target_include_directories(asc_complete_pivoting_fault_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_complete_pivoting_fault_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free"
  "-Wl,--wrap=sgetc2_,--wrap=dgetc2_,--wrap=cgetc2_,--wrap=zgetc2_,--wrap=sgesc2_,--wrap=dgesc2_,--wrap=cgesc2_,--wrap=zgesc2_")
foreach(_scalar IN ITEMS s d c z counts)
  asc_register_test(NAME "asc_cpp.dense_lapack.complete_pivoting.fault.${_scalar}"
    TARGET asc_complete_pivoting_fault_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack validation complete_pivoting TIMEOUT 120)
endforeach()
add_executable(asc_complete_pivoting_validation_test
  complete_pivoting_validation_test.cc complete_pivoting_faults.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_complete_pivoting_validation_test)
target_include_directories(asc_complete_pivoting_validation_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_complete_pivoting_validation_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free"
  "-Wl,--wrap=sgetc2_,--wrap=dgetc2_,--wrap=cgetc2_,--wrap=zgetc2_,--wrap=sgesc2_,--wrap=dgesc2_,--wrap=cgesc2_,--wrap=zgesc2_")
foreach(_scalar IN ITEMS s d c z)
  asc_register_test(NAME "asc_cpp.dense_lapack.complete_pivoting.preflight.${_scalar}"
    TARGET asc_complete_pivoting_validation_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack validation complete_pivoting TIMEOUT 120)
endforeach()
add_executable(asc_complete_pivoting_abi_probe complete_pivoting_abi_probe.cc)
_asc_dense_lapack_configure_test(asc_complete_pivoting_abi_probe)
target_include_directories(asc_complete_pivoting_abi_probe PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
asc_register_test(NAME asc_cpp.dense_lapack.complete_pivoting.abi
  TARGET asc_complete_pivoting_abi_probe
  LABELS dense_lapack lapack abi complete_pivoting TIMEOUT 600)
