add_library(asc_dense)
add_library(ASC::dense ALIAS asc_dense)

target_sources(
  asc_dense
  PRIVATE
    array_io.cc
    blas.cc
    blas_level1.cc
    blas_level2.cc
    blas_level3.cc
    lapack_foundations.cc
    lapack_cholesky.cc
    lapack_lu.cc
    lapack_qr.cc
    matrix_market.cc
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
    FILES
      "${PROJECT_SOURCE_DIR}/include/asc/dense.h"
      "${PROJECT_SOURCE_DIR}/include/asc/dense/array.h"
      "${PROJECT_SOURCE_DIR}/include/asc/dense/evaluate.h"
      "${PROJECT_SOURCE_DIR}/include/asc/dense/export.h"
      "${PROJECT_SOURCE_DIR}/include/asc/dense/io.h"
      "${PROJECT_SOURCE_DIR}/include/asc/dense/layout.h"
      "${PROJECT_SOURCE_DIR}/include/asc/dense/matrix_market.h"
      "${PROJECT_SOURCE_DIR}/include/asc/dense/blas.h"
      "${PROJECT_SOURCE_DIR}/include/asc/dense/view.h"
      "${PROJECT_SOURCE_DIR}/include/asc/dense/print.h"
      "${PROJECT_SOURCE_DIR}/include/asc/dense/lapack/types.h"
      "${PROJECT_SOURCE_DIR}/include/asc/dense/lapack/workspace.h"
      "${PROJECT_SOURCE_DIR}/include/asc/dense/lapack/report.h"
      "${PROJECT_SOURCE_DIR}/include/asc/dense/lapack/factor_view.h"
      "${PROJECT_SOURCE_DIR}/include/asc/dense/lapack/structured_view.h"
      "${PROJECT_SOURCE_DIR}/include/asc/dense/lapack/lu.h"
      "${PROJECT_SOURCE_DIR}/include/asc/dense/lapack/cholesky.h"
      "${PROJECT_SOURCE_DIR}/include/asc/dense/lapack/qr.h"
)

set_target_properties(
  asc_dense
  PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    EXPORT_NAME dense
    OUTPUT_NAME asc_dense
    VISIBILITY_INLINES_HIDDEN YES
)
if(APPLE)
  set_property(TARGET asc_dense PROPERTY INSTALL_RPATH "@loader_path")
elseif(UNIX)
  set_property(TARGET asc_dense PROPERTY INSTALL_RPATH "$ORIGIN")
endif()

target_compile_definitions(asc_dense PRIVATE ASC_DENSE_BUILDING_LIBRARY)
if(NOT BUILD_SHARED_LIBS)
  target_compile_definitions(asc_dense PUBLIC ASC_DENSE_STATIC_DEFINE)
endif()

target_link_libraries(asc_dense PUBLIC ASC::core ASC::expression)
asc_target_enable_cxx20(TARGET asc_dense PUBLIC)
if(ASC_CPP_WARNINGS_AS_ERRORS)
  asc_target_enable_warnings(TARGET asc_dense AS_ERRORS)
else()
  asc_target_enable_warnings(TARGET asc_dense)
endif()
if(ASC_CPP_SANITIZER_ARGUMENTS)
  asc_target_enable_sanitizers(
    TARGET asc_dense
    ${ASC_CPP_SANITIZER_ARGUMENTS}
  )
endif()

if(ASC_CPP_ENABLE_LAPACK)
  add_subdirectory(lapack)
endif()

if(ASC_CPP_ENABLE_CUDA)
  add_library(asc_dense_cuda)
  add_library(ASC::dense_cuda ALIAS asc_dense_cuda)

  target_sources(
    asc_dense_cuda
    PRIVATE
      cuda/context.cc
      cuda/blas_level1.cc
      cuda/blas_level1_kernels.cu
      cuda/blas_level2.cc
      cuda/blas_level2_kernels.cu
      cuda/blas_level3.cc
      cuda/kernels.cu
      cuda/operations.cc
    PUBLIC
      FILE_SET HEADERS
      BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
      FILES
        "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/cuda.h"
        "${PROJECT_SOURCE_DIR}/include/asc/dense/providers/cuda_export.h"
  )
  target_include_directories(
    asc_dense_cuda
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}"
      "${PROJECT_SOURCE_DIR}/src/core"
      "${PROJECT_SOURCE_DIR}/src/dense"
  )

  set_target_properties(
    asc_dense_cuda
    PROPERTIES
      CUDA_EXTENSIONS OFF
      CUDA_STANDARD 20
      CUDA_STANDARD_REQUIRED ON
      CUDA_RUNTIME_LIBRARY Shared
      CUDA_VISIBILITY_PRESET hidden
      CXX_VISIBILITY_PRESET hidden
      EXPORT_NAME dense_cuda
      OUTPUT_NAME asc_dense_cuda
      VISIBILITY_INLINES_HIDDEN YES
  )
  if(APPLE)
    set_property(TARGET asc_dense_cuda PROPERTY INSTALL_RPATH "@loader_path")
  elseif(UNIX)
    set_property(TARGET asc_dense_cuda PROPERTY INSTALL_RPATH "$ORIGIN")
  endif()

  target_compile_definitions(
    asc_dense_cuda
    PRIVATE ASC_DENSE_CUDA_BUILDING_LIBRARY
  )
  if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(
      asc_dense_cuda
      PUBLIC ASC_DENSE_CUDA_STATIC_DEFINE
    )
  endif()

  target_link_libraries(
    asc_dense_cuda
    PUBLIC
      ASC::dense
      ASC::core_cuda
    PRIVATE CUDA::cublas
  )
  asc_target_enable_cxx20(TARGET asc_dense_cuda PUBLIC)
  if(ASC_CPP_WARNINGS_AS_ERRORS)
    asc_target_enable_warnings(TARGET asc_dense_cuda AS_ERRORS)
  else()
    asc_target_enable_warnings(TARGET asc_dense_cuda)
  endif()
  if(ASC_CPP_SANITIZER_ARGUMENTS)
    asc_target_enable_sanitizers(
      TARGET asc_dense_cuda
      ${ASC_CPP_SANITIZER_ARGUMENTS}
    )
  endif()
endif()
