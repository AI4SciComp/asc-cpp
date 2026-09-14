add_executable(asc_positive_rotation_test positive_rotation_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_positive_rotation_test)
target_include_directories(asc_positive_rotation_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_libraries(asc_positive_rotation_test PRIVATE Threads::Threads)
target_link_options(asc_positive_rotation_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_scalar IN ITEMS s d)
  foreach(_mode IN ITEMS mathematical fidelity concurrency)
    asc_register_test(NAME "asc_cpp.dense_lapack.positive_rotation.${_scalar}.${_mode}"
      TARGET asc_positive_rotation_test ARGUMENTS "${_scalar}" "${_mode}"
      LABELS dense_lapack lapack numerical validation positive_rotation TIMEOUT 120)
  endforeach()
endforeach()
foreach(_kind IN ITEMS fault validation)
  add_executable(asc_positive_rotation_${_kind}_test positive_rotation_${_kind}_test.cc
    positive_rotation_faults.cc allocation_audit.cc
    "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
  _asc_dense_lapack_configure_test(asc_positive_rotation_${_kind}_test)
  target_include_directories(asc_positive_rotation_${_kind}_test PRIVATE
    "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
  target_link_options(asc_positive_rotation_${_kind}_test PRIVATE
    "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free"
    "-Wl,--wrap=slartgp_,--wrap=dlartgp_")
  foreach(_scalar IN ITEMS s d)
    asc_register_test(NAME "asc_cpp.dense_lapack.positive_rotation.${_kind}.${_scalar}"
      TARGET asc_positive_rotation_${_kind}_test ARGUMENTS "${_scalar}"
      LABELS dense_lapack lapack validation positive_rotation TIMEOUT 120)
  endforeach()
endforeach()
add_executable(asc_positive_rotation_abi_probe positive_rotation_abi_probe.cc)
_asc_dense_lapack_configure_test(asc_positive_rotation_abi_probe)
target_include_directories(asc_positive_rotation_abi_probe PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
asc_register_test(NAME asc_cpp.dense_lapack.positive_rotation.abi
  TARGET asc_positive_rotation_abi_probe LABELS dense_lapack lapack abi positive_rotation TIMEOUT 120)
