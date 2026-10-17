# Checked GEDMDQ resources and real-provider contract tests.
add_executable(asc_dmd_qr_counts_test dmd_qr_counts_test.cc)
_asc_dense_lapack_configure_test(asc_dmd_qr_counts_test)
target_include_directories(asc_dmd_qr_counts_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
asc_register_test(NAME asc_cpp.dense_lapack.dmd_qr.counts
  TARGET asc_dmd_qr_counts_test LABELS dense_lapack lapack dmd_qr validation
  TIMEOUT 120)

# Integrated checked adapter; required mathematical failures remain failures.
add_executable(asc_dmd_qr_test dmd_qr_test.cc)
_asc_dense_lapack_configure_test(asc_dmd_qr_test)
target_include_directories(asc_dmd_qr_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
foreach(_scalar IN ITEMS s d c z)
  asc_register_test(NAME "asc_cpp.dense_lapack.dmd_qr.required_math_${_scalar}"
    TARGET asc_dmd_qr_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack dmd_qr numerical required_math TIMEOUT 120)
endforeach()

add_executable(asc_dmd_qr_validation_test dmd_qr_validation_test.cc dmd_qr_entry.cc
  allocation_audit.cc "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dmd_qr_validation_test)
target_include_directories(asc_dmd_qr_validation_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
foreach(_allocator IN ITEMS malloc calloc realloc aligned_alloc posix_memalign free)
  target_link_options(asc_dmd_qr_validation_test PRIVATE "-Wl,--wrap=${_allocator}")
endforeach()
target_link_options(asc_dmd_qr_validation_test PRIVATE
  "-Wl,--wrap=sgedmdq_,--wrap=dgedmdq_,--wrap=cgedmdq_,--wrap=zgedmdq_")
foreach(_scalar IN ITEMS s d c z)
  asc_register_test(NAME "asc_cpp.dense_lapack.dmd_qr.validation_${_scalar}"
    TARGET asc_dmd_qr_validation_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack dmd_qr validation TIMEOUT 120)
endforeach()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(asc_dmd_qr_observation_test dmd_qr_observation_test.cc dmd_qr_entry.cc
    allocation_audit.cc "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
  _asc_dense_lapack_configure_test(asc_dmd_qr_observation_test)
  target_include_directories(asc_dmd_qr_observation_test PRIVATE
    "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
  foreach(_allocator IN ITEMS malloc calloc realloc aligned_alloc posix_memalign free)
    target_link_options(asc_dmd_qr_observation_test PRIVATE "-Wl,--wrap=${_allocator}")
  endforeach()
  target_link_options(asc_dmd_qr_observation_test PRIVATE
    "-Wl,--wrap=sgedmdq_,--wrap=dgedmdq_,--wrap=cgedmdq_,--wrap=zgedmdq_")
  asc_register_test(NAME asc_cpp.dense_lapack.dmd_qr.observation
    TARGET asc_dmd_qr_observation_test
    LABELS dense_lapack lapack dmd_qr observation validation TIMEOUT 120)
endif()

add_executable(asc_dmd_qr_contract_test dmd_qr_contract_test.cc
  allocation_audit.cc "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dmd_qr_contract_test)
target_link_libraries(asc_dmd_qr_contract_test PRIVATE Threads::Threads)
target_include_directories(asc_dmd_qr_contract_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
foreach(_allocator IN ITEMS malloc calloc realloc aligned_alloc posix_memalign free)
  target_link_options(asc_dmd_qr_contract_test PRIVATE "-Wl,--wrap=${_allocator}")
endforeach()
foreach(_scalar IN ITEMS s d c z)
  asc_register_test(NAME "asc_cpp.dense_lapack.dmd_qr.contract_${_scalar}"
    TARGET asc_dmd_qr_contract_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack dmd_qr validation concurrency TIMEOUT 120)
endforeach()

add_executable(asc_dmd_qr_public_example "${PROJECT_SOURCE_DIR}/examples/dmd_qr/main.cc")
_asc_dense_lapack_configure_test(asc_dmd_qr_public_example)
asc_register_test(NAME asc_cpp.dense_lapack.dmd_qr.public_example
  TARGET asc_dmd_qr_public_example
  LABELS dense_lapack lapack dmd_qr public_api TIMEOUT 120)
