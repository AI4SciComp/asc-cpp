add_library(asc_sparse)
add_library(ASC::sparse ALIAS asc_sparse)

target_sources(
  asc_sparse
  PRIVATE
    reference_blas.cc
    standard_blas.cc
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
    FILES
      "${PROJECT_SOURCE_DIR}/include/asc/sparse.h"
      "${PROJECT_SOURCE_DIR}/include/asc/sparse/compressed.h"
      "${PROJECT_SOURCE_DIR}/include/asc/sparse/coordinate.h"
      "${PROJECT_SOURCE_DIR}/include/asc/sparse/evaluate.h"
      "${PROJECT_SOURCE_DIR}/include/asc/sparse/export.h"
      "${PROJECT_SOURCE_DIR}/include/asc/sparse/blas.h"
)

set_target_properties(
  asc_sparse
  PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    EXPORT_NAME sparse
    OUTPUT_NAME asc_sparse
    VISIBILITY_INLINES_HIDDEN YES
)
if(APPLE)
  set_property(TARGET asc_sparse PROPERTY INSTALL_RPATH "@loader_path")
elseif(UNIX)
  set_property(TARGET asc_sparse PROPERTY INSTALL_RPATH "$ORIGIN")
endif()

target_compile_definitions(asc_sparse PRIVATE ASC_SPARSE_BUILDING_LIBRARY)
if(NOT BUILD_SHARED_LIBS)
  target_compile_definitions(asc_sparse PUBLIC ASC_SPARSE_STATIC_DEFINE)
endif()

target_link_libraries(asc_sparse PUBLIC ASC::core ASC::expression)
asc_target_enable_cxx20(TARGET asc_sparse PUBLIC)
if(ASC_CPP_WARNINGS_AS_ERRORS)
  asc_target_enable_warnings(TARGET asc_sparse AS_ERRORS)
else()
  asc_target_enable_warnings(TARGET asc_sparse)
endif()
if(ASC_CPP_SANITIZER_ARGUMENTS)
  asc_target_enable_sanitizers(
    TARGET asc_sparse
    ${ASC_CPP_SANITIZER_ARGUMENTS}
  )
endif()

if(ASC_CPP_ENABLE_CUDA)
  add_library(asc_sparse_cuda)
  add_library(ASC::sparse_cuda ALIAS asc_sparse_cuda)

  target_sources(
    asc_sparse_cuda
    PRIVATE
      cuda/context.cc
      cuda/kernels.cu
      cuda/operations.cc
    PUBLIC
      FILE_SET HEADERS
      BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
      FILES
        "${PROJECT_SOURCE_DIR}/include/asc/sparse/providers/cuda.h"
        "${PROJECT_SOURCE_DIR}/include/asc/sparse/providers/cuda_export.h"
  )
  target_include_directories(
    asc_sparse_cuda
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}"
      "${PROJECT_SOURCE_DIR}/src/core"
  )

  set_target_properties(
    asc_sparse_cuda
    PROPERTIES
      CUDA_EXTENSIONS OFF
      CUDA_STANDARD 20
      CUDA_STANDARD_REQUIRED ON
      CUDA_RUNTIME_LIBRARY Shared
      CUDA_VISIBILITY_PRESET hidden
      CXX_VISIBILITY_PRESET hidden
      EXPORT_NAME sparse_cuda
      OUTPUT_NAME asc_sparse_cuda
      VISIBILITY_INLINES_HIDDEN YES
  )
  if(APPLE)
    set_property(TARGET asc_sparse_cuda PROPERTY INSTALL_RPATH "@loader_path")
  elseif(UNIX)
    set_property(TARGET asc_sparse_cuda PROPERTY INSTALL_RPATH "$ORIGIN")
  endif()

  target_compile_definitions(
    asc_sparse_cuda
    PRIVATE ASC_SPARSE_CUDA_BUILDING_LIBRARY
  )
  if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(
      asc_sparse_cuda
      PUBLIC ASC_SPARSE_CUDA_STATIC_DEFINE
    )
  endif()

  target_link_libraries(
    asc_sparse_cuda
    PUBLIC
      ASC::sparse
      ASC::core_cuda
    PRIVATE CUDA::cusparse
  )
  asc_target_enable_cxx20(TARGET asc_sparse_cuda PUBLIC)
  if(ASC_CPP_WARNINGS_AS_ERRORS)
    asc_target_enable_warnings(TARGET asc_sparse_cuda AS_ERRORS)
  else()
    asc_target_enable_warnings(TARGET asc_sparse_cuda)
  endif()
  if(ASC_CPP_SANITIZER_ARGUMENTS)
    asc_target_enable_sanitizers(
      TARGET asc_sparse_cuda
      ${ASC_CPP_SANITIZER_ARGUMENTS}
    )
  endif()
endif()
