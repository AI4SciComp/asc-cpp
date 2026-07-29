cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SOURCE_DIR OR SOURCE_DIR STREQUAL "")
  message(FATAL_ERROR "SOURCE_DIR is required.")
endif()

include(
  "${SOURCE_DIR}/tests/architecture/check_public_file_policy.cmake"
)

file(
  GLOB_RECURSE _public_files
  LIST_DIRECTORIES FALSE
  "${SOURCE_DIR}/include/asc/*.h"
)
foreach(_file IN LISTS _public_files)
  file(READ "${_file}" _contents)
  if(_contents MATCHES "(^|[^A-Za-z0-9_])throw[ \t\r\n(]"
     OR _contents MATCHES "(^|[^A-Za-z0-9_])catch[ \t\r\n]*\\(")
    message(FATAL_ERROR
      "Public production exceptions are forbidden: ${_file}"
    )
  endif()
endforeach()

file(READ "${SOURCE_DIR}/include/asc/core/result.h" _result_header)
if(_result_header MATCHES "ToString[ \t\r\n]*\\(")
  message(FATAL_ERROR
    "Failed Result access must not render an allocating Status string."
  )
endif()
