add_executable(asc_triangular_storage_conversion_test
  triangular_storage_conversion_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_triangular_storage_conversion_test)
target_include_directories(asc_triangular_storage_conversion_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_libraries(asc_triangular_storage_conversion_test PRIVATE Threads::Threads)
target_link_options(asc_triangular_storage_conversion_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_scalar IN ITEMS s d c z)
  foreach(_mode IN ITEMS mathematical fidelity validation concurrency)
    asc_register_test(NAME "asc_cpp.dense_lapack.triangular_storage_conversion.${_scalar}.${_mode}"
      TARGET asc_triangular_storage_conversion_test ARGUMENTS "${_scalar}" "${_mode}"
      LABELS dense_lapack lapack numerical validation triangular_storage_conversion
      TIMEOUT 120)
  endforeach()
endforeach()
add_executable(asc_triangular_storage_conversion_fault_test
  triangular_storage_conversion_fault_test.cc triangular_storage_conversion_faults.cc
  allocation_audit.cc "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_triangular_storage_conversion_fault_test)
target_include_directories(asc_triangular_storage_conversion_fault_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_triangular_storage_conversion_fault_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free"
  "-Wl,--wrap=strttp_,--wrap=dtrttp_,--wrap=ctrttp_,--wrap=ztrttp_,--wrap=stpttr_,--wrap=dtpttr_,--wrap=ctpttr_,--wrap=ztpttr_")
foreach(_scalar IN ITEMS s d c z counts)
  asc_register_test(NAME "asc_cpp.dense_lapack.triangular_storage_conversion.fault.${_scalar}"
    TARGET asc_triangular_storage_conversion_fault_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack validation triangular_storage_conversion TIMEOUT 120)
endforeach()

add_executable(asc_triangular_storage_conversion_abi_probe
  triangular_storage_conversion_abi_probe.cc)
_asc_dense_lapack_configure_test(asc_triangular_storage_conversion_abi_probe)
target_include_directories(asc_triangular_storage_conversion_abi_probe PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
asc_register_test(NAME asc_cpp.dense_lapack.triangular_storage_conversion.abi
  TARGET asc_triangular_storage_conversion_abi_probe
  LABELS dense_lapack lapack abi triangular_storage_conversion TIMEOUT 120)
