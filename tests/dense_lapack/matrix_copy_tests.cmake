find_package(Threads REQUIRED)
add_executable(asc_matrix_copy_test matrix_copy_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_matrix_copy_test)
target_link_libraries(asc_matrix_copy_test PRIVATE Threads::Threads)
foreach(_allocator IN ITEMS malloc calloc realloc aligned_alloc posix_memalign free)
  target_link_options(asc_matrix_copy_test PRIVATE "-Wl,--wrap=${_allocator}")
endforeach()
foreach(_scalar IN ITEMS s d c z)
  asc_register_test(NAME "asc_cpp.dense_lapack.matrix_copy.${_scalar}"
    TARGET asc_matrix_copy_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack matrix_copy numerical validation allocation
      concurrency TIMEOUT 120)
endforeach()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(asc_matrix_copy_observation_test matrix_copy_observation_test.cc
    matrix_copy_entry.cc allocation_audit.cc
    "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
  _asc_dense_lapack_configure_test(asc_matrix_copy_observation_test)
  target_include_directories(asc_matrix_copy_observation_test PRIVATE
    "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
  foreach(_allocator IN ITEMS malloc calloc realloc aligned_alloc posix_memalign free)
    target_link_options(asc_matrix_copy_observation_test PRIVATE "-Wl,--wrap=${_allocator}")
  endforeach()
  target_link_options(asc_matrix_copy_observation_test PRIVATE
    "-Wl,--wrap=slacpy_,--wrap=dlacpy_,--wrap=clacpy_,--wrap=zlacpy_")
  asc_register_test(NAME asc_cpp.dense_lapack.matrix_copy.observation
    TARGET asc_matrix_copy_observation_test
    LABELS dense_lapack lapack matrix_copy observation validation TIMEOUT 120)
endif()

add_executable(asc_matrix_copy_public_example
  "${PROJECT_SOURCE_DIR}/examples/matrix_copy/main.cc")
_asc_dense_lapack_configure_test(asc_matrix_copy_public_example)
asc_register_test(NAME asc_cpp.dense_lapack.matrix_copy.public_example
  TARGET asc_matrix_copy_public_example
  LABELS dense_lapack lapack matrix_copy public_api TIMEOUT 120)
