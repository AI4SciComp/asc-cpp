# Checked GEDMD resources and real-provider execution.
foreach(_variant IN ITEMS "" _native)
  add_executable("asc_dmd_counts${_variant}_test" "dmd_counts${_variant}_test.cc")
  _asc_dense_lapack_configure_test("asc_dmd_counts${_variant}_test")
  target_include_directories("asc_dmd_counts${_variant}_test" PRIVATE
    "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
  asc_register_test(NAME "asc_cpp.dense_lapack.dmd.minimum_counts${_variant}"
    TARGET "asc_dmd_counts${_variant}_test"
    LABELS dense_lapack lapack dmd validation TIMEOUT 120)
endforeach()
add_executable(asc_dmd_test dmd_test.cc
  allocation_audit.cc "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dmd_test)
target_link_libraries(asc_dmd_test PRIVATE Threads::Threads)
target_include_directories(asc_dmd_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
foreach(_allocator IN ITEMS malloc calloc realloc aligned_alloc posix_memalign free)
  target_link_options(asc_dmd_test PRIVATE "-Wl,--wrap=${_allocator}")
endforeach()
foreach(_scalar IN ITEMS s d c z)
  asc_register_test(NAME "asc_cpp.dense_lapack.dmd.candidate_${_scalar}"
    TARGET asc_dmd_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack dmd numerical validation allocation concurrency TIMEOUT 120)
endforeach()

add_executable(asc_dmd_validation_test dmd_validation_test.cc dmd_entry.cc
  allocation_audit.cc "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dmd_validation_test)
target_include_directories(asc_dmd_validation_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
foreach(_allocator IN ITEMS malloc calloc realloc aligned_alloc posix_memalign free)
  target_link_options(asc_dmd_validation_test PRIVATE "-Wl,--wrap=${_allocator}")
endforeach()
target_link_options(asc_dmd_validation_test PRIVATE
  "-Wl,--wrap=sgedmd_,--wrap=dgedmd_,--wrap=cgedmd_,--wrap=zgedmd_")
foreach(_scalar IN ITEMS s d c z)
  asc_register_test(NAME "asc_cpp.dense_lapack.dmd.validation_${_scalar}"
    TARGET asc_dmd_validation_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack dmd validation TIMEOUT 120)
endforeach()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(asc_dmd_observation_test dmd_observation_test.cc dmd_entry.cc
      allocation_audit.cc "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
  _asc_dense_lapack_configure_test(asc_dmd_observation_test)
  target_include_directories(asc_dmd_observation_test PRIVATE
    "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
  foreach(_allocator IN ITEMS malloc calloc realloc aligned_alloc posix_memalign free)
    target_link_options(asc_dmd_observation_test PRIVATE "-Wl,--wrap=${_allocator}")
  endforeach()
  target_link_options(asc_dmd_observation_test PRIVATE
    "-Wl,--wrap=sgedmd_,--wrap=dgedmd_,--wrap=cgedmd_,--wrap=zgedmd_")
  asc_register_test(NAME asc_cpp.dense_lapack.dmd.observation
    TARGET asc_dmd_observation_test
    LABELS dense_lapack lapack dmd observation validation TIMEOUT 120)
endif()

add_executable(asc_dmd_public_example "${PROJECT_SOURCE_DIR}/examples/dmd/main.cc")
_asc_dense_lapack_configure_test(asc_dmd_public_example)
asc_register_test(NAME asc_cpp.dense_lapack.dmd.public_example
  TARGET asc_dmd_public_example
  LABELS dense_lapack lapack dmd public_api TIMEOUT 120)
