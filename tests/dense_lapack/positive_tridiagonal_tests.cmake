find_package(Threads REQUIRED)
add_executable(asc_positive_tridiagonal_test positive_tridiagonal_test.cc
  allocation_audit.cc "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_positive_tridiagonal_test)
target_link_libraries(asc_positive_tridiagonal_test PRIVATE Threads::Threads)
foreach(_allocator IN ITEMS malloc calloc realloc aligned_alloc posix_memalign free)
  target_link_options(asc_positive_tridiagonal_test PRIVATE "-Wl,--wrap=${_allocator}")
endforeach()
foreach(_scalar IN ITEMS s d c z)
  asc_register_test(NAME "asc_cpp.dense_lapack.positive_tridiagonal.${_scalar}"
    TARGET asc_positive_tridiagonal_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack positive_tridiagonal numerical validation allocation TIMEOUT 120)
  asc_register_test(NAME "asc_cpp.dense_lapack.positive_tridiagonal.required_math_${_scalar}"
    TARGET asc_positive_tridiagonal_test ARGUMENTS "${_scalar}" extreme
    LABELS dense_lapack lapack positive_tridiagonal numerical TIMEOUT 120)
endforeach()
add_executable(asc_positive_tridiagonal_fault_test positive_tridiagonal_fault_test.cc
  positive_tridiagonal_faults.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_positive_tridiagonal_fault_test)
target_include_directories(asc_positive_tridiagonal_fault_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
foreach(_allocator IN ITEMS malloc calloc realloc aligned_alloc posix_memalign free)
  target_link_options(asc_positive_tridiagonal_fault_test PRIVATE "-Wl,--wrap=${_allocator}")
endforeach()
foreach(_scalar IN ITEMS s d c z)
  foreach(_routine IN ITEMS pttrf pttrs)
    target_link_options(asc_positive_tridiagonal_fault_test PRIVATE "-Wl,--wrap=${_scalar}${_routine}_")
  endforeach()
  asc_register_test(NAME "asc_cpp.dense_lapack.positive_tridiagonal.fault_${_scalar}"
    TARGET asc_positive_tridiagonal_fault_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack positive_tridiagonal validation TIMEOUT 120)
endforeach()
add_executable(asc_positive_tridiagonal_native_comparison_test
  positive_tridiagonal_native_comparison_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_positive_tridiagonal_native_comparison_test)
target_include_directories(asc_positive_tridiagonal_native_comparison_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
foreach(_allocator IN ITEMS malloc calloc realloc aligned_alloc posix_memalign free)
  target_link_options(asc_positive_tridiagonal_native_comparison_test PRIVATE "-Wl,--wrap=${_allocator}")
endforeach()
asc_register_test(NAME asc_cpp.dense_lapack.positive_tridiagonal.native_comparison
  TARGET asc_positive_tridiagonal_native_comparison_test
  LABELS dense_lapack lapack positive_tridiagonal provider_fidelity TIMEOUT 120)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(asc_positive_tridiagonal_observation_test
    positive_tridiagonal_observation_test.cc allocation_audit.cc
    "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
  _asc_dense_lapack_configure_test(asc_positive_tridiagonal_observation_test)
  foreach(_allocator IN ITEMS malloc calloc realloc aligned_alloc posix_memalign free)
    target_link_options(asc_positive_tridiagonal_observation_test PRIVATE "-Wl,--wrap=${_allocator}")
  endforeach()
  asc_register_test(NAME asc_cpp.dense_lapack.positive_tridiagonal.observation
    TARGET asc_positive_tridiagonal_observation_test
    LABELS dense_lapack lapack positive_tridiagonal validation observation TIMEOUT 120)
endif()
add_executable(asc_positive_tridiagonal_public_example "${PROJECT_SOURCE_DIR}/examples/positive_tridiagonal/main.cc")
_asc_dense_lapack_configure_test(asc_positive_tridiagonal_public_example)
asc_register_test(NAME asc_cpp.dense_lapack.positive_tridiagonal.public_example
  TARGET asc_positive_tridiagonal_public_example
  LABELS dense_lapack lapack positive_tridiagonal public_api TIMEOUT 120)
