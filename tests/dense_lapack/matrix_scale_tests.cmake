find_package(Threads REQUIRED)
add_executable(asc_matrix_scale_test matrix_scale_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_matrix_scale_test)
target_include_directories(asc_matrix_scale_test PRIVATE
  "${PROJECT_SOURCE_DIR}/src/dense/lapack")
target_link_libraries(asc_matrix_scale_test PRIVATE Threads::Threads)
foreach(_allocator IN ITEMS malloc calloc realloc aligned_alloc posix_memalign free)
  target_link_options(asc_matrix_scale_test PRIVATE "-Wl,--wrap=${_allocator}")
endforeach()
foreach(_scalar IN ITEMS s d c z)
  asc_register_test(NAME "asc_cpp.dense_lapack.matrix_scale.${_scalar}"
    TARGET asc_matrix_scale_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack matrix_scale numerical validation allocation
      concurrency TIMEOUT 120)
endforeach()

foreach(_kind IN ITEMS fault exception)
  add_executable("asc_matrix_scale_${_kind}_test" "matrix_scale_${_kind}_test.cc")
  _asc_dense_lapack_configure_test("asc_matrix_scale_${_kind}_test")
  target_include_directories("asc_matrix_scale_${_kind}_test" PRIVATE
    "${PROJECT_SOURCE_DIR}/src/dense/lapack"
    "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
  foreach(_scalar IN ITEMS s d c z)
    asc_register_test(NAME "asc_cpp.dense_lapack.matrix_scale.${_kind}_${_scalar}"
      TARGET "asc_matrix_scale_${_kind}_test" ARGUMENTS "${_scalar}"
      LABELS dense_lapack lapack matrix_scale validation "${_kind}" TIMEOUT 120)
  endforeach()
endforeach()
target_sources(asc_matrix_scale_fault_test PRIVATE matrix_scale_entry.cc)
target_link_options(asc_matrix_scale_fault_test PRIVATE
  "-Wl,--wrap=slascl_,--wrap=dlascl_,--wrap=clascl_,--wrap=zlascl_")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(asc_matrix_scale_observation_test matrix_scale_observation_test.cc
    matrix_scale_entry.cc)
  _asc_dense_lapack_configure_test(asc_matrix_scale_observation_test)
  target_include_directories(asc_matrix_scale_observation_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/dense/lapack"
    "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
  target_link_options(asc_matrix_scale_observation_test PRIVATE
    "-Wl,--wrap=slascl_,--wrap=dlascl_,--wrap=clascl_,--wrap=zlascl_")
  asc_register_test(NAME asc_cpp.dense_lapack.matrix_scale.observation
    TARGET asc_matrix_scale_observation_test
    LABELS dense_lapack lapack matrix_scale observation validation TIMEOUT 120)
endif()

add_executable(asc_matrix_scale_public_example
  "${PROJECT_SOURCE_DIR}/examples/matrix_scale/main.cc")
_asc_dense_lapack_configure_test(asc_matrix_scale_public_example)
asc_register_test(NAME asc_cpp.dense_lapack.matrix_scale.public_example
  TARGET asc_matrix_scale_public_example
  LABELS dense_lapack lapack matrix_scale public_api TIMEOUT 120)
