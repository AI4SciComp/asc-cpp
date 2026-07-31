set(_architecture_script_arguments
  "-DSOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
  "-DDEPENDENCY_MANIFEST:FILEPATH=${PROJECT_SOURCE_DIR}/docs/development/asc-cpp-architecture/dependency-manifest.yaml"
  "-DCAPABILITY_MANIFEST:FILEPATH=${PROJECT_SOURCE_DIR}/docs/development/asc-cpp-architecture/capability-manifest.yaml"
  "-DENABLE_CUDA:BOOL=${ASC_CPP_ENABLE_CUDA}"
)
set(_blas_coverage_manifest
  "${PROJECT_SOURCE_DIR}/docs/development/asc-cpp-architecture/blas-coverage.yaml"
)

add_test(
  NAME asc_cpp.architecture.dependency_manifest
  COMMAND
    "${CMAKE_COMMAND}"
    ${_architecture_script_arguments}
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_dependency_manifest.cmake"
)

add_test(
  NAME asc_cpp.architecture.approved_product_targets
  COMMAND
    "${CMAKE_COMMAND}"
    "-DSOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
    "-DTARGET_INVENTORY:FILEPATH=${CMAKE_CURRENT_BINARY_DIR}/observed-targets-$<CONFIG>.txt"
    "-DENABLE_CUDA:BOOL=${ASC_CPP_ENABLE_CUDA}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_approved_product_targets.cmake"
)

add_test(
  NAME asc_cpp.architecture.public_file_policy
  COMMAND
    "${CMAKE_COMMAND}"
    "-DSOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
    "-DENABLE_CUDA:BOOL=${ASC_CPP_ENABLE_CUDA}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_public_file_policy.cmake"
)

add_test(
  NAME asc_cpp.architecture.blas_coverage
  COMMAND
    "${CMAKE_COMMAND}"
    "-DSOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
    "-DBLAS_MANIFEST:FILEPATH=${_blas_coverage_manifest}"
    "-DOUTPUT_REPORT:FILEPATH=${CMAKE_CURRENT_BINARY_DIR}/blas-coverage.md"
    "-DEXPECTED_REPORT:FILEPATH=${PROJECT_SOURCE_DIR}/docs/blas-coverage.md"
    -P "${PROJECT_SOURCE_DIR}/cmake/GenerateBlasCoverage.cmake"
)

add_test(
  NAME asc_cpp.architecture.blas_coverage_failures
  COMMAND
    "${CMAKE_COMMAND}"
    "-DSOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
    "-DTEST_BINARY_DIR:PATH=${CMAKE_CURRENT_BINARY_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_blas_coverage_failures.cmake"
)

set_tests_properties(
  asc_cpp.architecture.dependency_manifest
  asc_cpp.architecture.approved_product_targets
  asc_cpp.architecture.public_file_policy
  asc_cpp.architecture.blas_coverage
  asc_cpp.architecture.blas_coverage_failures
  PROPERTIES LABELS "architecture;milestone-7;milestone-8;milestone-9"
)

unset(_blas_coverage_manifest)
unset(_architecture_script_arguments)
