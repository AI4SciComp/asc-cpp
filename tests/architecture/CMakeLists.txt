set(_architecture_script_arguments
  "-DSOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
  "-DDEPENDENCY_MANIFEST:FILEPATH=${PROJECT_SOURCE_DIR}/docs/development/asc-cpp-architecture/dependency-manifest.yaml"
  "-DCAPABILITY_MANIFEST:FILEPATH=${PROJECT_SOURCE_DIR}/docs/development/asc-cpp-architecture/capability-manifest.yaml"
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
    "-DENABLE_CUDA:BOOL=${ASC_CPP_ENABLE_CUDA}"
    "-DTARGET_INVENTORY:FILEPATH=${CMAKE_CURRENT_BINARY_DIR}/observed-targets-$<CONFIG>.txt"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_approved_product_targets.cmake"
)

add_test(
  NAME asc_cpp.architecture.public_file_policy
  COMMAND
    "${CMAKE_COMMAND}"
    "-DSOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_public_file_policy.cmake"
)

set_tests_properties(
  asc_cpp.architecture.dependency_manifest
  asc_cpp.architecture.approved_product_targets
  asc_cpp.architecture.public_file_policy
  PROPERTIES LABELS "architecture;milestone-6;milestone-7"
)

unset(_architecture_script_arguments)
