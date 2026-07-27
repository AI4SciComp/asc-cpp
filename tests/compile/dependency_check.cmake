cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SOURCE_DIR OR SOURCE_DIR STREQUAL "")
  message(FATAL_ERROR "SOURCE_DIR is required.")
endif()

set(_expected_public_files
  include/asc/core.h
  include/asc/core/configuration.h
  include/asc/core/contracts.h
  include/asc/core/execution.h
  include/asc/core/export.h
  include/asc/core/extents.h
  include/asc/core/io.h
  include/asc/core/memory.h
  include/asc/core/result.h
  include/asc/core/status.h
  include/asc/core/types.h
  include/asc/core/providers/cuda.h
  include/asc/core/providers/cuda_export.h
  include/asc/dense.h
  include/asc/dense/array.h
  include/asc/dense/evaluate.h
  include/asc/dense/export.h
  include/asc/dense/layout.h
  include/asc/dense/linalg.h
  include/asc/dense/view.h
  include/asc/dense/providers/cuda.h
  include/asc/dense/providers/cuda_export.h
  include/asc/expression.h
  include/asc/expression/expression.h
  include/asc/expression/writable.h
  include/asc/random.h
  include/asc/random/distribution.h
  include/asc/random/dense.h
  include/asc/random/engine.h
  include/asc/random/export.h
  include/asc/random/providers/cuda.h
  include/asc/random/providers/cuda_export.h
  include/asc/random/providers/dense_cuda.h
  include/asc/random/providers/dense_cuda_export.h
  include/asc/random/providers/sparse_cuda.h
  include/asc/random/providers/sparse_cuda_export.h
  include/asc/random/sparse.h
  include/asc/sparse.h
  include/asc/sparse/compressed.h
  include/asc/sparse/coordinate.h
  include/asc/sparse/evaluate.h
  include/asc/sparse/export.h
  include/asc/sparse/linalg.h
  include/asc/sparse/providers/cuda.h
  include/asc/sparse/providers/cuda_export.h
  include/asc/utilities.h
  include/asc/utilities/command_line.h
  include/asc/utilities/export.h
  include/asc/utilities/timer.h
)
set(_expected_source_files
  src/core/configuration.cc
  src/core/contracts.cc
  src/core/execution.cc
  src/core/execution_internal.h
  src/core/io.cc
  src/core/memory.cc
  src/core/status.cc
  src/core/cuda/cuda.cc
  src/core/cuda/provider_internal.h
  src/dense/cuda/cuda.cc
  src/dense/cuda/kernels.cu
  src/dense/cuda/kernels_internal.h
  src/dense/reference_linalg.cc
  src/random/distribution.cc
  src/random/engine.cc
  src/random/cuda/cuda.cc
  src/random/cuda/dense_cuda.cc
  src/random/cuda/dense_kernels.cu
  src/random/cuda/dense_kernels_internal.h
  src/random/cuda/kernels.cu
  src/random/cuda/kernels_internal.h
  src/random/cuda/sparse_cuda.cc
  src/random/cuda/sparse_kernels.cu
  src/random/cuda/sparse_kernels_internal.h
  src/sparse/cuda/cuda.cc
  src/sparse/cuda/kernels.cu
  src/sparse/cuda/kernels_internal.h
  src/sparse/reference_linalg.cc
  src/utilities/command_line.cc
  src/utilities/timer.cc
)

file(
  GLOB_RECURSE _observed_public_files
  LIST_DIRECTORIES FALSE
  RELATIVE "${SOURCE_DIR}"
  "${SOURCE_DIR}/include/asc/*"
)
file(
  GLOB_RECURSE _observed_source_files
  LIST_DIRECTORIES FALSE
  RELATIVE "${SOURCE_DIR}"
  "${SOURCE_DIR}/src/core/*"
  "${SOURCE_DIR}/src/dense/*"
  "${SOURCE_DIR}/src/random/*"
  "${SOURCE_DIR}/src/sparse/*"
  "${SOURCE_DIR}/src/utilities/*"
)
list(REMOVE_ITEM
  _observed_source_files
  "src/core/CMakeLists.txt"
  "src/dense/CMakeLists.txt"
  "src/random/CMakeLists.txt"
  "src/sparse/CMakeLists.txt"
  "src/utilities/CMakeLists.txt"
)
list(SORT _expected_public_files)
list(SORT _expected_source_files)
list(SORT _observed_public_files)
list(SORT _observed_source_files)
if(NOT _observed_public_files STREQUAL _expected_public_files)
  message(FATAL_ERROR
    "Milestone 8 public file inventory differs from its frozen contract.\n"
    "Expected: ${_expected_public_files}\n"
    "Observed: ${_observed_public_files}"
  )
endif()
if(NOT _observed_source_files STREQUAL _expected_source_files)
  message(FATAL_ERROR
    "Milestone 8 source inventory differs from its frozen contract.\n"
    "Expected: ${_expected_source_files}\n"
    "Observed: ${_observed_source_files}"
  )
endif()

