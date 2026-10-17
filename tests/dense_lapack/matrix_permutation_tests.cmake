add_executable(asc_matrix_permutation_test
  matrix_permutation_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_matrix_permutation_test)
target_include_directories(asc_matrix_permutation_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_libraries(asc_matrix_permutation_test PRIVATE Threads::Threads)
target_link_options(asc_matrix_permutation_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_scalar IN ITEMS s d c z)
  foreach(_mode IN ITEMS mathematical fidelity validation concurrency)
    asc_register_test(NAME "asc_cpp.dense_lapack.matrix_permutation.${_scalar}.${_mode}"
      TARGET asc_matrix_permutation_test ARGUMENTS "${_scalar}" "${_mode}"
      LABELS dense_lapack lapack numerical validation matrix_permutation
      TIMEOUT 600)
  endforeach()
endforeach()
add_executable(asc_matrix_permutation_fault_test
  matrix_permutation_fault_test.cc matrix_permutation_faults.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_matrix_permutation_fault_test)
target_include_directories(asc_matrix_permutation_fault_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_matrix_permutation_fault_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free"
  "-Wl,--wrap=slapmr_,--wrap=dlapmr_,--wrap=clapmr_,--wrap=zlapmr_,--wrap=slapmt_,--wrap=dlapmt_,--wrap=clapmt_,--wrap=zlapmt_")
foreach(_scalar IN ITEMS s d c z counts)
  asc_register_test(NAME "asc_cpp.dense_lapack.matrix_permutation.fault.${_scalar}"
    TARGET asc_matrix_permutation_fault_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack validation matrix_permutation TIMEOUT 120)
endforeach()
add_executable(asc_matrix_permutation_abi_probe matrix_permutation_abi_probe.cc)
_asc_dense_lapack_configure_test(asc_matrix_permutation_abi_probe)
target_include_directories(asc_matrix_permutation_abi_probe PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
asc_register_test(NAME asc_cpp.dense_lapack.matrix_permutation.abi
  TARGET asc_matrix_permutation_abi_probe
  LABELS dense_lapack lapack abi matrix_permutation TIMEOUT 600)
