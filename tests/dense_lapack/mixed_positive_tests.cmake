find_package(Threads REQUIRED)
add_executable(asc_mixed_positive_test mixed_positive_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_mixed_positive_test)
target_link_libraries(asc_mixed_positive_test PRIVATE Threads::Threads)
foreach(_allocator IN ITEMS malloc calloc realloc aligned_alloc posix_memalign free)
  target_link_options(asc_mixed_positive_test PRIVATE "-Wl,--wrap=${_allocator}")
endforeach()
foreach(_scalar IN ITEMS d z)
  asc_register_test(NAME "asc_cpp.dense_lapack.mixed_positive.${_scalar}"
    TARGET asc_mixed_positive_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack mixed_positive numerical validation allocation TIMEOUT 120)
  asc_register_test(NAME "asc_cpp.dense_lapack.mixed_positive.required_math_${_scalar}"
    TARGET asc_mixed_positive_test ARGUMENTS "${_scalar}" extreme
    LABELS dense_lapack lapack mixed_positive numerical TIMEOUT 120)
endforeach()

add_executable(asc_mixed_positive_fault_test mixed_positive_fault_test.cc
  mixed_positive_faults.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_mixed_positive_fault_test)
target_include_directories(asc_mixed_positive_fault_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
foreach(_allocator IN ITEMS malloc calloc realloc aligned_alloc posix_memalign free)
  target_link_options(asc_mixed_positive_fault_test PRIVATE "-Wl,--wrap=${_allocator}")
endforeach()
target_link_options(asc_mixed_positive_fault_test PRIVATE "-Wl,--wrap=dsposv_,--wrap=zcposv_")
foreach(_scalar IN ITEMS d z)
  asc_register_test(NAME "asc_cpp.dense_lapack.mixed_positive.fault_${_scalar}"
    TARGET asc_mixed_positive_fault_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack mixed_positive validation TIMEOUT 120)
endforeach()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(asc_mixed_positive_observation_test mixed_positive_observation_test.cc
    mixed_positive_faults.cc allocation_audit.cc
    "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
  _asc_dense_lapack_configure_test(asc_mixed_positive_observation_test)
  target_include_directories(asc_mixed_positive_observation_test PRIVATE
    "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
  foreach(_allocator IN ITEMS malloc calloc realloc aligned_alloc posix_memalign free)
    target_link_options(asc_mixed_positive_observation_test PRIVATE "-Wl,--wrap=${_allocator}")
  endforeach()
  target_link_options(asc_mixed_positive_observation_test PRIVATE "-Wl,--wrap=dsposv_,--wrap=zcposv_")
  asc_register_test(NAME asc_cpp.dense_lapack.mixed_positive.observation
    TARGET asc_mixed_positive_observation_test
    LABELS dense_lapack lapack mixed_positive observation validation TIMEOUT 120)
endif()

add_executable(asc_mixed_positive_public_example "${PROJECT_SOURCE_DIR}/examples/mixed_positive/main.cc")
_asc_dense_lapack_configure_test(asc_mixed_positive_public_example)
asc_register_test(NAME asc_cpp.dense_lapack.mixed_positive.public_example
  TARGET asc_mixed_positive_public_example
  LABELS dense_lapack lapack mixed_positive public_api TIMEOUT 120)
