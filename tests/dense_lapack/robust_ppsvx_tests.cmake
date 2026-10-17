# The four Reference PPSVX candidates are deliberately not registered here.
# These tests exercise the explicitly selected original first-party algorithm.
find_package(Threads REQUIRED)
add_executable(asc_robust_ppsvx_math robust_packed_cholesky_expert_math_test.cc)
_asc_dense_lapack_configure_test(asc_robust_ppsvx_math)
target_include_directories(asc_robust_ppsvx_math PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/installed_lu")
foreach(_scalar IN ITEMS s d c z)
  asc_register_test(NAME "asc_cpp.dense_lapack.robust_ppsvx.required_math_${_scalar}"
    TARGET asc_robust_ppsvx_math ARGUMENTS "${_scalar}"
    LABELS dense_lapack robust_ppsvx experimental numerical TIMEOUT 120)
endforeach()

add_executable(asc_robust_ppsvx_consumer
  installed_lu/robust_packed_cholesky_expert_main.cc)
_asc_dense_lapack_configure_test(asc_robust_ppsvx_consumer)
target_link_libraries(asc_robust_ppsvx_consumer PRIVATE Threads::Threads)
foreach(_mode IN ITEMS ordinary concurrent general-s general-d general-c general-z)
  asc_register_test(NAME "asc_cpp.dense_lapack.robust_ppsvx.${_mode}"
    TARGET asc_robust_ppsvx_consumer ARGUMENTS "--${_mode}"
    LABELS dense_lapack robust_ppsvx experimental numerical TIMEOUT 120)
endforeach()

add_executable(asc_robust_ppsvx_safety
  installed_lu/robust_packed_cholesky_expert_main.cc
  allocation_audit.cc "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_robust_ppsvx_safety)
target_compile_definitions(asc_robust_ppsvx_safety PRIVATE
  ASC_ROBUST_PPSVX_ALLOCATION)
target_include_directories(asc_robust_ppsvx_safety PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/tests/dense")
target_link_libraries(asc_robust_ppsvx_safety PRIVATE Threads::Threads)
foreach(_allocator IN ITEMS malloc calloc realloc aligned_alloc posix_memalign free)
  target_link_options(asc_robust_ppsvx_safety PRIVATE "-Wl,--wrap=${_allocator}")
endforeach()
asc_register_test(NAME asc_cpp.dense_lapack.robust_ppsvx.safety
  TARGET asc_robust_ppsvx_safety ARGUMENTS --safety
  LABELS dense_lapack robust_ppsvx experimental validation allocation TIMEOUT 120)
