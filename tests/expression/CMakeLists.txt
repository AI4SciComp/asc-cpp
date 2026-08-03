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
  LABELS "expression;foundations;runtime"
)

add_executable(
  asc_expression_compile_contracts
  "${PROJECT_SOURCE_DIR}/tests/compile/m2_expression_compile_contracts.cc"
)
_asc_expression_configure_test(asc_expression_compile_contracts)
asc_register_test(
  NAME asc_cpp.expression.compile_contracts
  TARGET asc_expression_compile_contracts
  LABELS "compile;expression;foundations"
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
  LABELS "compile;expression;foundations"
)

foreach(_negative_source IN ITEMS
    m2_expression_negative_no_adapter.cc
    m2_expression_negative_wrong_rank.cc
)
  get_filename_component(_negative_name "${_negative_source}" NAME_WE)
  try_compile(
    _negative_compiled
    "${CMAKE_CURRENT_BINARY_DIR}/negative/${_negative_name}"
    SOURCES "${PROJECT_SOURCE_DIR}/tests/compile/${_negative_source}"
    CMAKE_FLAGS
      "-DCMAKE_CXX_EXTENSIONS:BOOL=OFF"
      "-DCMAKE_CXX_STANDARD:STRING=20"
      "-DCMAKE_CXX_STANDARD_REQUIRED:BOOL=ON"
      "-DINCLUDE_DIRECTORIES:STRING=${PROJECT_SOURCE_DIR}/include;${PROJECT_SOURCE_DIR}/tests/compile"
    OUTPUT_VARIABLE _negative_output
  )
  if(_negative_compiled)
    message(FATAL_ERROR
      "Expected ${_negative_source} to fail compilation.\n"
      "${_negative_output}"
    )
  endif()
endforeach()

unset(_negative_compiled)
unset(_negative_name)
unset(_negative_output)
unset(_negative_source)
