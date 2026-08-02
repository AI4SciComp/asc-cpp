cmake_minimum_required(VERSION 3.25)

if(POLICY CMP0159)
  cmake_policy(SET CMP0159 NEW)
endif()

if(NOT DEFINED SOURCE_DIR OR SOURCE_DIR STREQUAL "")
  message(FATAL_ERROR "SOURCE_DIR is required.")
endif()

set(_expected_public_files
  include/asc/expression.h
  include/asc/expression/expression.h
  include/asc/random.h
  include/asc/random/distribution.h
  include/asc/random/engine.h
  include/asc/random/export.h
  include/asc/random/generator.h
  include/asc/random/quasi.h
  include/asc/random/seed.h
  include/asc/utilities.h
  include/asc/utilities/command_line.h
  include/asc/utilities/export.h
  include/asc/utilities/timer.h
)
set(_expected_source_files
  src/random/distribution.cc
  src/random/engine.cc
  src/random/quasi.cc
  src/random/seed.cc
  src/random/sobol_table_generated.cc
  src/utilities/command_line.cc
  src/utilities/timer.cc
  src/utilities/timer_internal.h
)

set(_observed_public_files)
foreach(_module IN ITEMS expression random utilities)
  if(EXISTS "${SOURCE_DIR}/include/asc/${_module}.h")
    list(APPEND
      _observed_public_files
      "include/asc/${_module}.h"
    )
  endif()
  file(
    GLOB_RECURSE _module_headers
    LIST_DIRECTORIES FALSE
    RELATIVE "${SOURCE_DIR}"
    "${SOURCE_DIR}/include/asc/${_module}/*"
  )
  list(APPEND _observed_public_files ${_module_headers})
endforeach()
# Later milestones may add explicitly approved files to a Milestone 2 module.
# Their own milestone audit owns those additions; retain this check for the
# exact Milestone 2 layer.
list(REMOVE_ITEM
  _observed_public_files
  include/asc/expression/writable.h
  include/asc/random/dense.h
  include/asc/random/sparse.h
)
list(FILTER
  _observed_public_files
  EXCLUDE
  REGEX "^include/asc/random/providers/"
)

set(_observed_source_files)
foreach(_module IN ITEMS expression random utilities)
  file(
    GLOB_RECURSE _module_sources
    LIST_DIRECTORIES FALSE
    RELATIVE "${SOURCE_DIR}"
    "${SOURCE_DIR}/src/${_module}/*"
  )
  list(REMOVE_ITEM
    _module_sources
    "src/${_module}/CMakeLists.txt"
  )
  list(APPEND _observed_source_files ${_module_sources})
endforeach()
list(FILTER
  _observed_source_files
  EXCLUDE
  REGEX "^src/random/cuda/"
)

list(SORT _expected_public_files)
list(SORT _expected_source_files)
list(SORT _observed_public_files)
list(SORT _observed_source_files)
if(NOT _observed_public_files STREQUAL _expected_public_files)
  message(FATAL_ERROR
    "Milestone 2 public inventory differs from the frozen contract.\n"
    "Expected: ${_expected_public_files}\n"
    "Observed: ${_observed_public_files}"
  )
endif()
if(NOT _observed_source_files STREQUAL _expected_source_files)
  message(FATAL_ERROR
    "Milestone 2 source inventory differs from the frozen contract.\n"
    "Expected: ${_expected_source_files}\n"
    "Observed: ${_observed_source_files}"
  )
endif()

set(_provider_pattern
  "(^|/)(cuda|cublas|cusolver|cusparse|curand|hip|rocm|sycl|mkl)(/|\\.|_)"
)
set(_all_files ${_observed_public_files} ${_observed_source_files})
foreach(_relative_file IN LISTS _all_files)
  if(_relative_file MATCHES "^include/asc/([^./]+)")
    set(_owning_module "${CMAKE_MATCH_1}")
  elseif(_relative_file MATCHES "^src/([^/]+)")
    set(_owning_module "${CMAKE_MATCH_1}")
  else()
    message(FATAL_ERROR "Cannot determine module for ${_relative_file}.")
  endif()

  set(_path "${SOURCE_DIR}/${_relative_file}")
  file(STRINGS "${_path}" _includes REGEX "^[ \t]*#[ \t]*include")
  foreach(_include IN LISTS _includes)
    if(_include MATCHES
       "^[ \t]*#[ \t]*include[ \t]*[<\"]([^>\"]+)[>\"]")
      set(_included_path "${CMAKE_MATCH_1}")
      if(_included_path MATCHES "^asc/([^/.\"]+)")
        set(_included_module "${CMAKE_MATCH_1}")
        if(NOT _included_module STREQUAL "core"
           AND NOT _included_module STREQUAL "${_owning_module}")
          message(FATAL_ERROR
            "Forbidden sibling include in ${_relative_file}: "
            "${_included_path}"
          )
        endif()
      endif()
      string(TOLOWER "${_included_path}" _included_path_lower)
      if(_included_path_lower MATCHES "${_provider_pattern}")
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

foreach(_random_file IN ITEMS
    include/asc/random.h
    include/asc/random/distribution.h
    include/asc/random/engine.h
    src/random/distribution.cc
    src/random/engine.cc)
  file(READ "${SOURCE_DIR}/${_random_file}" _random_contents)
  if(_random_contents MATCHES
     "random_device|mt19937|normal_distribution|uniform_real_distribution")
    message(FATAL_ERROR
      "Unapproved mutable or standard distribution in ${_random_file}."
    )
  endif()
endforeach()
