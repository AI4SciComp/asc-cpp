find_package(Threads REQUIRED)
add_executable(asc_positive_tridiagonal_expert_test
  positive_tridiagonal_expert_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_positive_tridiagonal_expert_test)
target_link_libraries(asc_positive_tridiagonal_expert_test PRIVATE Threads::Threads)
target_include_directories(asc_positive_tridiagonal_expert_test PRIVATE
  "${PROJECT_SOURCE_DIR}/src/dense/lapack"
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
foreach(_allocator IN ITEMS malloc calloc realloc aligned_alloc posix_memalign free)
  target_link_options(asc_positive_tridiagonal_expert_test PRIVATE
    "-Wl,--wrap=${_allocator}")
endforeach()
foreach(_scalar IN ITEMS s d c z)
  asc_register_test(NAME "asc_cpp.dense_lapack.positive_tridiagonal_expert.${_scalar}"
    TARGET asc_positive_tridiagonal_expert_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack positive_tridiagonal_expert numerical validation
      allocation concurrency TIMEOUT 120)
  asc_register_test(NAME "asc_cpp.dense_lapack.positive_tridiagonal_expert.required_math_${_scalar}"
    TARGET asc_positive_tridiagonal_expert_test ARGUMENTS "${_scalar}" extreme
    LABELS dense_lapack lapack positive_tridiagonal_expert numerical TIMEOUT 120)
endforeach()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  foreach(_kind IN ITEMS fault observation)
    set(_target "asc_positive_tridiagonal_expert_${_kind}_test")
    add_executable("${_target}" "positive_tridiagonal_expert_${_kind}_test.cc"
      positive_tridiagonal_expert_entry.cc)
    _asc_dense_lapack_configure_test("${_target}")
    target_include_directories("${_target}" PRIVATE
      "${PROJECT_SOURCE_DIR}/src/dense/lapack"
      "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
    target_link_options("${_target}" PRIVATE
      "-Wl,--wrap=sptsvx_,--wrap=dptsvx_,--wrap=cptsvx_,--wrap=zptsvx_")
    # Finite observer controls use the existing ASan timeout margin.
    set(_ptsvx_timeout 120)
    if(_kind STREQUAL "observation")
      set(_ptsvx_timeout 240)
      # All 1848 controls are retained. Both actual-ABI ASan runs exceeded
      # 240 seconds; budget their fork instrumentation cost separately.
      if(ASC_CPP_ENABLE_ADDRESS_SANITIZER)
        set(_ptsvx_timeout 1800)
      endif()
    endif()
    asc_register_test(NAME "asc_cpp.dense_lapack.positive_tridiagonal_expert.${_kind}"
      TARGET "${_target}" LABELS dense_lapack lapack positive_tridiagonal_expert
        validation "${_kind}" TIMEOUT "${_ptsvx_timeout}")
  endforeach()
endif()

add_executable(asc_positive_tridiagonal_expert_public_example
  "${PROJECT_SOURCE_DIR}/examples/positive_tridiagonal_expert/main.cc")
_asc_dense_lapack_configure_test(asc_positive_tridiagonal_expert_public_example)
asc_register_test(NAME asc_cpp.dense_lapack.positive_tridiagonal_expert.public_example
  TARGET asc_positive_tridiagonal_expert_public_example
  LABELS dense_lapack lapack positive_tridiagonal_expert public_api TIMEOUT 120)
