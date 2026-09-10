find_package(Threads REQUIRED)
add_executable(asc_general_lu_public_example
  "${PROJECT_SOURCE_DIR}/examples/general_lu/main.cc")
_asc_dense_lapack_configure_test(asc_general_lu_public_example)
target_link_libraries(asc_general_lu_public_example PRIVATE Threads::Threads)
asc_register_test(NAME asc_cpp.dense_lapack.general_lu.public_modes
  TARGET asc_general_lu_public_example
  LABELS dense_lapack lapack general_lu public_api numerical concurrency TIMEOUT 120)
