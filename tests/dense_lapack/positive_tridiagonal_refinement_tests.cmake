find_package(Threads REQUIRED)
add_executable(asc_positive_tridiagonal_refinement_test
  positive_tridiagonal_refinement_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_positive_tridiagonal_refinement_test)
target_link_libraries(asc_positive_tridiagonal_refinement_test PRIVATE Threads::Threads)
target_include_directories(asc_positive_tridiagonal_refinement_test PRIVATE
  "${PROJECT_SOURCE_DIR}/src/dense/lapack"
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
foreach(_allocator IN ITEMS malloc calloc realloc aligned_alloc posix_memalign free)
  target_link_options(asc_positive_tridiagonal_refinement_test PRIVATE
    "-Wl,--wrap=${_allocator}")
endforeach()
foreach(_scalar IN ITEMS s d c z)
  asc_register_test(NAME "asc_cpp.dense_lapack.positive_tridiagonal_refinement.${_scalar}"
    TARGET asc_positive_tridiagonal_refinement_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack positive_tridiagonal_refinement numerical validation
      allocation concurrency TIMEOUT 120)
  asc_register_test(NAME "asc_cpp.dense_lapack.positive_tridiagonal_refinement.required_math_${_scalar}"
    TARGET asc_positive_tridiagonal_refinement_test ARGUMENTS "${_scalar}" extreme
    LABELS dense_lapack lapack positive_tridiagonal_refinement numerical TIMEOUT 120)
endforeach()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  foreach(_kind IN ITEMS fault observation)
    set(_target "asc_positive_tridiagonal_refinement_${_kind}_test")
    add_executable("${_target}" "positive_tridiagonal_refinement_${_kind}_test.cc"
      positive_tridiagonal_refinement_entry.cc)
    _asc_dense_lapack_configure_test("${_target}")
    target_include_directories("${_target}" PRIVATE
      "${PROJECT_SOURCE_DIR}/src/dense/lapack"
      "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
    target_link_options("${_target}" PRIVATE
      "-Wl,--wrap=sptrfs_,--wrap=dptrfs_,--wrap=cptrfs_,--wrap=zptrfs_")
    # The 128 fork-based calibration controls took 119.5 seconds under ASan
    # on the admitted local profile. Keep every control with a finite margin.
    set(_ptrfs_timeout 120)
    if(_kind STREQUAL "observation")
      set(_ptrfs_timeout 240)
    endif()
    asc_register_test(NAME "asc_cpp.dense_lapack.positive_tridiagonal_refinement.${_kind}"
      TARGET "${_target}" LABELS dense_lapack lapack positive_tridiagonal_refinement
        validation "${_kind}" TIMEOUT "${_ptrfs_timeout}")
  endforeach()
endif()

add_executable(asc_positive_tridiagonal_refinement_public_example
  "${PROJECT_SOURCE_DIR}/examples/positive_tridiagonal_refinement/main.cc")
_asc_dense_lapack_configure_test(asc_positive_tridiagonal_refinement_public_example)
asc_register_test(NAME asc_cpp.dense_lapack.positive_tridiagonal_refinement.public_example
  TARGET asc_positive_tridiagonal_refinement_public_example
  LABELS dense_lapack lapack positive_tridiagonal_refinement public_api TIMEOUT 120)
