add_library(asc_dense_lapack)
add_library(ASC::dense_lapack ALIAS asc_dense_lapack)
target_sources(asc_dense_lapack PRIVATE
  reference_lu.cc reference_lu_expert.cc reference_lu_equilibration.cc
  reference_lu_condition.cc reference_lu_refinement.cc reference_lu_driver.cc
  PUBLIC FILE_SET HEADERS BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
  FILES
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_export.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_lu.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_lu_equilibration.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_lu_condition.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_lu_refinement.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_lu_driver.h")
target_include_directories(asc_dense_lapack PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
set_target_properties(asc_dense_lapack PROPERTIES
  CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES
  EXPORT_NAME dense_lapack OUTPUT_NAME asc_dense_lapack)
target_compile_definitions(asc_dense_lapack
  PRIVATE ASC_DENSE_LAPACK_BUILDING_LIBRARY
  PUBLIC ASC_DENSE_LAPACK_STATIC_DEFINE)
target_link_libraries(asc_dense_lapack PUBLIC ASC::dense
  PRIVATE ASC_INTERNAL_LAPACK::reference)
asc_target_enable_cxx20(TARGET asc_dense_lapack PUBLIC)
if(ASC_CPP_WARNINGS_AS_ERRORS)
  asc_target_enable_warnings(TARGET asc_dense_lapack AS_ERRORS)
else()
  asc_target_enable_warnings(TARGET asc_dense_lapack)
endif()
if(ASC_CPP_SANITIZER_ARGUMENTS)
  asc_target_enable_sanitizers(TARGET asc_dense_lapack
    ${ASC_CPP_SANITIZER_ARGUMENTS})
endif()
