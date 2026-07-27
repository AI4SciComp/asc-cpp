set(_asc_core_public_headers
  asc/core.h
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

function(_asc_core_disable_exceptions target)
  target_compile_options("${target}" PRIVATE -fno-exceptions)
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
    LABELS "compile;core;milestone-1"
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
    _asc_core_disable_exceptions("${_no_exceptions_target}")
    asc_register_test(
      NAME "asc_cpp.compile.header_no_exceptions.${_header_identifier}"
      TARGET "${_no_exceptions_target}"
      LABELS "compile;core;milestone-1"
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
    message(FATAL_ERROR "Milestone 2 header has no owning target: ${_header}")
  endif()

  string(MAKE_C_IDENTIFIER "${_header}" _header_identifier)
  set(_target "asc_m2_header_${_header_identifier}")
  add_executable("${_target}" m2_header_self_containment.cc)
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
    LABELS "compile;milestone-2"
  )

  if(_asc_core_supports_disabled_exceptions)
    set(_no_exceptions_target
      "asc_m2_header_no_exceptions_${_header_identifier}"
    )
    add_executable(
      "${_no_exceptions_target}"
      m2_header_self_containment_no_exceptions.cc
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
    _asc_core_disable_exceptions("${_no_exceptions_target}")
    asc_register_test(
      NAME "asc_cpp.compile.m2_header_no_exceptions.${_header_identifier}"
      TARGET "${_no_exceptions_target}"
      LABELS "compile;milestone-2"
    )
  endif()
endforeach()

set(_asc_m3_public_headers
  asc/dense.h
  asc/dense/array.h
  asc/dense/evaluate.h
  asc/dense/export.h
  asc/dense/layout.h
  asc/dense/linalg.h
  asc/dense/view.h
)

foreach(_header IN LISTS _asc_m3_public_headers)
  string(MAKE_C_IDENTIFIER "${_header}" _header_identifier)
  set(_target "asc_m3_header_${_header_identifier}")
  add_executable("${_target}" m2_header_self_containment.cc)
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
    LABELS "compile;dense;milestone-3"
  )

  if(_asc_core_supports_disabled_exceptions)
    set(_no_exceptions_target
      "asc_m3_header_no_exceptions_${_header_identifier}"
    )
    add_executable(
      "${_no_exceptions_target}"
      m2_header_self_containment_no_exceptions.cc
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
    _asc_core_disable_exceptions("${_no_exceptions_target}")
    asc_register_test(
      NAME "asc_cpp.compile.m3_header_no_exceptions.${_header_identifier}"
      TARGET "${_no_exceptions_target}"
      LABELS "compile;dense;milestone-3"
    )
  endif()
endforeach()

set(_asc_m4_public_headers
  asc/expression/writable.h
  asc/sparse.h
  asc/sparse/compressed.h
  asc/sparse/coordinate.h
  asc/sparse/evaluate.h
  asc/sparse/export.h
  asc/sparse/linalg.h
)

foreach(_header IN LISTS _asc_m4_public_headers)
  if(_header MATCHES "^asc/expression")
    set(_owner_target ASC::expression)
  else()
    set(_owner_target ASC::sparse)
  endif()
  string(MAKE_C_IDENTIFIER "${_header}" _header_identifier)
  set(_target "asc_m4_header_${_header_identifier}")
  add_executable("${_target}" m2_header_self_containment.cc)
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
    LABELS "compile;sparse;milestone-4"
  )

  if(_asc_core_supports_disabled_exceptions)
    set(_no_exceptions_target
      "asc_m4_header_no_exceptions_${_header_identifier}"
    )
    add_executable(
      "${_no_exceptions_target}"
      m2_header_self_containment_no_exceptions.cc
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
    _asc_core_disable_exceptions("${_no_exceptions_target}")
    asc_register_test(
      NAME "asc_cpp.compile.m4_header_no_exceptions.${_header_identifier}"
      TARGET "${_no_exceptions_target}"
      LABELS "compile;sparse;milestone-4"
    )
  endif()
endforeach()

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
  add_executable("${_target}" m2_header_self_containment.cc)
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
    LABELS "compile;random;milestone-5"
  )

  if(_asc_core_supports_disabled_exceptions)
    set(_no_exceptions_target
      "asc_m5_header_no_exceptions_${_header_identifier}"
    )
    add_executable(
      "${_no_exceptions_target}"
      m2_header_self_containment_no_exceptions.cc
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
    _asc_core_disable_exceptions("${_no_exceptions_target}")
    asc_register_test(
      NAME "asc_cpp.compile.m5_header_no_exceptions.${_header_identifier}"
      TARGET "${_no_exceptions_target}"
      LABELS "compile;random;milestone-5"
    )
  endif()
endforeach()

if(ASC_CPP_ENABLE_CUDA)
  set(_asc_m6_public_headers
    asc/core/providers/cuda.h
    asc/core/providers/cuda_export.h
    asc/dense/providers/cuda.h
    asc/dense/providers/cuda_export.h
  )

  foreach(_header IN LISTS _asc_m6_public_headers)
    if(_header MATCHES "^asc/core")
      set(_owner_target ASC::core_cuda)
    else()
      set(_owner_target ASC::dense_cuda)
    endif()
    string(MAKE_C_IDENTIFIER "${_header}" _header_identifier)
    set(_target "asc_m6_header_${_header_identifier}")
    add_executable("${_target}" m2_header_self_containment.cc)
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
      NAME "asc_cpp.compile.m6_header.${_header_identifier}"
      TARGET "${_target}"
      LABELS "compile;gpu;milestone-6"
    )

    if(_asc_core_supports_disabled_exceptions)
      set(_no_exceptions_target
        "asc_m6_header_no_exceptions_${_header_identifier}"
      )
      add_executable(
        "${_no_exceptions_target}"
        m2_header_self_containment_no_exceptions.cc
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
      _asc_core_disable_exceptions("${_no_exceptions_target}")
      asc_register_test(
        NAME "asc_cpp.compile.m6_header_no_exceptions.${_header_identifier}"
        TARGET "${_no_exceptions_target}"
        LABELS "compile;gpu;milestone-6"
      )
    endif()
  endforeach()

  set(_asc_m7_public_headers
    asc/sparse/providers/cuda.h
    asc/sparse/providers/cuda_export.h
    asc/random/providers/cuda.h
    asc/random/providers/cuda_export.h
    asc/random/providers/dense_cuda.h
    asc/random/providers/dense_cuda_export.h
    asc/random/providers/sparse_cuda.h
    asc/random/providers/sparse_cuda_export.h
  )
  foreach(_header IN LISTS _asc_m7_public_headers)
    if(_header MATCHES "^asc/sparse")
      set(_owner_target ASC::sparse_cuda)
    elseif(_header MATCHES "random/providers/dense")
      set(_owner_target ASC::random_dense_cuda)
    elseif(_header MATCHES "random/providers/sparse")
      set(_owner_target ASC::random_sparse_cuda)
    else()
      set(_owner_target ASC::random_cuda)
    endif()
    string(MAKE_C_IDENTIFIER "${_header}" _header_identifier)
    set(_target "asc_m7_header_${_header_identifier}")
    add_executable("${_target}" m2_header_self_containment.cc)
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
      NAME "asc_cpp.compile.m7_header.${_header_identifier}"
      TARGET "${_target}"
      LABELS "compile;gpu;milestone-7"
    )

    if(_asc_core_supports_disabled_exceptions)
      set(_no_exceptions_target
        "asc_m7_header_no_exceptions_${_header_identifier}"
      )
      add_executable(
        "${_no_exceptions_target}"
        m2_header_self_containment_no_exceptions.cc
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
      _asc_core_disable_exceptions("${_no_exceptions_target}")
      asc_register_test(
        NAME "asc_cpp.compile.m7_header_no_exceptions.${_header_identifier}"
        TARGET "${_no_exceptions_target}"
        LABELS "compile;gpu;milestone-7"
      )
    endif()
  endforeach()
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
  LABELS "compile;core;milestone-1"
)

if(_asc_core_supports_disabled_exceptions)
  add_executable(asc_core_exceptions_disabled exceptions_disabled.cc)
  _asc_core_configure_compile_test(asc_core_exceptions_disabled)
  _asc_core_disable_exceptions(asc_core_exceptions_disabled)
  asc_register_test(
    NAME asc_cpp.compile.exceptions_disabled
    TARGET asc_core_exceptions_disabled
    LABELS "compile;core;milestone-1"
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
    "ASC::core must expose no transitive target dependencies, found: "
    "${_asc_core_interface_links}"
  )
endif()

foreach(_target IN ITEMS asc_utilities asc_expression asc_random)
  get_target_property(_interface_links "${_target}" INTERFACE_LINK_LIBRARIES)
  if(NOT _interface_links STREQUAL "ASC::core")
    message(FATAL_ERROR
      "${_target} must expose exactly ASC::core, found: ${_interface_links}"
    )
  endif()
  get_target_property(_target_type "${_target}" TYPE)
  if(NOT _target_type STREQUAL "INTERFACE_LIBRARY")
    get_target_property(_direct_links "${_target}" LINK_LIBRARIES)
    if(NOT _direct_links STREQUAL "ASC::core")
      message(FATAL_ERROR
        "${_target} must link exactly ASC::core, found: ${_direct_links}"
      )
    endif()
  endif()
endforeach()

get_target_property(_asc_dense_interface_links
  asc_dense
  INTERFACE_LINK_LIBRARIES
)
if(NOT _asc_dense_interface_links STREQUAL "ASC::core;ASC::expression")
  message(FATAL_ERROR
    "asc_dense must expose exactly ASC::core and ASC::expression, found: "
    "${_asc_dense_interface_links}"
  )
endif()
get_target_property(_asc_dense_direct_links asc_dense LINK_LIBRARIES)
if(NOT _asc_dense_direct_links STREQUAL "ASC::core;ASC::expression")
  message(FATAL_ERROR
    "asc_dense must link exactly ASC::core and ASC::expression, found: "
    "${_asc_dense_direct_links}"
  )
endif()

get_target_property(_asc_sparse_interface_links
  asc_sparse
  INTERFACE_LINK_LIBRARIES
)
if(NOT _asc_sparse_interface_links STREQUAL "ASC::core;ASC::expression")
  message(FATAL_ERROR
    "asc_sparse must expose exactly ASC::core and ASC::expression, found: "
    "${_asc_sparse_interface_links}"
  )
endif()
get_target_property(_asc_sparse_direct_links asc_sparse LINK_LIBRARIES)
if(NOT _asc_sparse_direct_links STREQUAL "ASC::core;ASC::expression")
  message(FATAL_ERROR
    "asc_sparse must link exactly ASC::core and ASC::expression, found: "
    "${_asc_sparse_direct_links}"
  )
endif()

get_target_property(
  _asc_random_dense_interface_links
  asc_random_dense
  INTERFACE_LINK_LIBRARIES
)
if(NOT _asc_random_dense_interface_links
   STREQUAL "ASC::random;ASC::dense")
  message(FATAL_ERROR
    "asc_random_dense must expose exactly ASC::random and ASC::dense, found: "
    "${_asc_random_dense_interface_links}"
  )
endif()

get_target_property(
  _asc_random_sparse_interface_links
  asc_random_sparse
  INTERFACE_LINK_LIBRARIES
)
if(NOT _asc_random_sparse_interface_links
   STREQUAL "ASC::random;ASC::sparse")
  message(FATAL_ERROR
    "asc_random_sparse must expose exactly ASC::random and ASC::sparse, found: "
    "${_asc_random_sparse_interface_links}"
  )
endif()

get_target_property(_asc_cpp_interface_links asc_cpp INTERFACE_LINK_LIBRARIES)
if(NOT _asc_cpp_interface_links STREQUAL
   "ASC::core;ASC::utilities;ASC::expression;ASC::dense;ASC::sparse;ASC::random;ASC::random_dense;ASC::random_sparse")
  message(FATAL_ERROR
    "asc_cpp must expose the exact provider-free aggregate, found: "
    "${_asc_cpp_interface_links}"
  )
endif()

if(ASC_CPP_ENABLE_CUDA)
  get_target_property(
    _asc_core_cuda_direct_links
    asc_core_cuda
    LINK_LIBRARIES
  )
  if(NOT _asc_core_cuda_direct_links STREQUAL
     "ASC::core;CUDA::cudart")
    message(FATAL_ERROR
      "asc_core_cuda must link exactly ASC::core and private CUDA::cudart, "
      "found: ${_asc_core_cuda_direct_links}"
    )
  endif()
  get_target_property(
    _asc_dense_cuda_direct_links
    asc_dense_cuda
    LINK_LIBRARIES
  )
  if(NOT _asc_dense_cuda_direct_links STREQUAL
     "ASC::dense;ASC::core_cuda;CUDA::cublas")
    message(FATAL_ERROR
      "asc_dense_cuda must link exactly ASC::dense, ASC::core_cuda, and "
      "private CUDA::cublas, found: ${_asc_dense_cuda_direct_links}"
    )
  endif()
  get_target_property(
    _asc_dense_cuda_runtime_library
    asc_dense_cuda
    CUDA_RUNTIME_LIBRARY
  )
  if(NOT _asc_dense_cuda_runtime_library STREQUAL "Shared")
    message(FATAL_ERROR
      "asc_dense_cuda must use the shared CUDA runtime so every provider "
      "facet observes one runtime state, found: "
      "${_asc_dense_cuda_runtime_library}"
    )
  endif()

  get_target_property(
    _asc_sparse_cuda_direct_links
    asc_sparse_cuda
    LINK_LIBRARIES
  )
  if(NOT _asc_sparse_cuda_direct_links STREQUAL
     "ASC::sparse;ASC::core_cuda;CUDA::cusparse")
    message(FATAL_ERROR
      "asc_sparse_cuda has an invalid direct link graph: "
      "${_asc_sparse_cuda_direct_links}"
    )
  endif()
  get_target_property(
    _asc_random_cuda_direct_links
    asc_random_cuda
    LINK_LIBRARIES
  )
  if(NOT _asc_random_cuda_direct_links STREQUAL
     "ASC::random;ASC::core_cuda")
    message(FATAL_ERROR
      "asc_random_cuda has an invalid direct link graph: "
      "${_asc_random_cuda_direct_links}"
    )
  endif()
  get_target_property(
    _asc_random_dense_cuda_direct_links
    asc_random_dense_cuda
    LINK_LIBRARIES
  )
  if(NOT _asc_random_dense_cuda_direct_links STREQUAL
     "ASC::random_dense;ASC::random_cuda;ASC::core_cuda")
    message(FATAL_ERROR
      "asc_random_dense_cuda has an invalid direct link graph: "
      "${_asc_random_dense_cuda_direct_links}"
    )
  endif()
  get_target_property(
    _asc_random_sparse_cuda_direct_links
    asc_random_sparse_cuda
    LINK_LIBRARIES
  )
  if(NOT _asc_random_sparse_cuda_direct_links STREQUAL
     "ASC::random_sparse;ASC::random_cuda;ASC::core_cuda")
    message(FATAL_ERROR
      "asc_random_sparse_cuda has an invalid direct link graph: "
      "${_asc_random_sparse_cuda_direct_links}"
    )
  endif()
  foreach(_m7_cuda_target IN ITEMS
      asc_sparse_cuda
      asc_random_cuda
      asc_random_dense_cuda
      asc_random_sparse_cuda
  )
    get_target_property(
      _asc_m7_cuda_runtime_library
      "${_m7_cuda_target}"
      CUDA_RUNTIME_LIBRARY
    )
    if(NOT _asc_m7_cuda_runtime_library STREQUAL "Shared")
      message(FATAL_ERROR
        "${_m7_cuda_target} must use the shared CUDA runtime; found "
        "'${_asc_m7_cuda_runtime_library}'."
      )
    endif()
  endforeach()
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
  PROPERTIES LABELS "architecture;compile;milestone-6;milestone-7"
)

unset(_asc_core_direct_links)
unset(_asc_core_interface_links)
unset(_asc_dense_direct_links)
unset(_asc_dense_interface_links)
unset(_asc_sparse_direct_links)
unset(_asc_sparse_interface_links)
unset(_asc_random_dense_interface_links)
unset(_asc_random_sparse_interface_links)
unset(_asc_cpp_interface_links)
unset(_direct_links)
unset(_interface_links)
unset(_target_type)
unset(_asc_core_public_headers)
unset(_asc_core_supports_disabled_exceptions)
unset(_asc_m2_public_headers)
unset(_asc_m3_public_headers)
unset(_asc_m4_public_headers)
unset(_asc_m5_public_headers)
unset(_asc_dense_cuda_runtime_library)
unset(_asc_m6_public_headers)
unset(_asc_m7_public_headers)
unset(_asc_core_cuda_direct_links)
unset(_asc_dense_cuda_direct_links)
unset(_asc_sparse_cuda_direct_links)
unset(_asc_random_cuda_direct_links)
unset(_asc_random_dense_cuda_direct_links)
unset(_asc_random_sparse_cuda_direct_links)
unset(_asc_m7_cuda_runtime_library)
unset(_m7_cuda_target)
unset(_owner_target)
