add_library(asc_linalg
  providers/serial/reference_kernels.cc
  runtime/capabilities.cc
)
add_library(ASC::linalg ALIAS asc_linalg)
asc_cpp_configure_compiled_target(asc_linalg linalg)
target_link_libraries(asc_linalg PUBLIC asc_array asc_core)
target_sources(
  asc_linalg
  PUBLIC FILE_SET public_headers TYPE HEADERS
  BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
  FILES
    "${PROJECT_SOURCE_DIR}/include/asc/linalg.h"
    "${PROJECT_SOURCE_DIR}/include/asc/linalg/blas.h"
    "${PROJECT_SOURCE_DIR}/include/asc/linalg/blas1.h"
    "${PROJECT_SOURCE_DIR}/include/asc/linalg/blas2.h"
    "${PROJECT_SOURCE_DIR}/include/asc/linalg/blas3.h"
    "${PROJECT_SOURCE_DIR}/include/asc/linalg/capabilities.h"
    "${PROJECT_SOURCE_DIR}/include/asc/linalg/concepts.h"
    "${PROJECT_SOURCE_DIR}/include/asc/linalg/decomp.h"
    "${PROJECT_SOURCE_DIR}/include/asc/linalg/detail/reference_kernels.h"
    "${PROJECT_SOURCE_DIR}/include/asc/linalg/eigen.h"
    "${PROJECT_SOURCE_DIR}/include/asc/linalg/lapack.h"
    "${PROJECT_SOURCE_DIR}/include/asc/linalg/types.h"
)

if(ASC_CPP_ENABLE_EIGEN)
  target_link_libraries(asc_linalg PUBLIC Eigen3::Eigen)
endif()
if(ASC_CPP_ENABLE_MKL)
  target_link_libraries(asc_linalg PUBLIC MKL::MKL)
endif()
