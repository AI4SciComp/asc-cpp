function(_asc_expression_configure_test target)
  target_link_libraries("${target}" PRIVATE ASC::expression)
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

add_executable(asc_expression_expression_test expression_test.cc)
target_include_directories(
  asc_expression_expression_test
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
)
_asc_expression_configure_test(asc_expression_expression_test)
asc_register_test(
  NAME asc_cpp.expression.expression
  TARGET asc_expression_expression_test
  LABELS "expression;milestone-2;runtime"
)

add_executable(
  asc_expression_adapter_contract
  "${PROJECT_SOURCE_DIR}/tests/compile/m2_expression_adapter_contract.cc"
)
_asc_expression_configure_test(asc_expression_adapter_contract)
asc_register_test(
  NAME asc_cpp.expression.adapter_contract
  TARGET asc_expression_adapter_contract
  LABELS "compile;expression;milestone-2"
)

add_executable(
  asc_expression_multi_tu
  "${PROJECT_SOURCE_DIR}/tests/compile/m2_expression_multi_tu_a.cc"
  "${PROJECT_SOURCE_DIR}/tests/compile/m2_expression_multi_tu_b.cc"
  "${PROJECT_SOURCE_DIR}/tests/compile/m2_expression_multi_tu_main.cc"
)
target_include_directories(
  asc_expression_multi_tu
  PRIVATE "${PROJECT_SOURCE_DIR}/tests/compile"
)
_asc_expression_configure_test(asc_expression_multi_tu)
asc_register_test(
  NAME asc_cpp.expression.multi_tu
  TARGET asc_expression_multi_tu
  LABELS "compile;expression;milestone-2"
)

set(_asc_expression_negative_sources
  m2_expression_negative_direct_binary.cc
  m2_expression_negative_direct_unary.cc
  m2_expression_negative_operation.cc
  m2_expression_negative_reference_rvalue.cc
  m2_expression_negative_unadapted.cc
)
foreach(_source IN LISTS _asc_expression_negative_sources)
  get_filename_component(_name "${_source}" NAME_WE)
  add_test(
    NAME "asc_cpp.expression.${_name}"
    COMMAND
      "${CMAKE_COMMAND}"
      "-DCXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}"
      "-DCXX_COMPILER_ID:STRING=${CMAKE_CXX_COMPILER_ID}"
      "-DCXX_COMPILER_FRONTEND_VARIANT:STRING=${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}"
      "-DINCLUDE_DIR:PATH=${PROJECT_SOURCE_DIR}/include"
      "-DSOURCE:FILEPATH=${PROJECT_SOURCE_DIR}/tests/compile/${_source}"
      "-DOUTPUT_DIR:PATH=${CMAKE_CURRENT_BINARY_DIR}/negative"
      "-DTEST_NAME:STRING=${_name}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_compile_failure.cmake"
  )
  set_tests_properties(
    "asc_cpp.expression.${_name}"
    PROPERTIES
      LABELS "compile;expression;milestone-2;negative"
      TIMEOUT 30
  )
endforeach()

unset(_asc_expression_negative_sources)
