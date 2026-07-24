if(NOT DEFINED ASC_CPP_SOURCE_DIR)
  message(FATAL_ERROR "ASC_CPP_SOURCE_DIR is required")
endif()

set(_canonical_headers
  include/asc/core.h
  include/asc/core/buffer.h
  include/asc/core/contracts.h
  include/asc/core/event.h
  include/asc/core/execution_context.h
  include/asc/core/memory_resource.h
  include/asc/core/memory_space.h
  include/asc/core/status.h
  include/asc/core/types.h
)
file(GLOB_RECURSE _canonical_sources
  RELATIVE "${ASC_CPP_SOURCE_DIR}"
  "${ASC_CPP_SOURCE_DIR}/src/core/runtime/*.cc"
  "${ASC_CPP_SOURCE_DIR}/src/core/providers/*.cc"
)
list(APPEND _canonical_headers ${_canonical_sources})
string(CONCAT _legacy_header_pattern
  "asc/core/(globals|error|memory|memory_impl|device|forall|cuda|operators)"
  "\\.h"
)

foreach(_relative_path IN LISTS _canonical_headers)
  set(_path "${ASC_CPP_SOURCE_DIR}/${_relative_path}")
  if(NOT EXISTS "${_path}")
    message(FATAL_ERROR "Canonical core file is missing: ${_relative_path}")
  endif()
  file(READ "${_path}" _contents)
  if(_contents MATCHES
     "asc/(utilities|array|linalg|random)/")
    message(FATAL_ERROR
      "Canonical core file depends on a higher component: ${_relative_path}"
    )
  endif()
  if(_contents MATCHES "${_legacy_header_pattern}")
    message(FATAL_ERROR
      "Canonical core file depends on the legacy runtime: ${_relative_path}"
    )
  endif()
  if(_contents MATCHES
     "[#]include[ \t]*[<\"](cuda|omp[.]h|Eigen|mkl|petsc|Kokkos)")
    message(FATAL_ERROR
      "Canonical core file exposes a provider SDK header: ${_relative_path}"
    )
  endif()
endforeach()
