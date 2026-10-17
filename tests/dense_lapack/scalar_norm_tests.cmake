add_executable(asc_scalar_norm_test scalar_norm_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_scalar_norm_test)
target_include_directories(asc_scalar_norm_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_libraries(asc_scalar_norm_test PRIVATE Threads::Threads)
target_link_options(asc_scalar_norm_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_scalar IN ITEMS s2 d2 s3 d3)
  foreach(_mode IN ITEMS mathematical fidelity concurrency)
    asc_register_test(NAME "asc_cpp.dense_lapack.scalar_norm.${_scalar}.${_mode}"
      TARGET asc_scalar_norm_test ARGUMENTS "${_scalar}" "${_mode}"
      LABELS dense_lapack lapack numerical validation scalar_norm TIMEOUT 120)
  endforeach()
endforeach()
add_executable(asc_scalar_norm_fault_test scalar_norm_fault_test.cc scalar_norm_faults.cc
  allocation_audit.cc "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_scalar_norm_fault_test)
target_include_directories(asc_scalar_norm_fault_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_scalar_norm_fault_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free"
  "-Wl,--wrap=slapy2_,--wrap=dlapy2_,--wrap=slapy3_,--wrap=dlapy3_")
foreach(_scalar IN ITEMS s2 d2 s3 d3)
  asc_register_test(NAME "asc_cpp.dense_lapack.scalar_norm.fault.${_scalar}"
    TARGET asc_scalar_norm_fault_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack validation scalar_norm TIMEOUT 120)
endforeach()
add_executable(asc_scalar_norm_abi_probe scalar_norm_abi_probe.cc)
_asc_dense_lapack_configure_test(asc_scalar_norm_abi_probe)
target_include_directories(asc_scalar_norm_abi_probe PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
asc_register_test(NAME asc_cpp.dense_lapack.scalar_norm.abi TARGET asc_scalar_norm_abi_probe
  LABELS dense_lapack lapack abi scalar_norm TIMEOUT 120)
