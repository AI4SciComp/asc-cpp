set(_canonical_files
  "${ASC_CPP_SOURCE_DIR}/include/asc/utilities.h"
  "${ASC_CPP_SOURCE_DIR}/include/asc/utilities/cli.h"
  "${ASC_CPP_SOURCE_DIR}/include/asc/utilities/config.h"
  "${ASC_CPP_SOURCE_DIR}/include/asc/utilities/timer.h"
  "${ASC_CPP_SOURCE_DIR}/src/utilities/cli.cc"
  "${ASC_CPP_SOURCE_DIR}/src/utilities/config.cc"
  "${ASC_CPP_SOURCE_DIR}/src/utilities/timer.cc"
)

set(_forbidden_asc_include
  "#[ \t]*include[ \t]*[<\"]asc/(array|linalg|random|cpp|asc)(/|\\.|[>\"])"
)
set(_forbidden_legacy_core
  "#[ \t]*include[ \t]*[<\"]asc/core/(casts|cuda|device|error|forall|globals|math|memory|numeric|operators|string)(\\.|/|[>\"])"
)
set(_forbidden_sdk_include
  "#[ \t]*include[ \t]*[<\"](cuda|cublas|cusparse|omp\\.h|Eigen/|mkl|petsc|Kokkos)"
)

foreach(_file IN LISTS _canonical_files)
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "Missing canonical Utilities file: ${_file}")
  endif()
  file(READ "${_file}" _contents)
  if(_contents MATCHES "${_forbidden_asc_include}")
    message(FATAL_ERROR
      "Canonical Utilities file includes a higher ASC component: ${_file}"
    )
  endif()
  if(_contents MATCHES "${_forbidden_legacy_core}")
    message(FATAL_ERROR
      "Canonical Utilities file includes legacy Core runtime: ${_file}"
    )
  endif()
  if(_contents MATCHES "${_forbidden_sdk_include}")
    message(FATAL_ERROR
      "Canonical Utilities file includes an optional provider SDK: ${_file}"
    )
  endif()
endforeach()