set(_provider_include
  "(^|/)(cuda|cublas|cusolver|cusparse|curand|hip|rocm|sycl|mkl)(/|\\.|_)"
)
set(_all_production_files ${_observed_public_files} ${_observed_source_files})
foreach(_relative_file IN LISTS _all_production_files)
  set(_file "${SOURCE_DIR}/${_relative_file}")
  file(STRINGS "${_file}" _include_lines REGEX "^[ \t]*#[ \t]*include")
  foreach(_include_line IN LISTS _include_lines)
    if(_include_line MATCHES
       "^[ \t]*#[ \t]*include[ \t]*[<\"]([^>\"]+)[>\"]")
      set(_included_path "${CMAKE_MATCH_1}")
      string(TOLOWER "${_included_path}" _included_path_lower)
      set(_forbidden_asc_include)
      if(_relative_file MATCHES "^(include/asc/core|src/core)")
        set(_forbidden_asc_include
          "^asc/(utilities|expression|dense|sparse|random|array|linalg)(/|\\.h)"
        )
      elseif(_relative_file MATCHES
             "^(include/asc/utilities|src/utilities)")
        set(_forbidden_asc_include
          "^asc/(utilities|expression|dense|sparse|random|array|linalg)(/|\\.h)"
        )
        string(REPLACE "utilities|" "" _forbidden_asc_include
          "${_forbidden_asc_include}"
        )
      elseif(_relative_file MATCHES "^include/asc/expression")
        set(_forbidden_asc_include
          "^asc/(utilities|dense|sparse|random|array|linalg)(/|\\.h)"
        )
      elseif(_relative_file MATCHES "^(include/asc/dense|src/dense)")
        set(_forbidden_asc_include
          "^asc/(utilities|sparse|random|array|linalg)(/|\\.h)"
        )
      elseif(_relative_file MATCHES "^(include/asc/sparse|src/sparse)")
        set(_forbidden_asc_include
          "^asc/(utilities|dense|random|array|linalg)(/|\\.h)"
        )
      elseif(_relative_file MATCHES
             "^include/asc/random/providers/dense_cuda")
        set(_forbidden_asc_include
          "^asc/(utilities|sparse|array|linalg)(/|\\.h)"
        )
      elseif(_relative_file MATCHES
             "^include/asc/random/providers/sparse_cuda")
        set(_forbidden_asc_include
          "^asc/(utilities|dense|array|linalg)(/|\\.h)"
        )
      elseif(_relative_file STREQUAL "include/asc/random/dense.h")
        set(_forbidden_asc_include
          "^asc/(utilities|sparse|array|linalg)(/|\\.h)"
        )
      elseif(_relative_file STREQUAL "include/asc/random/sparse.h")
        set(_forbidden_asc_include
          "^asc/(utilities|dense|array|linalg)(/|\\.h)"
        )
      elseif(_relative_file MATCHES
             "^src/random/cuda/(dense_cuda|dense_kernels)")
        set(_forbidden_asc_include
          "^asc/(utilities|sparse|array|linalg)(/|\\.h)"
        )
      elseif(_relative_file MATCHES
             "^src/random/cuda/(sparse_cuda|sparse_kernels)")
        set(_forbidden_asc_include
          "^asc/(utilities|dense|array|linalg)(/|\\.h)"
        )
      elseif(_relative_file MATCHES "^(include/asc/random|src/random)")
        set(_forbidden_asc_include
          "^asc/(utilities|expression|dense|sparse|random|array|linalg)(/|\\.h)"
        )
        string(REPLACE "random|" "" _forbidden_asc_include
          "${_forbidden_asc_include}"
        )
      endif()
      if(_forbidden_asc_include
         AND _included_path MATCHES "${_forbidden_asc_include}")
        message(FATAL_ERROR
          "Forbidden ASC dependency in ${_relative_file}: ${_included_path}"
        )
      endif()
      set(_is_provider_source FALSE)
      if(_relative_file MATCHES "^src/(core|dense|sparse|random)/cuda/")
        set(_is_provider_source TRUE)
      endif()
      if(_included_path_lower MATCHES "${_provider_include}"
         AND NOT _included_path_lower MATCHES "^asc/"
         AND NOT _is_provider_source)
        message(FATAL_ERROR
          "Provider SDK include leaked outside its provider source in "
          "${_relative_file}: "
          "${_included_path}"
        )
      endif()
    endif()
  endforeach()

  file(READ "${_file}" _contents)
  if(_contents MATCHES "#[ \t]*pragma[ \t]+once")
    message(FATAL_ERROR "#pragma once is forbidden: ${_relative_file}")
  endif()
  if(_contents MATCHES "asc::detail"
     OR _contents MATCHES "namespace[ \t\r\n]+detail")
    message(FATAL_ERROR
      "Forbidden internal namespace spelling in ${_relative_file}."
    )
  endif()
endforeach()

foreach(_relative_file IN LISTS _observed_public_files)
  file(READ "${SOURCE_DIR}/${_relative_file}" _contents)
  if(_contents MATCHES "(^|[^A-Za-z0-9_])throw[ \t\r\n(]"
     OR _contents MATCHES "(^|[^A-Za-z0-9_])catch[ \t\r\n]*\\(")
    message(FATAL_ERROR
      "Public production exceptions are forbidden: ${_relative_file}"
    )
  endif()
endforeach()

file(READ "${SOURCE_DIR}/include/asc/core/result.h" _result_header)
if(_result_header MATCHES "ToString[ \t\r\n]*\\(")
  message(FATAL_ERROR
    "Failed Result value access must not allocate a rendered Status string "
    "before FatalContract."
  )
endif()
