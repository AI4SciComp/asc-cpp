set(
  ASC_CPP_ASC_XDE_REPOSITORY
  "${PROJECT_SOURCE_DIR}/../asc-xde"
  CACHE PATH "Read-only asc-xde repository used by the M8 downstream trial"
)

if(NOT EXISTS "${ASC_CPP_ASC_XDE_REPOSITORY}/.git")
  add_test(
    NAME asc_cpp.downstream.asc_xde.skipped
    COMMAND
      "${CMAKE_COMMAND}" -E echo
      "skipped: asc-xde repository is unavailable at ${ASC_CPP_ASC_XDE_REPOSITORY}"
  )
  set_tests_properties(
    asc_cpp.downstream.asc_xde.skipped
    PROPERTIES
      LABELS "hardening;package;consumer;downstream;milestone-8"
      SKIP_REGULAR_EXPRESSION "^skipped:"
  )
  return()
endif()

set(_m8_downstream_common_arguments
  "-DGENERATOR:STRING=${CMAKE_GENERATOR}"
  "-DGENERATOR_PLATFORM:STRING=${CMAKE_GENERATOR_PLATFORM}"
  "-DGENERATOR_TOOLSET:STRING=${CMAKE_GENERATOR_TOOLSET}"
  "-DMAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}"
  "-DCONFIG:STRING=$<CONFIG>"
  "-DASC_XDE_REPOSITORY:PATH=${ASC_CPP_ASC_XDE_REPOSITORY}"
)

function(_asc_cpp_add_m8_downstream_test name package_directory fixture)
  add_test(
    NAME "asc_cpp.downstream.asc_xde.${name}"
    COMMAND
      "${CMAKE_COMMAND}"
      ${_m8_downstream_common_arguments}
      "-DPACKAGE_DIR:PATH=${package_directory}"
      "-DWORK_DIR:PATH=${CMAKE_CURRENT_BINARY_DIR}/${name} work"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/run_asc_xde_trial.cmake"
  )
  set_tests_properties(
    "asc_cpp.downstream.asc_xde.${name}"
    PROPERTIES
      LABELS "hardening;package;consumer;downstream;milestone-8"
      RUN_SERIAL TRUE
      TIMEOUT 180
  )
  if(fixture)
    set_tests_properties(
      "asc_cpp.downstream.asc_xde.${name}"
      PROPERTIES FIXTURES_REQUIRED "${fixture}"
    )
  endif()
endfunction()

_asc_cpp_add_m8_downstream_test(
  build_tree "${ASC_CPP_BUILD_PACKAGE_DIR}" ""
)
_asc_cpp_add_m8_downstream_test(
  copied_build_tree
  "${ASC_CPP_M8_COPIED_BUILD_PACKAGE}"
  M8CopiedBuildPackage
)
_asc_cpp_add_m8_downstream_test(
  installed
  "${ASC_CPP_M8_INSTALL_PACKAGE}"
  M8InstalledPackage
)
_asc_cpp_add_m8_downstream_test(
  relocated
  "${ASC_CPP_M8_RELOCATED_PACKAGE}"
  M8RelocatedPackage
)

unset(_m8_downstream_common_arguments)
