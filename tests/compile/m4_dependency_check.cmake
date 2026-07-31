cmake_minimum_required(VERSION 3.25)

if(POLICY CMP0159)
  cmake_policy(SET CMP0159 NEW)
endif()

if(NOT DEFINED SOURCE_DIR OR SOURCE_DIR STREQUAL "")
  message(FATAL_ERROR "SOURCE_DIR is required.")
endif()

set(_expected_sparse_headers
  include/asc/sparse.h
  include/asc/sparse/compressed.h
  include/asc/sparse/coordinate.h
  include/asc/sparse/evaluate.h
  include/asc/sparse/export.h
  include/asc/sparse/blas.h
)
set(_expected_sparse_sources
  src/sparse/reference_blas.cc
)

set(_observed_sparse_headers)
if(EXISTS "${SOURCE_DIR}/include/asc/sparse.h")
  list(APPEND _observed_sparse_headers include/asc/sparse.h)
endif()
file(
  GLOB_RECURSE _nested_sparse_headers
  LIST_DIRECTORIES FALSE
  RELATIVE "${SOURCE_DIR}"
  "${SOURCE_DIR}/include/asc/sparse/*"
)
list(APPEND _observed_sparse_headers ${_nested_sparse_headers})
list(FILTER
  _observed_sparse_headers
  EXCLUDE
  REGEX "^include/asc/sparse/providers/"
)

file(
  GLOB_RECURSE _observed_sparse_sources
  LIST_DIRECTORIES FALSE
  RELATIVE "${SOURCE_DIR}"
  "${SOURCE_DIR}/src/sparse/*"
)
list(REMOVE_ITEM _observed_sparse_sources src/sparse/CMakeLists.txt)
list(FILTER
  _observed_sparse_sources
  EXCLUDE
  REGEX "^src/sparse/cuda/"
)

list(SORT _expected_sparse_headers)
list(SORT _expected_sparse_sources)
list(SORT _observed_sparse_headers)
list(SORT _observed_sparse_sources)
if(NOT _observed_sparse_headers STREQUAL _expected_sparse_headers)
  message(FATAL_ERROR
    "Milestone 4 Sparse public inventory differs from the contract.\n"
    "Expected: ${_expected_sparse_headers}\n"
    "Observed: ${_observed_sparse_headers}"
  )
endif()
if(NOT _observed_sparse_sources STREQUAL _expected_sparse_sources)
  message(FATAL_ERROR
    "Milestone 4 Sparse source inventory differs from the contract.\n"
    "Expected: ${_expected_sparse_sources}\n"
    "Observed: ${_observed_sparse_sources}"
  )
endif()
if(NOT EXISTS "${SOURCE_DIR}/include/asc/expression/writable.h")
  message(FATAL_ERROR
    "Milestone 4 requires include/asc/expression/writable.h."
  )
endif()

set(_provider_pattern
  "(^|/)(cuda|cublas|cusolver|cusparse|curand|hip|rocm|sycl|mkl|blas|lapack)(/|\\.|_)"
)
set(
  _audited_files
  ${_observed_sparse_headers}
  ${_observed_sparse_sources}
  include/asc/expression.h
  include/asc/expression/expression.h
  include/asc/expression/writable.h
  include/asc/dense/view.h
)
foreach(_relative_file IN LISTS _audited_files)
  set(_path "${SOURCE_DIR}/${_relative_file}")
  if(NOT EXISTS "${_path}")
    message(FATAL_ERROR "Missing Milestone 4 audited file: ${_relative_file}")
  endif()
  file(STRINGS "${_path}" _includes REGEX "^[ \t]*#[ \t]*include")
  foreach(_include IN LISTS _includes)
    if(_include MATCHES
       "^[ \t]*#[ \t]*include[ \t]*[<\"]([^>\"]+)[>\"]")
      set(_included_path "${CMAKE_MATCH_1}")
      if(_included_path MATCHES "^asc/([^/.\"]+)")
        set(_included_module "${CMAKE_MATCH_1}")
        if(_relative_file MATCHES "^(include/asc/sparse|src/sparse)")
          if(NOT _included_module STREQUAL "core"
             AND NOT _included_module STREQUAL "expression"
             AND NOT _included_module STREQUAL "sparse")
            message(FATAL_ERROR
              "Forbidden Sparse module include in ${_relative_file}: "
              "${_included_path}"
            )
          endif()
        elseif(_relative_file MATCHES "^include/asc/expression")
          if(NOT _included_module STREQUAL "core"
             AND NOT _included_module STREQUAL "expression")
            message(FATAL_ERROR
              "Expression gained a forbidden module include in "
              "${_relative_file}: ${_included_path}"
            )
          endif()
        elseif(_relative_file STREQUAL "include/asc/dense/view.h")
          if(_included_module STREQUAL "sparse")
            message(FATAL_ERROR
              "Dense view gained a forbidden Sparse include."
            )
          endif()
        endif()
      endif()
      string(TOLOWER "${_included_path}" _included_path_lower)
      if(NOT _included_path_lower STREQUAL "asc/sparse/blas.h"
         AND _included_path_lower MATCHES "${_provider_pattern}")
        message(FATAL_ERROR
          "Provider SDK include leaked into ${_relative_file}: "
          "${_included_path}"
        )
      endif()
    endif()
  endforeach()

  file(READ "${_path}" _contents)
  if(_contents MATCHES "#[ \t]*pragma[ \t]+once")
    message(FATAL_ERROR "#pragma once is forbidden: ${_relative_file}")
  endif()
  if(_contents MATCHES "asc::detail"
     OR _contents MATCHES "namespace[ \t\r\n]+detail")
    message(FATAL_ERROR
      "Forbidden detail namespace spelling in ${_relative_file}."
    )
  endif()
  if(_relative_file MATCHES "^include/"
     AND (_contents MATCHES "(^|[^A-Za-z0-9_])throw[ \t\r\n(]"
          OR _contents MATCHES "(^|[^A-Za-z0-9_])catch[ \t\r\n]*\\("))
    message(FATAL_ERROR
      "Public exceptions are forbidden: ${_relative_file}"
    )
  endif()
endforeach()
