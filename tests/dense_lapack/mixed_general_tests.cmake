find_package(Threads REQUIRED)
add_executable(asc_mixed_general_test mixed_general_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_mixed_general_test)
target_link_libraries(asc_mixed_general_test PRIVATE Threads::Threads)
foreach(_allocator IN ITEMS malloc calloc realloc aligned_alloc posix_memalign free)
  target_link_options(asc_mixed_general_test PRIVATE "-Wl,--wrap=${_allocator}")
endforeach()
foreach(_scalar IN ITEMS d z)
  asc_register_test(NAME "asc_cpp.dense_lapack.mixed_general.${_scalar}"
    TARGET asc_mixed_general_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack mixed_general numerical validation allocation TIMEOUT 120)
  asc_register_test(NAME "asc_cpp.dense_lapack.mixed_general.required_math_${_scalar}"
    TARGET asc_mixed_general_test ARGUMENTS "${_scalar}" extreme
    LABELS dense_lapack lapack mixed_general numerical TIMEOUT 120)
endforeach()

add_executable(asc_mixed_general_fault_test mixed_general_fault_test.cc
  mixed_general_faults.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_mixed_general_fault_test)
target_include_directories(asc_mixed_general_fault_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
foreach(_allocator IN ITEMS malloc calloc realloc aligned_alloc posix_memalign free)
  target_link_options(asc_mixed_general_fault_test PRIVATE "-Wl,--wrap=${_allocator}")
endforeach()
target_link_options(asc_mixed_general_fault_test PRIVATE "-Wl,--wrap=dsgesv_,--wrap=zcgesv_")
foreach(_scalar IN ITEMS d z)
  asc_register_test(NAME "asc_cpp.dense_lapack.mixed_general.fault_${_scalar}"
    TARGET asc_mixed_general_fault_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack mixed_general validation TIMEOUT 120)
endforeach()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(asc_mixed_general_observation_test mixed_general_observation_test.cc
    mixed_general_faults.cc allocation_audit.cc
    "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
  _asc_dense_lapack_configure_test(asc_mixed_general_observation_test)
  target_include_directories(asc_mixed_general_observation_test PRIVATE
    "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
  foreach(_allocator IN ITEMS malloc calloc realloc aligned_alloc posix_memalign free)
    target_link_options(asc_mixed_general_observation_test PRIVATE "-Wl,--wrap=${_allocator}")
  endforeach()
  target_link_options(asc_mixed_general_observation_test PRIVATE "-Wl,--wrap=dsgesv_,--wrap=zcgesv_")
  asc_register_test(NAME asc_cpp.dense_lapack.mixed_general.observation
    TARGET asc_mixed_general_observation_test
    LABELS dense_lapack lapack mixed_general observation validation TIMEOUT 120)
endif()

add_executable(asc_mixed_general_public_example "${PROJECT_SOURCE_DIR}/examples/mixed_general/main.cc")
_asc_dense_lapack_configure_test(asc_mixed_general_public_example)
asc_register_test(NAME asc_cpp.dense_lapack.mixed_general.public_example
  TARGET asc_mixed_general_public_example
  LABELS dense_lapack lapack mixed_general public_api TIMEOUT 120)
