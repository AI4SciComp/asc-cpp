if(NOT DEFINED ASC_CPP_BUILD_PACKAGE_DIR)
  message(FATAL_ERROR
    "The root project must expose ASC_CPP_BUILD_PACKAGE_DIR to package tests."
  )
endif()
if(NOT DEFINED ASCCMake_DIR)
  message(FATAL_ERROR "ASCCMake_DIR is required for the registry test.")
endif()

set(_package_test_common_arguments
  "-DSOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
  "-DPRODUCER_BINARY_DIR:PATH=${PROJECT_BINARY_DIR}"
  "-DCONSUMER_SOURCE_DIR:PATH=${CMAKE_CURRENT_SOURCE_DIR}/component_unavailable"
  "-DGENERATOR:STRING=${CMAKE_GENERATOR}"
  "-DGENERATOR_PLATFORM:STRING=${CMAKE_GENERATOR_PLATFORM}"
  "-DGENERATOR_TOOLSET:STRING=${CMAKE_GENERATOR_TOOLSET}"
  "-DMAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}"
  "-DCONFIG:STRING=$<CONFIG>"
  "-DASCCMAKE_DIR:PATH=${ASCCMake_DIR}"
  "-DENABLE_CUDA:BOOL=${ASC_CPP_ENABLE_CUDA}"
    "-DENABLE_LAPACK:BOOL=${ASC_CPP_ENABLE_LAPACK}"
  "-DCUDA_ARCHITECTURES:STRING=${CMAKE_CUDA_ARCHITECTURES}"
  "-DASC_CPP_TEST_WORKSPACE_ROOT:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}"
  "-DASC_CPP_TEST_WORKSPACE_GUARD:FILEPATH=${ASC_CPP_TEST_WORKSPACE_GUARD}"
)

add_test(
  NAME asc_cpp.package.build_tree_components
  COMMAND
    "${CMAKE_COMMAND}"
    ${_package_test_common_arguments}
    "-DACTION:STRING=VERIFY_BUILD_TREE"
    "-DPACKAGE_DIR:PATH=${ASC_CPP_BUILD_PACKAGE_DIR}"
    "-DWORK_DIR:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}/package build tree"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/package_test.cmake"
)

add_test(
  NAME asc_cpp.package.install_and_relocate_components
  COMMAND
    "${CMAKE_COMMAND}"
    ${_package_test_common_arguments}
    "-DACTION:STRING=INSTALL_AND_RELOCATE"
    "-DWORK_DIR:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}/package install relocate"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/package_test.cmake"
)

add_test(
  NAME asc_cpp.package.registry_unchanged
  COMMAND
    "${CMAKE_COMMAND}"
    ${_package_test_common_arguments}
    "-DACTION:STRING=VERIFY_REGISTRY"
    "-DWORK_DIR:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}/package registry isolation"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/package_test.cmake"
)

add_test(
  NAME asc_cpp.package.cuda_disabled_isolation
  COMMAND
    "${CMAKE_COMMAND}"
    ${_package_test_common_arguments}
    "-DWORK_DIR:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}/package cuda disabled isolation"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_cuda_disabled_isolation.cmake"
)

add_test(
  NAME asc_cpp.package.cuda_requested_unavailable
  COMMAND
    "${CMAKE_COMMAND}"
    ${_package_test_common_arguments}
    "-DWORK_DIR:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}/package cuda unavailable"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_cuda_unavailable.cmake"
)

set(_repeated_component_generator_arguments -G "${CMAKE_GENERATOR}")
if(CMAKE_GENERATOR_PLATFORM)
  list(APPEND _repeated_component_generator_arguments
    -A "${CMAKE_GENERATOR_PLATFORM}"
  )
endif()
if(CMAKE_GENERATOR_TOOLSET)
  list(APPEND _repeated_component_generator_arguments
    -T "${CMAKE_GENERATOR_TOOLSET}"
  )
endif()
if(CMAKE_MAKE_PROGRAM)
  list(APPEND _repeated_component_generator_arguments
    "-DCMAKE_MAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}"
  )
endif()

set(_repeated_component_work_dir
  "${ASC_CPP_TEST_WORKSPACE_ROOT}/package repeated component lookup"
)
add_test(
  NAME asc_cpp.package.repeated_component_lookup_clean
  COMMAND
    "${CMAKE_COMMAND}"
    "-DWORK_DIR:PATH=${_repeated_component_work_dir}"
    "-DROOT:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}"
    "-DGUARD:FILEPATH=${ASC_CPP_TEST_WORKSPACE_GUARD}"
    -DVALIDATE_ONLY:BOOL=OFF
    -P
    "${PROJECT_SOURCE_DIR}/tests/cmake/test_workspace_guard_probe.cmake"
)
add_test(
  NAME asc_cpp.package.repeated_component_lookup
  COMMAND
    "${CMAKE_COMMAND}"
    -S "${CMAKE_CURRENT_SOURCE_DIR}/repeated_components"
    -B "${_repeated_component_work_dir}"
    ${_repeated_component_generator_arguments}
    "-DASCCpp_DIR:PATH=${ASC_CPP_BUILD_PACKAGE_DIR}"
    "-DENABLE_CUDA:BOOL=${ASC_CPP_ENABLE_CUDA}"
    "-DENABLE_LAPACK:BOOL=${ASC_CPP_ENABLE_LAPACK}"
    "-DCMAKE_FIND_USE_PACKAGE_REGISTRY:BOOL=OFF"
    "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY:BOOL=OFF"
)
add_test(
  NAME asc_cpp.package.repeated_component_lookup_build
  COMMAND
    "${CMAKE_COMMAND}" --build
    "${_repeated_component_work_dir}"
    --config "$<CONFIG>"
)
set_tests_properties(
  asc_cpp.package.repeated_component_lookup_build
  PROPERTIES
    DEPENDS asc_cpp.package.repeated_component_lookup
    FIXTURES_REQUIRED M8RepeatedComponentLookup
    LABELS "package;hardening"
    RUN_SERIAL TRUE
    TIMEOUT 180
)

set_tests_properties(
  asc_cpp.package.build_tree_components
  asc_cpp.package.install_and_relocate_components
  asc_cpp.package.registry_unchanged
  asc_cpp.package.cuda_disabled_isolation
  asc_cpp.package.cuda_requested_unavailable
  asc_cpp.package.repeated_component_lookup_clean
  asc_cpp.package.repeated_component_lookup
  PROPERTIES
    LABELS "package;cuda-sparse-random;hardening"
    RUN_SERIAL TRUE
    TIMEOUT 600
)
set_tests_properties(
  asc_cpp.package.repeated_component_lookup_clean
  PROPERTIES FIXTURES_SETUP M8RepeatedComponentLookupClean
)
set_tests_properties(
  asc_cpp.package.repeated_component_lookup
  PROPERTIES
    FIXTURES_REQUIRED M8RepeatedComponentLookupClean
    FIXTURES_SETUP M8RepeatedComponentLookup
)

unset(_repeated_component_generator_arguments)
unset(_repeated_component_work_dir)
unset(_package_test_common_arguments)
