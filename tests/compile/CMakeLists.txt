set(_asc_core_public_headers
  asc/core.h
  asc/core/array_format.h
  asc/core/configuration.h
  asc/core/contracts.h
  asc/core/execution.h
  asc/core/export.h
  asc/core/extents.h
  asc/core/io.h
  asc/core/memory.h
  asc/core/result.h
  asc/core/status.h
  asc/core/types.h
)

function(_asc_core_configure_compile_test target)
  target_link_libraries("${target}" PRIVATE ASC::core)
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

set(_asc_core_supports_disabled_exceptions FALSE)
if(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$"
   AND NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
  set(_asc_core_supports_disabled_exceptions TRUE)
endif()

foreach(_header IN LISTS _asc_core_public_headers)
  string(MAKE_C_IDENTIFIER "${_header}" _header_identifier)
  set(_target "asc_core_header_${_header_identifier}")
  add_executable("${_target}" header_self_containment.cc)
  target_compile_definitions(
    "${_target}"
    PRIVATE "ASC_TEST_HEADER=\"${_header}\""
  )
  _asc_core_configure_compile_test("${_target}")
  asc_register_test(
    NAME "asc_cpp.compile.header.${_header_identifier}"
    TARGET "${_target}"
    LABELS "compile;core"
  )

  if(_asc_core_supports_disabled_exceptions)
    set(_no_exceptions_target
      "asc_core_header_no_exceptions_${_header_identifier}"
    )
    add_executable(
      "${_no_exceptions_target}"
      header_self_containment_no_exceptions.cc
    )
    target_compile_definitions(
      "${_no_exceptions_target}"
      PRIVATE "ASC_TEST_HEADER=\"${_header}\""
    )
    _asc_core_configure_compile_test("${_no_exceptions_target}")
    target_compile_options("${_no_exceptions_target}" PRIVATE -fno-exceptions)
    asc_register_test(
      NAME "asc_cpp.compile.header_no_exceptions.${_header_identifier}"
      TARGET "${_no_exceptions_target}"
      LABELS "compile;core"
    )
  endif()
endforeach()

set(_asc_m2_public_headers
  asc/expression.h
  asc/expression/expression.h
  asc/random.h
  asc/random/distribution.h
  asc/random/engine.h
  asc/random/export.h
  asc/random/generator.h
  asc/random/quasi.h
  asc/random/seed.h
  asc/utilities.h
  asc/utilities/command_line.h
  asc/utilities/export.h
  asc/utilities/timer.h
)

foreach(_header IN LISTS _asc_m2_public_headers)
  if(_header MATCHES "^asc/utilities")
    set(_owner_target ASC::utilities)
  elseif(_header MATCHES "^asc/expression")
    set(_owner_target ASC::expression)
  elseif(_header MATCHES "^asc/random")
    set(_owner_target ASC::random)
  else()
    message(FATAL_ERROR "foundational modules header has no owning target: ${_header}")
  endif()

  string(MAKE_C_IDENTIFIER "${_header}" _header_identifier)
  set(_target "asc_m2_header_${_header_identifier}")
  add_executable("${_target}" header_self_containment.cc)
  target_compile_definitions(
    "${_target}"
    PRIVATE "ASC_TEST_HEADER=\"${_header}\""
  )
  target_link_libraries("${_target}" PRIVATE "${_owner_target}")
  asc_target_enable_cxx20(TARGET "${_target}")
  if(ASC_CPP_WARNINGS_AS_ERRORS)
    asc_target_enable_warnings(TARGET "${_target}" AS_ERRORS)
  else()
    asc_target_enable_warnings(TARGET "${_target}")
  endif()
  if(ASC_CPP_SANITIZER_ARGUMENTS)
    asc_target_enable_sanitizers(
      TARGET "${_target}"
      ${ASC_CPP_SANITIZER_ARGUMENTS}
    )
  endif()
  asc_register_test(
    NAME "asc_cpp.compile.m2_header.${_header_identifier}"
    TARGET "${_target}"
    LABELS "compile;foundations"
  )

  if(_asc_core_supports_disabled_exceptions)
    set(_no_exceptions_target
      "asc_m2_header_no_exceptions_${_header_identifier}"
    )
    add_executable(
      "${_no_exceptions_target}"
      header_self_containment_no_exceptions.cc
    )
    target_compile_definitions(
      "${_no_exceptions_target}"
      PRIVATE "ASC_TEST_HEADER=\"${_header}\""
    )
    target_link_libraries(
      "${_no_exceptions_target}"
      PRIVATE "${_owner_target}"
    )
    asc_target_enable_cxx20(TARGET "${_no_exceptions_target}")
    if(ASC_CPP_WARNINGS_AS_ERRORS)
      asc_target_enable_warnings(
        TARGET "${_no_exceptions_target}"
        AS_ERRORS
      )
    else()
      asc_target_enable_warnings(TARGET "${_no_exceptions_target}")
    endif()
    if(ASC_CPP_SANITIZER_ARGUMENTS)
      asc_target_enable_sanitizers(
        TARGET "${_no_exceptions_target}"
        ${ASC_CPP_SANITIZER_ARGUMENTS}
      )
    endif()
    target_compile_options("${_no_exceptions_target}" PRIVATE -fno-exceptions)
    asc_register_test(
      NAME "asc_cpp.compile.m2_header_no_exceptions.${_header_identifier}"
      TARGET "${_no_exceptions_target}"
      LABELS "compile;foundations"
    )
  endif()
endforeach()

if(_asc_core_supports_disabled_exceptions)
  add_executable(asc_m2_exceptions_disabled m2_exceptions_disabled.cc)
  target_link_libraries(
    asc_m2_exceptions_disabled
    PRIVATE
      ASC::utilities
      ASC::expression
      ASC::random
  )
  asc_target_enable_cxx20(TARGET asc_m2_exceptions_disabled)
  if(ASC_CPP_WARNINGS_AS_ERRORS)
    asc_target_enable_warnings(
      TARGET asc_m2_exceptions_disabled
      AS_ERRORS
    )
  else()
    asc_target_enable_warnings(TARGET asc_m2_exceptions_disabled)
  endif()
  if(ASC_CPP_SANITIZER_ARGUMENTS)
    asc_target_enable_sanitizers(
      TARGET asc_m2_exceptions_disabled
      ${ASC_CPP_SANITIZER_ARGUMENTS}
    )
  endif()
  target_compile_options(asc_m2_exceptions_disabled PRIVATE -fno-exceptions)
  asc_register_test(
    NAME asc_cpp.compile.m2_exceptions_disabled
    TARGET asc_m2_exceptions_disabled
    LABELS "compile;foundations"
  )
endif()

add_executable(
  asc_core_multi_tu
  multi_tu_a.cc
  multi_tu_b.cc
  multi_tu_main.cc
)
_asc_core_configure_compile_test(asc_core_multi_tu)
asc_register_test(
  NAME asc_cpp.compile.multi_tu
  TARGET asc_core_multi_tu
  LABELS "compile;core"
)

if(_asc_core_supports_disabled_exceptions)
  add_executable(asc_core_exceptions_disabled exceptions_disabled.cc)
  _asc_core_configure_compile_test(asc_core_exceptions_disabled)
  target_compile_options(
    asc_core_exceptions_disabled
    PRIVATE -fno-exceptions
  )
  asc_register_test(
    NAME asc_cpp.compile.exceptions_disabled
    TARGET asc_core_exceptions_disabled
    LABELS "compile;core"
  )
endif()

get_target_property(_asc_core_direct_links asc_core LINK_LIBRARIES)
if(_asc_core_direct_links)
  message(FATAL_ERROR
    "asc_core must have no direct target dependencies, found: "
    "${_asc_core_direct_links}"
  )
endif()
get_target_property(
  _asc_core_interface_links
  asc_core
  INTERFACE_LINK_LIBRARIES
)
if(_asc_core_interface_links)
  message(FATAL_ERROR
    "ASC::core must expose no target dependencies, found: "
    "${_asc_core_interface_links}"
  )
endif()

foreach(_asc_m2_target IN ITEMS asc_utilities asc_random)
  get_target_property(
    _asc_m2_direct_links
    "${_asc_m2_target}"
    LINK_LIBRARIES
  )
  if(NOT _asc_m2_direct_links STREQUAL "ASC::core")
    message(FATAL_ERROR
      "${_asc_m2_target} must link directly only to ASC::core, found: "
      "${_asc_m2_direct_links}"
    )
  endif()
  get_target_property(
    _asc_m2_interface_links
    "${_asc_m2_target}"
    INTERFACE_LINK_LIBRARIES
  )
  if(NOT _asc_m2_interface_links STREQUAL "ASC::core")
    message(FATAL_ERROR
      "${_asc_m2_target} must expose only ASC::core, found: "
      "${_asc_m2_interface_links}"
    )
  endif()
endforeach()

get_target_property(_asc_expression_type asc_expression TYPE)
if(NOT _asc_expression_type STREQUAL "INTERFACE_LIBRARY")
  message(FATAL_ERROR
    "asc_expression must be an interface library, found: "
    "${_asc_expression_type}"
  )
endif()
get_target_property(
  _asc_expression_links
  asc_expression
  INTERFACE_LINK_LIBRARIES
)
if(NOT _asc_expression_links STREQUAL "ASC::core")
  message(FATAL_ERROR
    "asc_expression must expose only ASC::core, found: "
    "${_asc_expression_links}"
  )
endif()

add_test(
  NAME asc_cpp.compile.dependency_audit
  COMMAND
    "${CMAKE_COMMAND}"
    "-DSOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/dependency_check.cmake"
)
set_tests_properties(
  asc_cpp.compile.dependency_audit
  PROPERTIES LABELS "architecture;compile;foundations"
)

add_test(
  NAME asc_cpp.compile.m2_dependency_audit
  COMMAND
    "${CMAKE_COMMAND}"
    "-DSOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/m2_dependency_check.cmake"
)
set_tests_properties(
  asc_cpp.compile.m2_dependency_audit
  PROPERTIES LABELS "architecture;compile;foundations"
)

set(_asc_m3_public_headers
  asc/dense.h
  asc/dense/array.h
  asc/dense/evaluate.h
  asc/dense/export.h
  asc/dense/layout.h
  asc/dense/blas.h
  asc/dense/view.h
  asc/dense/print.h
  asc/dense/lapack/types.h
  asc/dense/lapack/workspace.h
  asc/dense/lapack/report.h
  asc/dense/lapack/factor_view.h
  asc/dense/lapack/structured_view.h
)

foreach(_header IN LISTS _asc_m3_public_headers)
  string(MAKE_C_IDENTIFIER "${_header}" _header_identifier)
  set(_target "asc_m3_header_${_header_identifier}")
  add_executable("${_target}" header_self_containment.cc)
  target_compile_definitions(
    "${_target}"
    PRIVATE "ASC_TEST_HEADER=\"${_header}\""
  )
  target_link_libraries("${_target}" PRIVATE ASC::dense)
  asc_target_enable_cxx20(TARGET "${_target}")
  if(ASC_CPP_WARNINGS_AS_ERRORS)
    asc_target_enable_warnings(TARGET "${_target}" AS_ERRORS)
  else()
    asc_target_enable_warnings(TARGET "${_target}")
  endif()
  if(ASC_CPP_SANITIZER_ARGUMENTS)
    asc_target_enable_sanitizers(
      TARGET "${_target}"
      ${ASC_CPP_SANITIZER_ARGUMENTS}
    )
  endif()
  asc_register_test(
    NAME "asc_cpp.compile.m3_header.${_header_identifier}"
    TARGET "${_target}"
    LABELS "compile;dense"
  )

  if(_asc_core_supports_disabled_exceptions)
    set(_no_exceptions_target
      "asc_m3_header_no_exceptions_${_header_identifier}"
    )
    add_executable(
      "${_no_exceptions_target}"
      header_self_containment_no_exceptions.cc
    )
    target_compile_definitions(
      "${_no_exceptions_target}"
      PRIVATE "ASC_TEST_HEADER=\"${_header}\""
    )
    target_link_libraries("${_no_exceptions_target}" PRIVATE ASC::dense)
    asc_target_enable_cxx20(TARGET "${_no_exceptions_target}")
    if(ASC_CPP_WARNINGS_AS_ERRORS)
      asc_target_enable_warnings(
        TARGET "${_no_exceptions_target}"
        AS_ERRORS
      )
    else()
      asc_target_enable_warnings(TARGET "${_no_exceptions_target}")
    endif()
    if(ASC_CPP_SANITIZER_ARGUMENTS)
      asc_target_enable_sanitizers(
        TARGET "${_no_exceptions_target}"
        ${ASC_CPP_SANITIZER_ARGUMENTS}
      )
    endif()
    target_compile_options("${_no_exceptions_target}" PRIVATE -fno-exceptions)
    asc_register_test(
      NAME "asc_cpp.compile.m3_header_no_exceptions.${_header_identifier}"
      TARGET "${_no_exceptions_target}"
      LABELS "compile;dense"
    )
  endif()
endforeach()

foreach(_compile_source IN ITEMS
    m3_header_dense.cc
    m3_header_array.cc
    m3_header_evaluate.cc
    m3_header_export.cc
    m3_header_layout.cc
    m3_header_blas.cc
    m3_header_view.cc
)
  get_filename_component(_compile_name "${_compile_source}" NAME_WE)
  set(_target "asc_dense_${_compile_name}")
  add_executable("${_target}" "${_compile_source}")
  target_link_libraries("${_target}" PRIVATE ASC::dense)
  asc_target_enable_cxx20(TARGET "${_target}")
  if(ASC_CPP_WARNINGS_AS_ERRORS)
    asc_target_enable_warnings(TARGET "${_target}" AS_ERRORS)
  else()
    asc_target_enable_warnings(TARGET "${_target}")
  endif()
  if(ASC_CPP_SANITIZER_ARGUMENTS)
    asc_target_enable_sanitizers(
      TARGET "${_target}"
      ${ASC_CPP_SANITIZER_ARGUMENTS}
    )
  endif()
  asc_register_test(
    NAME "asc_cpp.compile.${_compile_name}"
    TARGET "${_target}"
    LABELS "compile;dense"
  )
endforeach()

if(_asc_core_supports_disabled_exceptions)
  add_executable(asc_m3_exceptions_disabled m3_exceptions_disabled.cc)
  target_link_libraries(asc_m3_exceptions_disabled PRIVATE ASC::dense)
  asc_target_enable_cxx20(TARGET asc_m3_exceptions_disabled)
  if(ASC_CPP_WARNINGS_AS_ERRORS)
    asc_target_enable_warnings(
      TARGET asc_m3_exceptions_disabled
      AS_ERRORS
    )
  else()
    asc_target_enable_warnings(TARGET asc_m3_exceptions_disabled)
  endif()
  if(ASC_CPP_SANITIZER_ARGUMENTS)
    asc_target_enable_sanitizers(
      TARGET asc_m3_exceptions_disabled
      ${ASC_CPP_SANITIZER_ARGUMENTS}
    )
  endif()
  target_compile_options(asc_m3_exceptions_disabled PRIVATE -fno-exceptions)
  asc_register_test(
    NAME asc_cpp.compile.m3_exceptions_disabled
    TARGET asc_m3_exceptions_disabled
    LABELS "compile;dense"
  )
endif()

get_target_property(_asc_dense_direct_links asc_dense LINK_LIBRARIES)
if(NOT _asc_dense_direct_links STREQUAL "ASC::core;ASC::expression")
  message(FATAL_ERROR
    "asc_dense must link directly only to ASC::core and ASC::expression, "
    "found: ${_asc_dense_direct_links}"
  )
endif()
get_target_property(
  _asc_dense_interface_links
  asc_dense
  INTERFACE_LINK_LIBRARIES
)
if(NOT _asc_dense_interface_links STREQUAL "ASC::core;ASC::expression")
  message(FATAL_ERROR
    "ASC::dense must expose only ASC::core and ASC::expression, found: "
    "${_asc_dense_interface_links}"
  )
endif()

add_test(
  NAME asc_cpp.compile.m3_dependency_audit
  COMMAND
    "${CMAKE_COMMAND}"
    "-DSOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/m3_dependency_check.cmake"
)
set_tests_properties(
  asc_cpp.compile.m3_dependency_audit
  PROPERTIES LABELS "architecture;compile;dense"
)

set(_asc_m4_public_headers
  asc/expression/writable.h
  asc/sparse.h
  asc/sparse/compressed.h
  asc/sparse/coordinate.h
  asc/sparse/evaluate.h
  asc/sparse/export.h
  asc/sparse/blas.h
  asc/sparse/print.h
)

foreach(_header IN LISTS _asc_m4_public_headers)
  if(_header STREQUAL "asc/expression/writable.h")
    set(_owner_target ASC::expression)
  else()
    set(_owner_target ASC::sparse)
  endif()
  string(MAKE_C_IDENTIFIER "${_header}" _header_identifier)
  set(_target "asc_m4_header_${_header_identifier}")
  add_executable("${_target}" header_self_containment.cc)
  target_compile_definitions(
    "${_target}"
    PRIVATE "ASC_TEST_HEADER=\"${_header}\""
  )
  target_link_libraries("${_target}" PRIVATE "${_owner_target}")
  asc_target_enable_cxx20(TARGET "${_target}")
  if(ASC_CPP_WARNINGS_AS_ERRORS)
    asc_target_enable_warnings(TARGET "${_target}" AS_ERRORS)
  else()
    asc_target_enable_warnings(TARGET "${_target}")
  endif()
  if(ASC_CPP_SANITIZER_ARGUMENTS)
    asc_target_enable_sanitizers(
      TARGET "${_target}"
      ${ASC_CPP_SANITIZER_ARGUMENTS}
    )
  endif()
  asc_register_test(
    NAME "asc_cpp.compile.m4_header.${_header_identifier}"
    TARGET "${_target}"
    LABELS "compile;sparse"
  )

  if(_asc_core_supports_disabled_exceptions)
    set(_no_exceptions_target
      "asc_m4_header_no_exceptions_${_header_identifier}"
    )
    add_executable(
      "${_no_exceptions_target}"
      header_self_containment_no_exceptions.cc
    )
    target_compile_definitions(
      "${_no_exceptions_target}"
      PRIVATE "ASC_TEST_HEADER=\"${_header}\""
    )
    target_link_libraries(
      "${_no_exceptions_target}"
      PRIVATE "${_owner_target}"
    )
    asc_target_enable_cxx20(TARGET "${_no_exceptions_target}")
    if(ASC_CPP_WARNINGS_AS_ERRORS)
      asc_target_enable_warnings(
        TARGET "${_no_exceptions_target}"
        AS_ERRORS
      )
    else()
      asc_target_enable_warnings(TARGET "${_no_exceptions_target}")
    endif()
    if(ASC_CPP_SANITIZER_ARGUMENTS)
      asc_target_enable_sanitizers(
        TARGET "${_no_exceptions_target}"
        ${ASC_CPP_SANITIZER_ARGUMENTS}
      )
    endif()
    target_compile_options("${_no_exceptions_target}" PRIVATE -fno-exceptions)
    asc_register_test(
      NAME "asc_cpp.compile.m4_header_no_exceptions.${_header_identifier}"
      TARGET "${_no_exceptions_target}"
      LABELS "compile;sparse"
    )
  endif()
endforeach()

foreach(_compile_source IN ITEMS
    m4_header_sparse.cc
    m4_header_coordinate.cc
    m4_header_compressed.cc
    m4_header_evaluate.cc
    m4_header_export.cc
    m4_header_blas.cc
    m4_header_writable.cc
)
  get_filename_component(_compile_name "${_compile_source}" NAME_WE)
  set(_target "asc_sparse_${_compile_name}")
  add_executable("${_target}" "${_compile_source}")
  if(_compile_source STREQUAL "m4_header_writable.cc")
    target_link_libraries("${_target}" PRIVATE ASC::expression)
  else()
    target_link_libraries("${_target}" PRIVATE ASC::sparse)
  endif()
  asc_target_enable_cxx20(TARGET "${_target}")
  if(ASC_CPP_WARNINGS_AS_ERRORS)
    asc_target_enable_warnings(TARGET "${_target}" AS_ERRORS)
  else()
    asc_target_enable_warnings(TARGET "${_target}")
  endif()
  if(ASC_CPP_SANITIZER_ARGUMENTS)
    asc_target_enable_sanitizers(
      TARGET "${_target}"
      ${ASC_CPP_SANITIZER_ARGUMENTS}
    )
  endif()
  asc_register_test(
    NAME "asc_cpp.compile.${_compile_name}"
    TARGET "${_target}"
    LABELS "compile;sparse"
  )
endforeach()

add_executable(asc_sparse_m4_dense_interop m4_dense_interop.cc)
target_link_libraries(
  asc_sparse_m4_dense_interop
  PRIVATE ASC::sparse ASC::dense
)
asc_target_enable_cxx20(TARGET asc_sparse_m4_dense_interop)
if(ASC_CPP_WARNINGS_AS_ERRORS)
  asc_target_enable_warnings(TARGET asc_sparse_m4_dense_interop AS_ERRORS)
else()
  asc_target_enable_warnings(TARGET asc_sparse_m4_dense_interop)
endif()
if(ASC_CPP_SANITIZER_ARGUMENTS)
  asc_target_enable_sanitizers(
    TARGET asc_sparse_m4_dense_interop
    ${ASC_CPP_SANITIZER_ARGUMENTS}
  )
endif()
asc_register_test(
  NAME asc_cpp.compile.m4_dense_interop
  TARGET asc_sparse_m4_dense_interop
  LABELS "compile;dense;sparse"
)

if(_asc_core_supports_disabled_exceptions)
  add_executable(asc_m4_exceptions_disabled m4_exceptions_disabled.cc)
  target_link_libraries(
    asc_m4_exceptions_disabled
    PRIVATE ASC::sparse
  )
  asc_target_enable_cxx20(TARGET asc_m4_exceptions_disabled)
  if(ASC_CPP_WARNINGS_AS_ERRORS)
    asc_target_enable_warnings(
      TARGET asc_m4_exceptions_disabled
      AS_ERRORS
    )
  else()
    asc_target_enable_warnings(TARGET asc_m4_exceptions_disabled)
  endif()
  if(ASC_CPP_SANITIZER_ARGUMENTS)
    asc_target_enable_sanitizers(
      TARGET asc_m4_exceptions_disabled
      ${ASC_CPP_SANITIZER_ARGUMENTS}
    )
  endif()
  target_compile_options(asc_m4_exceptions_disabled PRIVATE -fno-exceptions)
  asc_register_test(
    NAME asc_cpp.compile.m4_exceptions_disabled
    TARGET asc_m4_exceptions_disabled
    LABELS "compile;sparse"
  )
endif()

get_target_property(_asc_sparse_direct_links asc_sparse LINK_LIBRARIES)
if(NOT _asc_sparse_direct_links STREQUAL "ASC::core;ASC::expression")
  message(FATAL_ERROR
    "asc_sparse must link directly only to ASC::core and ASC::expression, "
    "found: ${_asc_sparse_direct_links}"
  )
endif()
get_target_property(
  _asc_sparse_interface_links
  asc_sparse
  INTERFACE_LINK_LIBRARIES
)
if(NOT _asc_sparse_interface_links STREQUAL "ASC::core;ASC::expression")
  message(FATAL_ERROR
    "ASC::sparse must expose only ASC::core and ASC::expression, found: "
    "${_asc_sparse_interface_links}"
  )
endif()

add_test(
  NAME asc_cpp.compile.m4_dependency_audit
  COMMAND
    "${CMAKE_COMMAND}"
    "-DSOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/m4_dependency_check.cmake"
)
set_tests_properties(
  asc_cpp.compile.m4_dependency_audit
  PROPERTIES LABELS "architecture;compile;sparse"
)

set(_asc_m5_public_headers
  asc/random/dense.h
  asc/random/sparse.h
)

foreach(_header IN LISTS _asc_m5_public_headers)
  if(_header STREQUAL "asc/random/dense.h")
    set(_owner_target ASC::random_dense)
  else()
    set(_owner_target ASC::random_sparse)
  endif()
  string(MAKE_C_IDENTIFIER "${_header}" _header_identifier)
  set(_target "asc_m5_header_${_header_identifier}")
  add_executable("${_target}" header_self_containment.cc)
  target_compile_definitions(
    "${_target}"
    PRIVATE "ASC_TEST_HEADER=\"${_header}\""
  )
  target_link_libraries("${_target}" PRIVATE "${_owner_target}")
  asc_target_enable_cxx20(TARGET "${_target}")
  if(ASC_CPP_WARNINGS_AS_ERRORS)
    asc_target_enable_warnings(TARGET "${_target}" AS_ERRORS)
  else()
    asc_target_enable_warnings(TARGET "${_target}")
  endif()
  if(ASC_CPP_SANITIZER_ARGUMENTS)
    asc_target_enable_sanitizers(
      TARGET "${_target}"
      ${ASC_CPP_SANITIZER_ARGUMENTS}
    )
  endif()
  asc_register_test(
    NAME "asc_cpp.compile.m5_header.${_header_identifier}"
    TARGET "${_target}"
    LABELS "compile;random;random-storage"
  )

  if(_asc_core_supports_disabled_exceptions)
    set(_no_exceptions_target
      "asc_m5_header_no_exceptions_${_header_identifier}"
    )
    add_executable(
      "${_no_exceptions_target}"
      header_self_containment_no_exceptions.cc
    )
    target_compile_definitions(
      "${_no_exceptions_target}"
      PRIVATE "ASC_TEST_HEADER=\"${_header}\""
    )
    target_link_libraries(
      "${_no_exceptions_target}"
      PRIVATE "${_owner_target}"
    )
    asc_target_enable_cxx20(TARGET "${_no_exceptions_target}")
    if(ASC_CPP_WARNINGS_AS_ERRORS)
      asc_target_enable_warnings(
        TARGET "${_no_exceptions_target}"
        AS_ERRORS
      )
    else()
      asc_target_enable_warnings(TARGET "${_no_exceptions_target}")
    endif()
    if(ASC_CPP_SANITIZER_ARGUMENTS)
      asc_target_enable_sanitizers(
        TARGET "${_no_exceptions_target}"
        ${ASC_CPP_SANITIZER_ARGUMENTS}
      )
    endif()
    target_compile_options("${_no_exceptions_target}" PRIVATE -fno-exceptions)
    asc_register_test(
      NAME "asc_cpp.compile.m5_header_no_exceptions.${_header_identifier}"
      TARGET "${_no_exceptions_target}"
      LABELS "compile;random;random-storage"
    )
  endif()
endforeach()

foreach(_compile_source IN ITEMS
    m5_random_dense_header.cc
    m5_random_sparse_header.cc
)
  get_filename_component(_compile_name "${_compile_source}" NAME_WE)
  set(_target "asc_random_${_compile_name}")
  add_executable("${_target}" "${_compile_source}")
  if(_compile_source STREQUAL "m5_random_dense_header.cc")
    target_link_libraries("${_target}" PRIVATE ASC::random_dense)
  else()
    target_link_libraries("${_target}" PRIVATE ASC::random_sparse)
  endif()
  asc_target_enable_cxx20(TARGET "${_target}")
  if(ASC_CPP_WARNINGS_AS_ERRORS)
    asc_target_enable_warnings(TARGET "${_target}" AS_ERRORS)
  else()
    asc_target_enable_warnings(TARGET "${_target}")
  endif()
  if(ASC_CPP_SANITIZER_ARGUMENTS)
    asc_target_enable_sanitizers(
      TARGET "${_target}"
      ${ASC_CPP_SANITIZER_ARGUMENTS}
    )
  endif()
  asc_register_test(
    NAME "asc_cpp.compile.${_compile_name}"
    TARGET "${_target}"
    LABELS "compile;random;random-storage"
  )
endforeach()

add_executable(
  asc_random_m5_multi_tu
  m5_random_facets_odr_a.cc
  m5_random_facets_odr_b.cc
  m5_random_facets_odr_main.cc
)
target_link_libraries(
  asc_random_m5_multi_tu
  PRIVATE ASC::random_dense ASC::random_sparse
)
asc_target_enable_cxx20(TARGET asc_random_m5_multi_tu)
if(ASC_CPP_WARNINGS_AS_ERRORS)
  asc_target_enable_warnings(TARGET asc_random_m5_multi_tu AS_ERRORS)
else()
  asc_target_enable_warnings(TARGET asc_random_m5_multi_tu)
endif()
if(ASC_CPP_SANITIZER_ARGUMENTS)
  asc_target_enable_sanitizers(
    TARGET asc_random_m5_multi_tu
    ${ASC_CPP_SANITIZER_ARGUMENTS}
  )
endif()
asc_register_test(
  NAME asc_cpp.compile.m5_multi_tu
  TARGET asc_random_m5_multi_tu
  LABELS "compile;random;random-storage"
)

foreach(_facet IN ITEMS random_dense random_sparse cpp)
  get_target_property(_facet_type "asc_${_facet}" TYPE)
  if(NOT _facet_type STREQUAL "INTERFACE_LIBRARY")
    message(FATAL_ERROR
      "asc_${_facet} must be an interface library, found: ${_facet_type}"
    )
  endif()
endforeach()
get_target_property(
  _asc_random_dense_links
  asc_random_dense
  INTERFACE_LINK_LIBRARIES
)
if(NOT _asc_random_dense_links STREQUAL "ASC::random;ASC::dense")
  message(FATAL_ERROR
    "ASC::random_dense direct links differ: ${_asc_random_dense_links}"
  )
endif()
get_target_property(
  _asc_random_sparse_links
  asc_random_sparse
  INTERFACE_LINK_LIBRARIES
)
if(NOT _asc_random_sparse_links STREQUAL "ASC::random;ASC::sparse")
  message(FATAL_ERROR
    "ASC::random_sparse direct links differ: ${_asc_random_sparse_links}"
  )
endif()
get_target_property(_asc_cpp_links asc_cpp INTERFACE_LINK_LIBRARIES)
if(NOT _asc_cpp_links STREQUAL
   "ASC::core;ASC::utilities;ASC::expression;ASC::dense;ASC::sparse;ASC::random;ASC::random_dense;ASC::random_sparse")
  message(FATAL_ERROR "ASC::cpp direct links differ: ${_asc_cpp_links}")
endif()

if(ASC_CPP_ENABLE_CUDA)
  function(_asc_cuda_configure_compile_test target)
    target_link_libraries("${target}" PRIVATE ${ARGN})
    target_include_directories(
      "${target}"
      PRIVATE "${PROJECT_SOURCE_DIR}/tests/compile"
    )
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

  add_executable(asc_m6_core_cuda_header m6_core_cuda_header.cc)
  _asc_cuda_configure_compile_test(asc_m6_core_cuda_header ASC::core_cuda)
  asc_register_test(
    NAME asc_cpp.compile.m6_core_cuda_header
    TARGET asc_m6_core_cuda_header
    LABELS "compile;core_cuda;cuda-core-dense"
  )

  add_executable(asc_m6_dense_cuda_header m6_dense_cuda_header.cc)
  _asc_cuda_configure_compile_test(asc_m6_dense_cuda_header ASC::dense_cuda)
  asc_register_test(
    NAME asc_cpp.compile.m6_dense_cuda_header
    TARGET asc_m6_dense_cuda_header
    LABELS "compile;dense_cuda;cuda-core-dense"
  )

  add_executable(
    asc_m6_provider_odr
    m6_provider_odr_a.cc
    m6_provider_odr_b.cc
    m6_provider_odr_main.cc
  )
  _asc_cuda_configure_compile_test(asc_m6_provider_odr ASC::core_cuda)
  asc_register_test(
    NAME asc_cpp.compile.m6_provider_odr
    TARGET asc_m6_provider_odr
    LABELS "compile;core_cuda;dense_cuda;cuda-core-dense"
  )

  if(_asc_core_supports_disabled_exceptions)
    add_executable(
      asc_m6_provider_headers_no_exceptions
      m6_provider_headers_no_exceptions.cc
    )
    _asc_cuda_configure_compile_test(
      asc_m6_provider_headers_no_exceptions
      ASC::dense_cuda
    )
    target_compile_options(
      asc_m6_provider_headers_no_exceptions
      PRIVATE -fno-exceptions
    )
    asc_register_test(
      NAME asc_cpp.compile.m6_provider_headers_no_exceptions
      TARGET asc_m6_provider_headers_no_exceptions
      LABELS "compile;core_cuda;dense_cuda;cuda-core-dense"
    )
  endif()

  foreach(_negative_source IN ITEMS
      m6_negative_copy_completion_event.cc
      m6_negative_copy_cuda_resource.cc
      m6_negative_copy_dense_cuda_context.cc
      m6_negative_integral_cuda_destination.cc
      m6_negative_volatile_cuda_destination.cc
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
        "-DCMAKE_TRY_COMPILE_TARGET_TYPE:STRING=STATIC_LIBRARY"
        "-DINCLUDE_DIRECTORIES:STRING=${PROJECT_SOURCE_DIR}/include"
      OUTPUT_VARIABLE _negative_output
    )
    if(_negative_compiled)
      message(FATAL_ERROR
        "Expected ${_negative_source} to fail compilation.\n"
        "${_negative_output}"
      )
    endif()
  endforeach()

  get_target_property(_asc_core_cuda_links asc_core_cuda LINK_LIBRARIES)
  if(NOT _asc_core_cuda_links STREQUAL "ASC::core;CUDA::cudart")
    message(FATAL_ERROR
      "asc_core_cuda direct links differ: ${_asc_core_cuda_links}"
    )
  endif()
  get_target_property(_asc_dense_cuda_links asc_dense_cuda LINK_LIBRARIES)
  if(NOT _asc_dense_cuda_links STREQUAL
     "ASC::dense;ASC::core_cuda;CUDA::cublas")
    message(FATAL_ERROR
      "asc_dense_cuda direct links differ: ${_asc_dense_cuda_links}"
    )
  endif()

  set(_asc_m7_public_provider_headers
    "asc/random/providers/cuda.h|ASC::random_cuda"
    "asc/random/providers/cuda_export.h|ASC::random_cuda"
    "asc/random/providers/dense_cuda.h|ASC::random_dense_cuda"
    "asc/random/providers/dense_cuda_export.h|ASC::random_dense_cuda"
    "asc/random/providers/sparse_cuda.h|ASC::random_sparse_cuda"
    "asc/random/providers/sparse_cuda_export.h|ASC::random_sparse_cuda"
    "asc/sparse/providers/cuda.h|ASC::sparse_cuda"
    "asc/sparse/providers/cuda_export.h|ASC::sparse_cuda"
  )
  foreach(_header_record IN LISTS _asc_m7_public_provider_headers)
    string(REPLACE "|" ";" _header_fields "${_header_record}")
    list(GET _header_fields 0 _header)
    list(GET _header_fields 1 _owner_target)
    string(MAKE_C_IDENTIFIER "${_header}" _header_identifier)

    set(_target "asc_m7_header_${_header_identifier}")
    add_executable("${_target}" header_self_containment.cc)
    target_compile_definitions(
      "${_target}"
      PRIVATE "ASC_TEST_HEADER=\"${_header}\""
    )
    _asc_cuda_configure_compile_test("${_target}" "${_owner_target}")
    asc_register_test(
      NAME "asc_cpp.compile.m7_header.${_header_identifier}"
      TARGET "${_target}"
      LABELS "compile;cuda;cuda-sparse-random"
    )

    if(_asc_core_supports_disabled_exceptions)
      set(_no_exceptions_target
        "asc_m7_header_no_exceptions_${_header_identifier}"
      )
      add_executable(
        "${_no_exceptions_target}"
        header_self_containment_no_exceptions.cc
      )
      target_compile_definitions(
        "${_no_exceptions_target}"
        PRIVATE "ASC_TEST_HEADER=\"${_header}\""
      )
      _asc_cuda_configure_compile_test(
        "${_no_exceptions_target}"
        "${_owner_target}"
      )
      target_compile_options(
        "${_no_exceptions_target}"
        PRIVATE -fno-exceptions
      )
      asc_register_test(
        NAME
          "asc_cpp.compile.m7_header_no_exceptions.${_header_identifier}"
        TARGET "${_no_exceptions_target}"
        LABELS "compile;cuda;cuda-sparse-random"
      )
    endif()
  endforeach()

  foreach(_positive_source IN ITEMS
      m7_cuda_contracts.cc
      m7_random_cuda_header.cc
      m7_random_dense_cuda_header.cc
      m7_random_sparse_cuda_header.cc
      m7_sparse_cuda_header.cc
  )
    get_filename_component(_positive_name "${_positive_source}" NAME_WE)
    set(_target "asc_${_positive_name}")
    add_executable("${_target}" "${_positive_source}")
    if(_positive_source STREQUAL "m7_random_cuda_header.cc")
      set(_owner_targets ASC::random_cuda)
    elseif(_positive_source STREQUAL "m7_random_dense_cuda_header.cc")
      set(_owner_targets ASC::random_dense_cuda)
    elseif(_positive_source STREQUAL "m7_random_sparse_cuda_header.cc")
      set(_owner_targets ASC::random_sparse_cuda)
    elseif(_positive_source STREQUAL "m7_sparse_cuda_header.cc")
      set(_owner_targets ASC::sparse_cuda)
    else()
      set(_owner_targets
        ASC::sparse_cuda
        ASC::random_dense_cuda
        ASC::random_sparse_cuda
      )
    endif()
    _asc_cuda_configure_compile_test("${_target}" ${_owner_targets})
    asc_register_test(
      NAME "asc_cpp.compile.${_positive_name}"
      TARGET "${_target}"
      LABELS "compile;cuda;cuda-sparse-random"
    )
  endforeach()

  foreach(_negative_source IN ITEMS
      m7_cuda_negative_copy.cc
      m7_negative_dense_const_destination.cc
      m7_negative_dense_integral.cc
      m7_negative_random_result_copy.cc
      m7_negative_sparse_integral.cc
      m7_negative_sparse_random_result_copy.cc
      m7_negative_sparse_volatile.cc
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
        "-DCMAKE_TRY_COMPILE_TARGET_TYPE:STRING=STATIC_LIBRARY"
        "-DINCLUDE_DIRECTORIES:STRING=${PROJECT_SOURCE_DIR}/include"
      OUTPUT_VARIABLE _negative_output
    )
    if(_negative_compiled)
      message(FATAL_ERROR
        "Expected ${_negative_source} to fail compilation.\n"
        "${_negative_output}"
      )
    endif()
  endforeach()

  add_executable(
    asc_m7_provider_odr
    m7_provider_odr_a.cc
    m7_provider_odr_b.cc
    m7_provider_odr_main.cc
  )
  _asc_cuda_configure_compile_test(
    asc_m7_provider_odr
    ASC::sparse_cuda
    ASC::random_dense_cuda
    ASC::random_sparse_cuda
  )
  asc_register_test(
    NAME asc_cpp.compile.m7_provider_odr
    TARGET asc_m7_provider_odr
    LABELS "compile;cuda;cuda-sparse-random"
  )

  get_target_property(_asc_sparse_cuda_links asc_sparse_cuda LINK_LIBRARIES)
  if(NOT _asc_sparse_cuda_links STREQUAL
     "ASC::sparse;ASC::core_cuda;CUDA::cusparse")
    message(FATAL_ERROR
      "asc_sparse_cuda direct links differ: ${_asc_sparse_cuda_links}"
    )
  endif()
  get_target_property(_asc_random_cuda_links asc_random_cuda LINK_LIBRARIES)
  if(NOT _asc_random_cuda_links STREQUAL "ASC::random;ASC::core_cuda")
    message(FATAL_ERROR
      "asc_random_cuda direct links differ: ${_asc_random_cuda_links}"
    )
  endif()
  get_target_property(
    _asc_random_dense_cuda_links
    asc_random_dense_cuda
    LINK_LIBRARIES
  )
  if(NOT _asc_random_dense_cuda_links STREQUAL
     "ASC::random_dense;ASC::random_cuda;ASC::core_cuda")
    message(FATAL_ERROR
      "asc_random_dense_cuda direct links differ: "
      "${_asc_random_dense_cuda_links}"
    )
  endif()
  get_target_property(
    _asc_random_sparse_cuda_links
    asc_random_sparse_cuda
    LINK_LIBRARIES
  )
  if(NOT _asc_random_sparse_cuda_links STREQUAL
     "ASC::random_sparse;ASC::random_cuda;ASC::core_cuda")
    message(FATAL_ERROR
      "asc_random_sparse_cuda direct links differ: "
      "${_asc_random_sparse_cuda_links}"
    )
  endif()
endif()

unset(_asc_cpp_links)
unset(_asc_expression_links)
unset(_asc_expression_type)
unset(_asc_core_direct_links)
unset(_asc_core_interface_links)
unset(_asc_core_cuda_links)
unset(_asc_core_public_headers)
unset(_asc_core_supports_disabled_exceptions)
unset(_asc_m2_direct_links)
unset(_asc_m2_interface_links)
unset(_asc_m2_public_headers)
unset(_asc_m2_target)
unset(_asc_m3_public_headers)
unset(_asc_m4_public_headers)
unset(_asc_m5_public_headers)
unset(_asc_random_dense_links)
unset(_asc_random_dense_cuda_links)
unset(_asc_random_cuda_links)
unset(_asc_random_sparse_links)
unset(_asc_random_sparse_cuda_links)
unset(_asc_sparse_cuda_links)
unset(_asc_dense_direct_links)
unset(_asc_dense_interface_links)
unset(_asc_dense_cuda_links)
unset(_asc_sparse_direct_links)
unset(_asc_sparse_interface_links)
unset(_compile_name)
unset(_compile_source)
unset(_facet)
unset(_facet_type)
unset(_header)
unset(_header_identifier)
unset(_no_exceptions_target)
unset(_owner_target)
unset(_owner_targets)
unset(_positive_name)
unset(_positive_source)
unset(_target)
