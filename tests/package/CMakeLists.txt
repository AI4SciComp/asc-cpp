if(NOT DEFINED ASC_CPP_BUILD_PACKAGE_DIR)
  message(FATAL_ERROR
    "The root project must expose ASC_CPP_BUILD_PACKAGE_DIR to package tests."
  )
endif()
if(NOT DEFINED ASCCMake_DIR)
  message(FATAL_ERROR "ASCCMake_DIR is required for the registry-isolation test.")
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
)

add_test(
  NAME asc_cpp.package.build_tree_components
  COMMAND
    "${CMAKE_COMMAND}"
    ${_package_test_common_arguments}
    "-DACTION:STRING=VERIFY_BUILD_TREE"
    "-DPACKAGE_DIR:PATH=${ASC_CPP_BUILD_PACKAGE_DIR}"
    "-DWORK_DIR:PATH=${CMAKE_CURRENT_BINARY_DIR}/build tree package work"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/package_test.cmake"
)

add_test(
  NAME asc_cpp.package.install_and_relocate_components
  COMMAND
    "${CMAKE_COMMAND}"
    ${_package_test_common_arguments}
    "-DACTION:STRING=INSTALL_AND_RELOCATE"
    "-DPACKAGE_DIR:PATH=${ASC_CPP_BUILD_PACKAGE_DIR}"
    "-DWORK_DIR:PATH=${CMAKE_CURRENT_BINARY_DIR}/install and relocate package work"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/package_test.cmake"
)

add_test(
  NAME asc_cpp.package.registry_unchanged
  COMMAND
    "${CMAKE_COMMAND}"
    ${_package_test_common_arguments}
    "-DACTION:STRING=VERIFY_REGISTRY"
    "-DPACKAGE_DIR:PATH=${ASC_CPP_BUILD_PACKAGE_DIR}"
    "-DWORK_DIR:PATH=${CMAKE_CURRENT_BINARY_DIR}/registry isolation work"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/package_test.cmake"
)

add_test(
  NAME asc_cpp.package.cuda_unavailable
  COMMAND
    "${CMAKE_COMMAND}"
    ${_package_test_common_arguments}
    "-DWORK_DIR:PATH=${CMAKE_CURRENT_BINARY_DIR}/cuda unavailable work"
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

add_test(
  NAME asc_cpp.package.repeated_component_lookup
  COMMAND
    "${CMAKE_COMMAND}"
    -S "${CMAKE_CURRENT_SOURCE_DIR}/repeated_components"
    -B "${CMAKE_CURRENT_BINARY_DIR}/repeated component lookup work"
    ${_repeated_component_generator_arguments}
    "-DASCCpp_DIR:PATH=${ASC_CPP_BUILD_PACKAGE_DIR}"
    "-DENABLE_CUDA:BOOL=${ASC_CPP_ENABLE_CUDA}"
    "-DCMAKE_MAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}"
    "-DCMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY:BOOL=ON"
    "-DCMAKE_FIND_USE_PACKAGE_REGISTRY:BOOL=OFF"
    "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY:BOOL=OFF"
)
add_test(
  NAME asc_cpp.package.repeated_component_lookup_build
  COMMAND
    "${CMAKE_COMMAND}" --build
    "${CMAKE_CURRENT_BINARY_DIR}/repeated component lookup work"
    --config "$<CONFIG>"
)
set_tests_properties(
  asc_cpp.package.repeated_component_lookup_build
  PROPERTIES
    DEPENDS asc_cpp.package.repeated_component_lookup
    LABELS "package;hardening;milestone-8"
    RUN_SERIAL TRUE
    TIMEOUT 180
)

if(NOT ASC_CPP_ENABLE_CUDA)
  add_test(
    NAME asc_cpp.package.cuda_disabled_isolation
    COMMAND
      "${CMAKE_COMMAND}"
      "-DPRODUCER_BINARY_DIR:PATH=${PROJECT_BINARY_DIR}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/check_cuda_disabled_isolation.cmake"
  )
endif()

set_tests_properties(
  asc_cpp.package.build_tree_components
  asc_cpp.package.install_and_relocate_components
  asc_cpp.package.registry_unchanged
  asc_cpp.package.cuda_unavailable
  asc_cpp.package.repeated_component_lookup
  PROPERTIES
    LABELS "package;hardening;milestone-6;milestone-7;milestone-8"
    RUN_SERIAL TRUE
    TIMEOUT 600
)
if(NOT ASC_CPP_ENABLE_CUDA)
  set_tests_properties(
    asc_cpp.package.cuda_disabled_isolation
    PROPERTIES LABELS "architecture;package;milestone-6;milestone-7"
  )
endif()

unset(_repeated_component_generator_arguments)
unset(_package_test_common_arguments)
