cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS INCLUDE_ROOT ENABLE_CUDA)
  if(NOT DEFINED "${_required}")
    message(FATAL_ERROR "${_required} is required.")
  endif()
endforeach()
if(NOT IS_DIRECTORY "${INCLUDE_ROOT}/asc")
  message(FATAL_ERROR
    "Installed ASCCpp include root is missing '${INCLUDE_ROOT}/asc'."
  )
endif()

# Independent M8 oracle: the approved provider-free public file sets.
set(_expected_headers
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
  asc/dense.h
  asc/dense/array.h
  asc/dense/evaluate.h
  asc/dense/export.h
  asc/dense/layout.h
  asc/dense/linalg.h
  asc/dense/view.h
  asc/expression.h
  asc/expression/expression.h
  asc/expression/writable.h
  asc/random.h
  asc/random/dense.h
  asc/random/distribution.h
  asc/random/engine.h
  asc/random/export.h
  asc/random/sparse.h
  asc/sparse.h
  asc/sparse/compressed.h
  asc/sparse/coordinate.h
  asc/sparse/evaluate.h
  asc/sparse/export.h
  asc/sparse/linalg.h
  asc/utilities.h
  asc/utilities/command_line.h
  asc/utilities/export.h
  asc/utilities/timer.h
)
if(ENABLE_CUDA)
  list(APPEND _expected_headers
    asc/core/providers/cuda.h
    asc/core/providers/cuda_export.h
    asc/dense/providers/cuda.h
    asc/dense/providers/cuda_export.h
    asc/random/providers/cuda.h
    asc/random/providers/cuda_export.h
    asc/random/providers/dense_cuda.h
    asc/random/providers/dense_cuda_export.h
    asc/random/providers/sparse_cuda.h
    asc/random/providers/sparse_cuda_export.h
    asc/sparse/providers/cuda.h
    asc/sparse/providers/cuda_export.h
  )
endif()

file(
  GLOB_RECURSE _actual_headers
  LIST_DIRECTORIES FALSE
  RELATIVE "${INCLUDE_ROOT}"
  "${INCLUDE_ROOT}/asc/*.h"
)
list(SORT _actual_headers)
list(SORT _expected_headers)
if(NOT _actual_headers STREQUAL _expected_headers)
  set(_missing "${_expected_headers}")
  set(_unexpected "${_actual_headers}")
  foreach(_header IN LISTS _actual_headers)
    list(REMOVE_ITEM _missing "${_header}")
  endforeach()
  foreach(_header IN LISTS _expected_headers)
    list(REMOVE_ITEM _unexpected "${_header}")
  endforeach()
  message(FATAL_ERROR
    "Installed public headers differ from the independent M8 manifest.\n"
    "missing: ${_missing}\n"
    "unexpected: ${_unexpected}"
  )
endif()

foreach(_legacy_header IN ITEMS
    asc/array.h
    asc/asc.h
    asc/cpp.h
    asc/linalg.h
)
  if(EXISTS "${INCLUDE_ROOT}/${_legacy_header}")
    message(FATAL_ERROR
      "Deleted compatibility header was installed: ${_legacy_header}"
    )
  endif()
endforeach()

list(LENGTH _actual_headers _header_count)
message(STATUS
  "M8 installed-header manifest passed: ${_header_count} exact headers; "
  "CUDA=${ENABLE_CUDA}."
)

unset(_actual_headers)
unset(_expected_headers)
unset(_header)
unset(_header_count)
unset(_legacy_header)
unset(_missing)
unset(_required)
unset(_unexpected)
