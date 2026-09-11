add_library(asc_dense_lapack)
add_library(ASC::dense_lapack ALIAS asc_dense_lapack)
if(ASC_CPP_ENABLE_EXPERIMENTAL_ROBUST_PPSVX)
  target_sources(asc_dense_lapack PRIVATE robust_cholesky_packed_expert.cc)
endif()
target_sources(asc_dense_lapack PRIVATE
  reference_lu.cc reference_lu_expert.cc reference_lu_equilibration.cc
  reference_lu_condition.cc reference_lu_refinement.cc reference_lu_driver.cc
  reference_lu_helpers.cc reference_cholesky.cc reference_qr.cc
  reference_cholesky_condition.cc
  reference_cholesky_driver.cc
  reference_cholesky_equilibration.cc
  reference_cholesky_refinement.cc
  reference_least_squares.cc
  reference_cholesky_band.cc
  reference_indefinite.cc
  reference_rank_revealing.cc
  reference_sylvester.cc
  reference_indefinite_condition.cc
  reference_indefinite_driver.cc
  reference_indefinite_expert_driver.cc
  reference_indefinite_refinement.cc
  reference_svd_least_squares.cc
  reference_lu_band.cc
  reference_lu_band_condition.cc
  reference_lu_band_driver.cc
  reference_lu_band_equilibration.cc
  reference_lu_band_equilibration_radix.cc
  reference_lu_band_refinement.cc
  reference_lu_band_solve.cc
  reference_tridiagonal.cc
  reference_tridiagonal_condition.cc
  reference_tridiagonal_driver.cc
  reference_tridiagonal_expert_driver.cc
  reference_tridiagonal_refinement.cc
  reference_cholesky_band_condition.cc
  reference_cholesky_band_driver.cc
  reference_cholesky_band_equilibration.cc
  reference_cholesky_band_expert.cc
  reference_cholesky_band_refinement.cc
  reference_triangular.cc
  reference_triangular_expert.cc reference_packed_triangular.cc
  reference_packed_triangular_expert.cc reference_triangular_band.cc
  reference_triangular_band_expert.cc reference_cholesky_packed.cc
  reference_cholesky_packed_solve.cc
  reference_cholesky_packed_inverse.cc
  reference_cholesky_packed_driver.cc
  reference_cholesky_packed_equilibration.cc
  reference_cholesky_packed_condition.cc
  reference_cholesky_packed_refinement.cc
  reference_mixed_positive.cc
  reference_precision_conversion.cc
  reference_matrix_copy.cc
  reference_matrix_scale.cc
  reference_matrix_set.cc
  reference_dmd.cc
  reference_dmd_qr.cc
  reference_mixed_general.cc
  reference_positive_tridiagonal.cc
  PUBLIC FILE_SET HEADERS BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
  FILES
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_export.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_lu.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_lu_equilibration.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_lu_condition.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_lu_refinement.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_lu_driver.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_lu_helpers.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_cholesky.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_cholesky_packed.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_cholesky_packed_solve.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_cholesky_packed_inverse.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_cholesky_packed_driver.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_cholesky_packed_equilibration.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_cholesky_packed_condition.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_cholesky_packed_refinement.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_cholesky_packed_robust.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_qr.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_cholesky_condition.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_cholesky_driver.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_cholesky_equilibration.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_cholesky_refinement.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_least_squares.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_cholesky_band.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_indefinite.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_rank_revealing.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_sylvester.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_indefinite_condition.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_indefinite_driver.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_indefinite_refinement.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_svd_least_squares.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_lu_band.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_general_band.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_lu_band_condition.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_lu_band_driver.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_lu_band_equilibration.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_lu_band_equilibration_radix.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_lu_band_expert.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_lu_band_refinement.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_mixed_positive.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_precision_conversion.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_matrix_copy.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_matrix_scale.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_matrix_set.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_dmd.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_dmd_qr.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_mixed_general.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_positive_tridiagonal.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_tridiagonal.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_tridiagonal_condition.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_tridiagonal_driver.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_tridiagonal_refinement.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_cholesky_band_condition.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_cholesky_band_driver.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_cholesky_band_equilibration.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_cholesky_band_expert.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_cholesky_band_refinement.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_triangular.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_triangular_condition.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_triangular_error_bounds.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_triangular_packed.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_triangular_band.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_triangular_band_condition.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_triangular_band_error_bounds.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_triangular_packed_condition.h"
    "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/lapack_triangular_packed_error_bounds.h")
target_include_directories(asc_dense_lapack PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
set_target_properties(asc_dense_lapack PROPERTIES
  CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES
  EXPORT_NAME dense_lapack OUTPUT_NAME asc_dense_lapack)
target_compile_definitions(asc_dense_lapack
  PRIVATE ASC_DENSE_LAPACK_BUILDING_LIBRARY)
if(NOT BUILD_SHARED_LIBS)
  target_compile_definitions(asc_dense_lapack PUBLIC ASC_DENSE_LAPACK_STATIC_DEFINE)
endif()
if(BUILD_SHARED_LIBS)
  # Resolve installed ASC dependencies after relocation, as Dense does.
  set_property(TARGET asc_dense_lapack PROPERTY INSTALL_RPATH "$ORIGIN")
  target_link_options(asc_dense_lapack PRIVATE "LINKER:--no-undefined" "LINKER:-z,text")
endif()
target_link_libraries(asc_dense_lapack PUBLIC ASC::dense
  PRIVATE ASC_INTERNAL_LAPACK::reference)
if(BUILD_SHARED_LIBS)
  target_link_libraries(asc_dense_lapack INTERFACE
    "$<LINK_ONLY:ASC_INTERNAL_LAPACK::reference>")
endif()
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

if(BUILD_SHARED_LIBS)
  # Production and observation libraries use exactly the same compiled PIC
  # objects. Observation changes only link-time foreign-entry/allocator binding.
  # The object target is private and is never installed or exported.
  get_target_property(_asc_lapack_sources asc_dense_lapack SOURCES)
  add_library(asc_dense_lapack_objects OBJECT ${_asc_lapack_sources})
  set_property(TARGET asc_dense_lapack PROPERTY SOURCES
    "$<TARGET_OBJECTS:asc_dense_lapack_objects>")
  set_target_properties(asc_dense_lapack_objects PROPERTIES
    POSITION_INDEPENDENT_CODE YES CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS OFF)
  foreach(_property IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS
      COMPILE_FEATURES)
    set_property(TARGET asc_dense_lapack_objects PROPERTY "${_property}"
      "$<TARGET_PROPERTY:asc_dense_lapack,${_property}>")
  endforeach()
  target_compile_definitions(asc_dense_lapack_objects PRIVATE asc_dense_lapack_EXPORTS)
  unset(_asc_lapack_sources)
  unset(_property)
endif()
