find_package(Threads REQUIRED)
add_executable(asc_matrix_set_test matrix_set_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_matrix_set_test)
target_link_libraries(asc_matrix_set_test PRIVATE Threads::Threads)
foreach(_allocator IN ITEMS malloc calloc realloc aligned_alloc posix_memalign free)
  target_link_options(asc_matrix_set_test PRIVATE "-Wl,--wrap=${_allocator}")
endforeach()
foreach(_scalar IN ITEMS s d c z)
  asc_register_test(NAME "asc_cpp.dense_lapack.matrix_set.${_scalar}"
    TARGET asc_matrix_set_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack matrix_set numerical validation allocation
      concurrency TIMEOUT 120)
endforeach()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(asc_matrix_set_observation_test matrix_set_observation_test.cc
    matrix_set_entry.cc allocation_audit.cc
    "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
  _asc_dense_lapack_configure_test(asc_matrix_set_observation_test)
  target_include_directories(asc_matrix_set_observation_test PRIVATE
    "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
  foreach(_allocator IN ITEMS malloc calloc realloc aligned_alloc posix_memalign free)
    target_link_options(asc_matrix_set_observation_test PRIVATE "-Wl,--wrap=${_allocator}")
  endforeach()
  target_link_options(asc_matrix_set_observation_test PRIVATE
    "-Wl,--wrap=slaset_,--wrap=dlaset_,--wrap=claset_,--wrap=zlaset_")
  asc_register_test(NAME asc_cpp.dense_lapack.matrix_set.observation
    TARGET asc_matrix_set_observation_test
    LABELS dense_lapack lapack matrix_set observation validation TIMEOUT 120)
endif()

add_executable(asc_matrix_set_public_example
  "${PROJECT_SOURCE_DIR}/examples/matrix_set/main.cc")
_asc_dense_lapack_configure_test(asc_matrix_set_public_example)
asc_register_test(NAME asc_cpp.dense_lapack.matrix_set.public_example
  TARGET asc_matrix_set_public_example
  LABELS dense_lapack lapack matrix_set public_api TIMEOUT 120)
