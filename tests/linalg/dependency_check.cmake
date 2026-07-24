if(NOT DEFINED ASC_CPP_SOURCE_DIR)
  message(FATAL_ERROR "ASC_CPP_SOURCE_DIR is required")
endif()

set(_canonical_files
  "${ASC_CPP_SOURCE_DIR}/include/asc/linalg.h"
  "${ASC_CPP_SOURCE_DIR}/include/asc/linalg/blas1.h"
  "${ASC_CPP_SOURCE_DIR}/include/asc/linalg/blas2.h"
  "${ASC_CPP_SOURCE_DIR}/include/asc/linalg/blas3.h"
  "${ASC_CPP_SOURCE_DIR}/include/asc/linalg/capabilities.h"
  "${ASC_CPP_SOURCE_DIR}/include/asc/linalg/concepts.h"
  "${ASC_CPP_SOURCE_DIR}/include/asc/linalg/detail/reference_kernels.h"
  "${ASC_CPP_SOURCE_DIR}/include/asc/linalg/types.h"
  "${ASC_CPP_SOURCE_DIR}/src/linalg/runtime/capabilities.cc"
  "${ASC_CPP_SOURCE_DIR}/src/linalg/providers/serial/reference_kernels.cc"
)

set(_forbidden_asc_component
  "#[ \t]*include[ \t]*[<\"]asc/(utilities|random|cpp|asc)(/|\\.|[>\"])"
)
set(_forbidden_legacy_core
  "#[ \t]*include[ \t]*[<\"]asc/core/(casts|cuda|device|error|forall|globals|math|memory|memory_impl|numeric|operators|string)(\\.|/|[>\"])"
)
set(_forbidden_legacy_array
  "#[ \t]*include[ \t]*[<\"]asc/array/(carray|concepts|dsmarray|dsmarray_impl|expr|forwards|marray|mindex|miterator|mlayout|mobject|mobject_impl|mshape|spmarray|spmarray_impl|spmindex|spmiterator|spmlayout|uarray)\\.h"
)
set(_forbidden_legacy_linalg
  "#[ \t]*include[ \t]*[<\"]asc/linalg/(blas|decomp|eigen|lapack)\\.h"
)
set(_forbidden_sdk_include
  "#[ \t]*include[ \t]*[<\"](cuda|cublas|cusparse|omp\\.h|Eigen/|mkl|petsc|Kokkos|cblas|lapacke)"
)

foreach(_file IN LISTS _canonical_files)
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "Missing canonical Linalg file: ${_file}")
  endif()
  file(READ "${_file}" _contents)
  if(_contents MATCHES "${_forbidden_asc_component}")
    message(FATAL_ERROR
      "Canonical Linalg file includes a forbidden ASC component: ${_file}"
    )
  endif()
  if(_contents MATCHES "${_forbidden_legacy_core}")
    message(FATAL_ERROR
      "Canonical Linalg file includes legacy Core runtime: ${_file}"
    )
  endif()
  if(_contents MATCHES "${_forbidden_legacy_array}")
    message(FATAL_ERROR
      "Canonical Linalg file includes legacy Array compatibility: ${_file}"
    )
  endif()
  if(_contents MATCHES "${_forbidden_legacy_linalg}")
    message(FATAL_ERROR
      "Canonical Linalg file includes legacy Linalg compatibility: ${_file}"
    )
  endif()
  if(_contents MATCHES "${_forbidden_sdk_include}")
    message(FATAL_ERROR
      "Canonical Linalg file includes an optional provider SDK: ${_file}"
    )
  endif()
  if(_contents MATCHES "namespace[ \t]+asc::linalg")
    message(FATAL_ERROR
      "Canonical Linalg declarations must use the flat asc namespace: ${_file}"
    )
  endif()
endforeach()
