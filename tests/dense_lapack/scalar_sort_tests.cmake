add_executable(asc_scalar_sort_test scalar_sort_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_scalar_sort_test)
target_include_directories(asc_scalar_sort_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_libraries(asc_scalar_sort_test PRIVATE Threads::Threads)
target_link_options(asc_scalar_sort_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_scalar IN ITEMS s d)
  foreach(_mode IN ITEMS mathematical fidelity concurrency)
    asc_register_test(NAME "asc_cpp.dense_lapack.scalar_sort.${_scalar}.${_mode}"
      TARGET asc_scalar_sort_test ARGUMENTS "${_scalar}" "${_mode}"
      LABELS dense_lapack lapack numerical validation scalar_sort TIMEOUT 120)
  endforeach()
endforeach()
add_executable(asc_scalar_sort_fault_test scalar_sort_fault_test.cc scalar_sort_faults.cc
  allocation_audit.cc "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_scalar_sort_fault_test)
target_include_directories(asc_scalar_sort_fault_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_scalar_sort_fault_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free"
  "-Wl,--wrap=slasrt_,--wrap=dlasrt_")
foreach(_scalar IN ITEMS s d counts)
  asc_register_test(NAME "asc_cpp.dense_lapack.scalar_sort.fault.${_scalar}"
    TARGET asc_scalar_sort_fault_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack validation scalar_sort TIMEOUT 120)
endforeach()
add_executable(asc_scalar_sort_abi_probe scalar_sort_abi_probe.cc)
_asc_dense_lapack_configure_test(asc_scalar_sort_abi_probe)
target_include_directories(asc_scalar_sort_abi_probe PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
asc_register_test(NAME asc_cpp.dense_lapack.scalar_sort.abi
  TARGET asc_scalar_sort_abi_probe
  LABELS dense_lapack lapack abi scalar_sort TIMEOUT 120)
