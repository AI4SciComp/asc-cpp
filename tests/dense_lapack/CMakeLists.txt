function(_asc_dense_lapack_configure_test target)
  target_link_libraries("${target}" PRIVATE ASC::dense_lapack)
  asc_target_enable_cxx20(TARGET "${target}")
  if(ASC_CPP_WARNINGS_AS_ERRORS)
    asc_target_enable_warnings(TARGET "${target}" AS_ERRORS)
  else()
    asc_target_enable_warnings(TARGET "${target}")
  endif()
  if(ASC_CPP_SANITIZER_ARGUMENTS)
    asc_target_enable_sanitizers(TARGET "${target}" ${ASC_CPP_SANITIZER_ARGUMENTS})
  endif()
endfunction()

add_executable(asc_dense_lapack_reference_lu_test
  reference_lu_test.cc allocation_audit.cc fault_injection.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_reference_lu_test)
target_include_directories(asc_dense_lapack_reference_lu_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_reference_lu_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free,--wrap=dgetrf_")
foreach(_scalar IN ITEMS s d c z)
  asc_register_test(NAME "asc_cpp.dense_lapack.reference_lu.${_scalar}"
    TARGET asc_dense_lapack_reference_lu_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack runtime numerical allocation TIMEOUT 120)
endforeach()

if(ASC_CPP_INSTALL)
  string(REPLACE ";" "|" _lapack_runtimes "${ASC_CPP_LAPACK_RUNTIME_LIBRARIES}")
  add_test(NAME asc_cpp.dense_lapack.package_integration
    COMMAND "${CMAKE_COMMAND}"
      "-DSOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
      "-DPRODUCER_BINARY_DIR:PATH=${PROJECT_BINARY_DIR}"
      "-DWORK_DIR:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}/reference provider package"
      "-DASC_CPP_TEST_WORKSPACE_ROOT:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}"
      "-DASC_CPP_TEST_WORKSPACE_GUARD:FILEPATH=${ASC_CPP_TEST_WORKSPACE_GUARD}"
      "-DLAPACK_ROOT:PATH=${ASC_CPP_LAPACK_ROOT}"
      "-DLAPACK_RUNTIMES:STRING=${_lapack_runtimes}"
      "-DCXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}"
      "-DCONFIG:STRING=$<CONFIG>"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/package_integration_test.cmake")
  set_tests_properties(asc_cpp.dense_lapack.package_integration PROPERTIES
    LABELS "dense_lapack;lapack;package;consumer;isolation"
    RUN_SERIAL TRUE TIMEOUT 300)
endif()

string(REPLACE ";" "|" _lapack_runtimes "${ASC_CPP_LAPACK_RUNTIME_LIBRARIES}")
add_test(NAME asc_cpp.dense_lapack.configuration_gates
  COMMAND "${CMAKE_COMMAND}"
    "-DSOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
    "-DWORK_DIR:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}/reference configuration gates"
    "-DASC_CPP_TEST_WORKSPACE_ROOT:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}"
    "-DASC_CPP_TEST_WORKSPACE_GUARD:FILEPATH=${ASC_CPP_TEST_WORKSPACE_GUARD}"
    "-DASCCMAKE_DIR:PATH=${ASCCMake_DIR}"
    "-DCXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}"
    "-DFORTRAN_COMPILER:FILEPATH=${CMAKE_Fortran_COMPILER}"
    "-DFORTRAN_FLAGS:STRING=${CMAKE_Fortran_FLAGS}"
    "-DINTEGER_BITS:STRING=${ASC_CPP_LAPACK_INTEGER_BITS}"
    "-DLAPACK_ROOT:PATH=${ASC_CPP_LAPACK_ROOT}"
    "-DLAPACK_ATTESTATION:FILEPATH=${ASC_CPP_LAPACK_ATTESTATION}"
    "-DLAPACK_RUNTIMES:STRING=${_lapack_runtimes}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/configuration_gate_test.cmake")
set_tests_properties(asc_cpp.dense_lapack.configuration_gates PROPERTIES
  LABELS "dense_lapack;lapack;package;configuration;isolation"
  RUN_SERIAL TRUE TIMEOUT 240)

foreach(_header IN ITEMS lapack.h lapack_export.h)
  string(MAKE_C_IDENTIFIER "${_header}" _id)
  foreach(_mode IN ITEMS normal no_exceptions)
    set(_target "asc_dense_lapack_header_${_id}_${_mode}")
    add_executable("${_target}"
      "${PROJECT_SOURCE_DIR}/tests/compile/header_self_containment.cc")
    _asc_dense_lapack_configure_test("${_target}")
    target_compile_definitions("${_target}" PRIVATE
      "ASC_TEST_HEADER=\"asc/dense/providers/${_header}\"")
    if(_mode STREQUAL "no_exceptions")
      target_compile_options("${_target}" PRIVATE -fno-exceptions)
    endif()
    asc_register_test(NAME "asc_cpp.dense_lapack.header.${_id}.${_mode}"
      TARGET "${_target}" LABELS dense_lapack lapack compile)
  endforeach()
endforeach()
