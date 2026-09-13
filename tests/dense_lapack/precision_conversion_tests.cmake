find_package(Threads REQUIRED)
add_executable(asc_precision_conversion_test precision_conversion_test.cc
  allocation_audit.cc "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_precision_conversion_test)
target_link_libraries(asc_precision_conversion_test PRIVATE Threads::Threads)
foreach(_allocator IN ITEMS malloc calloc realloc aligned_alloc posix_memalign free)
  target_link_options(asc_precision_conversion_test PRIVATE "-Wl,--wrap=${_allocator}")
endforeach()
foreach(_scalar IN ITEMS s d c z)
  asc_register_test(NAME "asc_cpp.dense_lapack.precision_conversion.${_scalar}"
    TARGET asc_precision_conversion_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack precision_conversion numerical validation allocation
      concurrency TIMEOUT 120)
endforeach()
add_executable(asc_precision_conversion_fault_test precision_conversion_fault_test.cc
  precision_conversion_faults.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_precision_conversion_fault_test)
target_include_directories(asc_precision_conversion_fault_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
foreach(_allocator IN ITEMS malloc calloc realloc aligned_alloc posix_memalign free)
  target_link_options(asc_precision_conversion_fault_test PRIVATE "-Wl,--wrap=${_allocator}")
endforeach()
target_link_options(asc_precision_conversion_fault_test PRIVATE
  "-Wl,--wrap=slag2d_,--wrap=dlag2s_,--wrap=clag2z_,--wrap=zlag2c_")
foreach(_scalar IN ITEMS s d c z)
  asc_register_test(NAME "asc_cpp.dense_lapack.precision_conversion.fault_${_scalar}"
    TARGET asc_precision_conversion_fault_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack precision_conversion validation TIMEOUT 120)
endforeach()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(asc_precision_conversion_observation_test
    precision_conversion_observation_test.cc precision_conversion_faults.cc
    allocation_audit.cc "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
  _asc_dense_lapack_configure_test(asc_precision_conversion_observation_test)
  target_include_directories(asc_precision_conversion_observation_test PRIVATE
    "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
  foreach(_allocator IN ITEMS malloc calloc realloc aligned_alloc posix_memalign free)
    target_link_options(asc_precision_conversion_observation_test PRIVATE "-Wl,--wrap=${_allocator}")
  endforeach()
  target_link_options(asc_precision_conversion_observation_test PRIVATE
    "-Wl,--wrap=slag2d_,--wrap=dlag2s_,--wrap=clag2z_,--wrap=zlag2c_")
  asc_register_test(NAME asc_cpp.dense_lapack.precision_conversion.observation
    TARGET asc_precision_conversion_observation_test
    LABELS dense_lapack lapack precision_conversion observation validation TIMEOUT 120)
endif()
add_executable(asc_precision_conversion_public_example
  "${PROJECT_SOURCE_DIR}/examples/precision_conversion/main.cc")
_asc_dense_lapack_configure_test(asc_precision_conversion_public_example)
asc_register_test(NAME asc_cpp.dense_lapack.precision_conversion.public_example
  TARGET asc_precision_conversion_public_example
  LABELS dense_lapack lapack precision_conversion public_api TIMEOUT 120)
