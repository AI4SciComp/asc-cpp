set(_architecture_script_arguments
  "-DSOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
  "-DDEPENDENCY_MANIFEST:FILEPATH=${PROJECT_SOURCE_DIR}/docs/contracts/dependency-manifest.yaml"
  "-DCAPABILITY_MANIFEST:FILEPATH=${PROJECT_SOURCE_DIR}/docs/contracts/capability-manifest.yaml"
  "-DENABLE_CUDA:BOOL=${ASC_CPP_ENABLE_CUDA}"
)
set(_blas_coverage_manifest
  "${PROJECT_SOURCE_DIR}/docs/contracts/blas-coverage.yaml"
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

add_test(
  NAME asc_cpp.architecture.blas_completion
  COMMAND
    "${CMAKE_COMMAND}"
    "-DSOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
    "-DBINARY_DIR:PATH=${PROJECT_BINARY_DIR}"
    "-DENABLE_CUDA:BOOL=${ASC_CPP_ENABLE_CUDA}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_blas_completion.cmake"
)

add_test(
  NAME asc_cpp.architecture.random_contract
  COMMAND
    "${CMAKE_COMMAND}"
    "-DSOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
    "-DBINARY_DIR:PATH=${CMAKE_CURRENT_BINARY_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_random_contract.cmake"
)

set(_random_completion_supports_disabled_exceptions FALSE)
if(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$"
   AND NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
  set(_random_completion_supports_disabled_exceptions TRUE)
endif()
add_test(
  NAME asc_cpp.architecture.random_completion
  COMMAND
    "${CMAKE_COMMAND}"
    "-DSOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
    "-DBINARY_DIR:PATH=${PROJECT_BINARY_DIR}"
    "-DENABLE_CUDA:BOOL=${ASC_CPP_ENABLE_CUDA}"
    "-DSUPPORTS_DISABLED_EXCEPTIONS:BOOL=${_random_completion_supports_disabled_exceptions}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_random_completion.cmake"
)
unset(_random_completion_supports_disabled_exceptions)

set_tests_properties(
  asc_cpp.architecture.dependency_manifest
  asc_cpp.architecture.approved_product_targets
  asc_cpp.architecture.public_file_policy
  asc_cpp.architecture.blas_coverage
  asc_cpp.architecture.blas_coverage_failures
  asc_cpp.architecture.blas_completion
  asc_cpp.architecture.random_contract
  PROPERTIES LABELS
    "architecture;cuda-sparse-random;hardening;blas-level3;sparse-blas;blas-completion;random-contract"
)
set_tests_properties(
  asc_cpp.architecture.random_contract
  PROPERTIES LABELS
    "architecture;random-adapters;cuda-sparse-random;hardening;blas-level3;sparse-blas;blas-completion;random-contract"
)
set_tests_properties(
  asc_cpp.architecture.random_completion
  PROPERTIES LABELS "architecture;random-completion;random"
)

unset(_blas_coverage_manifest)
unset(_architecture_script_arguments)
