option(
  ASC_CPP_ENABLE_ASC_XDE_REPOSITORY_AUDIT
  "Audit an explicitly selected real asc-xde checkout during hardening tests"
  OFF
)
set(
  ASC_CPP_ASC_XDE_REPOSITORY
  ""
  CACHE PATH "Explicit read-only asc-xde checkout for the optional hardening audit"
)
set(
  ASC_CPP_ASC_XDE_EXPECTED_COMMIT
  ""
  CACHE STRING "Exact asc-xde commit required by the optional hardening audit"
)
if(ASC_CPP_ENABLE_ASC_XDE_REPOSITORY_AUDIT)
  if(NOT IS_DIRECTORY "${ASC_CPP_ASC_XDE_REPOSITORY}/.git")
    message(FATAL_ERROR
      "ASC_CPP_ENABLE_ASC_XDE_REPOSITORY_AUDIT requires an explicit "
      "ASC_CPP_ASC_XDE_REPOSITORY Git checkout"
    )
  endif()
  if(ASC_CPP_ASC_XDE_EXPECTED_COMMIT STREQUAL "")
    message(FATAL_ERROR
      "ASC_CPP_ENABLE_ASC_XDE_REPOSITORY_AUDIT requires "
      "ASC_CPP_ASC_XDE_EXPECTED_COMMIT"
    )
  endif()
endif()

function(
  _asc_cpp_add_m8_downstream_test
  name
  package_directory
  fixture
  audit_repository
)
  set(_command
    "${CMAKE_COMMAND}"
    "-DASCCPP_PACKAGE_DIR:PATH=${package_directory}"
    "-DWORK_DIR:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}/downstream ${name}"
    "-DASC_CPP_TEST_WORKSPACE_ROOT:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}"
    "-DASC_CPP_TEST_WORKSPACE_GUARD:FILEPATH=${ASC_CPP_TEST_WORKSPACE_GUARD}"
    "-DTRIAL_MODE:STRING=${name}"
    "-DTEST_GENERATOR:STRING=${CMAKE_GENERATOR}"
    "-DCXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}"
    "-DTEST_CONFIGURATION:STRING=$<CONFIG>"
  )
  if(audit_repository)
    list(APPEND _command
      "-DASC_XDE_SOURCE_DIR:PATH=${ASC_CPP_ASC_XDE_REPOSITORY}"
      "-DEXPECTED_ASC_XDE_COMMIT:STRING=${ASC_CPP_ASC_XDE_EXPECTED_COMMIT}"
    )
  endif()
  list(APPEND _command
    -P "${CMAKE_CURRENT_SOURCE_DIR}/run_asc_xde_trial.cmake"
  )
  add_test(
    NAME "asc_cpp.downstream.asc_xde.${name}"
    COMMAND ${_command}
  )
  set_tests_properties(
    "asc_cpp.downstream.asc_xde.${name}"
    PROPERTIES
      LABELS "hardening;package;consumer;downstream"
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
  build_tree "${ASC_CPP_BUILD_PACKAGE_DIR}" "" FALSE
)
_asc_cpp_add_m8_downstream_test(
  copied_build_tree
  "${ASC_CPP_M8_COPIED_BUILD_PACKAGE}"
  M8CopiedBuildPackage
  FALSE
)
_asc_cpp_add_m8_downstream_test(
  installed
  "${ASC_CPP_M8_INSTALL_PACKAGE}"
  M8InstalledPackage
  FALSE
)
_asc_cpp_add_m8_downstream_test(
  relocated
  "${ASC_CPP_M8_RELOCATED_PACKAGE}"
  M8RelocatedPackage
  FALSE
)
if(ASC_CPP_ENABLE_ASC_XDE_REPOSITORY_AUDIT)
  _asc_cpp_add_m8_downstream_test(
    repository_audit
    "${ASC_CPP_BUILD_PACKAGE_DIR}"
    ""
    TRUE
  )
endif()
