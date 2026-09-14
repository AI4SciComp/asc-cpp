add_executable(asc_vector_conjugation_test vector_conjugation_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_vector_conjugation_test)
target_include_directories(asc_vector_conjugation_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_libraries(asc_vector_conjugation_test PRIVATE Threads::Threads)
target_link_options(asc_vector_conjugation_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_scalar IN ITEMS c z)
  foreach(_mode IN ITEMS mathematical fidelity concurrency)
    asc_register_test(NAME "asc_cpp.dense_lapack.vector_conjugation.${_scalar}.${_mode}"
      TARGET asc_vector_conjugation_test ARGUMENTS "${_scalar}" "${_mode}"
      LABELS dense_lapack lapack numerical validation vector_conjugation TIMEOUT 120)
  endforeach()
endforeach()
add_executable(asc_vector_conjugation_validation_test vector_conjugation_validation_test.cc
  allocation_audit.cc "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_vector_conjugation_validation_test)
target_include_directories(asc_vector_conjugation_validation_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_vector_conjugation_validation_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free"
  "-Wl,--wrap=clacgv_,--wrap=zlacgv_")
foreach(_scalar IN ITEMS c z counts)
  asc_register_test(NAME "asc_cpp.dense_lapack.vector_conjugation.validation.${_scalar}"
    TARGET asc_vector_conjugation_validation_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack validation vector_conjugation TIMEOUT 120)
endforeach()
add_executable(asc_vector_conjugation_abi_probe vector_conjugation_abi_probe.cc)
_asc_dense_lapack_configure_test(asc_vector_conjugation_abi_probe)
target_include_directories(asc_vector_conjugation_abi_probe PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
asc_register_test(NAME asc_cpp.dense_lapack.vector_conjugation.abi
  TARGET asc_vector_conjugation_abi_probe
  LABELS dense_lapack lapack abi vector_conjugation TIMEOUT 120)
