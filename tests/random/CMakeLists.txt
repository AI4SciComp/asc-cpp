function(_asc_random_configure_test target)
  target_link_libraries("${target}" PRIVATE ASC::random)
  target_include_directories("${target}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  asc_target_enable_cxx20(TARGET "${target}")
  if(ASC_CPP_WARNINGS_AS_ERRORS)
    asc_target_enable_warnings(TARGET "${target}" AS_ERRORS)
  else()
    asc_target_enable_warnings(TARGET "${target}")
  endif()
  if(ASC_CPP_SANITIZER_ARGUMENTS)
    asc_target_enable_sanitizers(
      TARGET "${target}"
      ${ASC_CPP_SANITIZER_ARGUMENTS}
    )
  endif()
endfunction()

foreach(_source IN ITEMS
    distribution_test.cc
    engine_test.cc
    generator_test.cc
    seed_test.cc
    stateful_engine_test.cc
    statistical_test.cc
)
  get_filename_component(_name "${_source}" NAME_WE)
  set(_target "asc_random_${_name}")
  add_executable("${_target}" "${_source}")
  _asc_random_configure_test("${_target}")
  asc_register_test(
    NAME "asc_cpp.random.${_name}"
    TARGET "${_target}"
    LABELS "milestone-2;milestone-13;random;runtime"
  )
endforeach()

add_executable(
  asc_random_quasi_test
  quasi_test.cc
  "${PROJECT_SOURCE_DIR}/benchmarks/random_storage/allocation_probe.cc"
)
target_include_directories(
  asc_random_quasi_test
  PRIVATE "${PROJECT_SOURCE_DIR}/benchmarks/random_storage"
)
target_link_libraries(asc_random_quasi_test PRIVATE Threads::Threads)
_asc_random_configure_test(asc_random_quasi_test)
asc_register_test(
  NAME asc_cpp.random.quasi_test
  TARGET asc_random_quasi_test
  LABELS "allocation;concurrency;milestone-14;performance;random;runtime"
)

add_test(
  NAME asc_cpp.random.sobol_artifacts
  COMMAND
    "${CMAKE_COMMAND}"
    "-DSOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_sobol_artifacts.cmake"
)
set_tests_properties(
  asc_cpp.random.sobol_artifacts
  PROPERTIES LABELS "architecture;milestone-14;package;provenance;random"
)

add_executable(
  asc_random_generator_benchmark
  "${PROJECT_SOURCE_DIR}/benchmarks/random/random_benchmark.cc"
  "${PROJECT_SOURCE_DIR}/benchmarks/random_storage/allocation_probe.cc"
)
target_include_directories(
  asc_random_generator_benchmark
  PRIVATE "${PROJECT_SOURCE_DIR}/benchmarks/random_storage"
)
_asc_random_configure_test(asc_random_generator_benchmark)
asc_register_test(
  NAME asc_cpp.random.generator_benchmark
  TARGET asc_random_generator_benchmark
  LABELS "benchmark;milestone-13;milestone-14;performance;random"
)
