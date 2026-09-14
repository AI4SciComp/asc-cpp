add_executable(asc_rotation_test rotation_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_rotation_test)
target_include_directories(asc_rotation_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_libraries(asc_rotation_test PRIVATE Threads::Threads)
target_link_options(asc_rotation_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_scalar IN ITEMS s d)
  foreach(_mode IN ITEMS mathematical fidelity concurrency)
    asc_register_test(NAME "asc_cpp.dense_lapack.rotation.${_scalar}.${_mode}"
      TARGET asc_rotation_test ARGUMENTS "${_scalar}" "${_mode}"
      LABELS dense_lapack lapack numerical validation rotation TIMEOUT 120)
  endforeach()
endforeach()
foreach(_kind IN ITEMS fault validation)
  add_executable(asc_rotation_${_kind}_test rotation_${_kind}_test.cc
    rotation_faults.cc allocation_audit.cc
    "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
  _asc_dense_lapack_configure_test(asc_rotation_${_kind}_test)
  target_include_directories(asc_rotation_${_kind}_test PRIVATE
    "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
  target_link_options(asc_rotation_${_kind}_test PRIVATE
    "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free"
    "-Wl,--wrap=slartg_,--wrap=dlartg_")
  foreach(_scalar IN ITEMS s d)
    asc_register_test(NAME "asc_cpp.dense_lapack.rotation.${_kind}.${_scalar}"
      TARGET asc_rotation_${_kind}_test ARGUMENTS "${_scalar}"
      LABELS dense_lapack lapack validation rotation TIMEOUT 120)
  endforeach()
endforeach()
add_executable(asc_rotation_abi_probe rotation_abi_probe.cc)
_asc_dense_lapack_configure_test(asc_rotation_abi_probe)
target_include_directories(asc_rotation_abi_probe PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
asc_register_test(NAME asc_cpp.dense_lapack.rotation.abi
  TARGET asc_rotation_abi_probe LABELS dense_lapack lapack abi rotation TIMEOUT 120)
