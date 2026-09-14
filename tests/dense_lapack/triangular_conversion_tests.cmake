add_executable(asc_triangular_conversion_test triangular_conversion_test.cc
  allocation_audit.cc "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_triangular_conversion_test)
target_include_directories(asc_triangular_conversion_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_libraries(asc_triangular_conversion_test PRIVATE Threads::Threads)
target_link_options(asc_triangular_conversion_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_scalar IN ITEMS d z)
  foreach(_mode IN ITEMS mathematical fidelity concurrency)
    asc_register_test(NAME "asc_cpp.dense_lapack.triangular_conversion.${_scalar}.${_mode}"
      TARGET asc_triangular_conversion_test ARGUMENTS "${_scalar}" "${_mode}"
      LABELS dense_lapack lapack numerical validation triangular_conversion TIMEOUT 120)
  endforeach()
endforeach()
add_executable(asc_triangular_conversion_abi_probe triangular_conversion_abi_probe.cc)
_asc_dense_lapack_configure_test(asc_triangular_conversion_abi_probe)
target_include_directories(asc_triangular_conversion_abi_probe PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
asc_register_test(NAME asc_cpp.dense_lapack.triangular_conversion.abi
  TARGET asc_triangular_conversion_abi_probe
  LABELS dense_lapack lapack abi triangular_conversion TIMEOUT 120)
foreach(_kind IN ITEMS fault validation)
  add_executable("asc_triangular_conversion_${_kind}_test"
    "triangular_conversion_${_kind}_test.cc" triangular_conversion_faults.cc
    allocation_audit.cc "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
  _asc_dense_lapack_configure_test("asc_triangular_conversion_${_kind}_test")
  target_include_directories("asc_triangular_conversion_${_kind}_test" PRIVATE
    "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
  target_link_options("asc_triangular_conversion_${_kind}_test" PRIVATE
    "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free"
    "-Wl,--wrap=dlat2s_,--wrap=zlat2c_")
  foreach(_scalar IN ITEMS d z)
    asc_register_test(NAME "asc_cpp.dense_lapack.triangular_conversion.${_kind}.${_scalar}"
      TARGET "asc_triangular_conversion_${_kind}_test" ARGUMENTS "${_scalar}"
      LABELS dense_lapack lapack validation triangular_conversion TIMEOUT 120)
  endforeach()
endforeach()
