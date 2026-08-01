add_library(asc_random)
add_library(ASC::random ALIAS asc_random)

target_sources(
  asc_random
  PRIVATE
    distribution.cc
    engine.cc
    seed.cc
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
    FILES
      "${PROJECT_SOURCE_DIR}/include/asc/random.h"
      "${PROJECT_SOURCE_DIR}/include/asc/random/distribution.h"
      "${PROJECT_SOURCE_DIR}/include/asc/random/engine.h"
      "${PROJECT_SOURCE_DIR}/include/asc/random/export.h"
      "${PROJECT_SOURCE_DIR}/include/asc/random/generator.h"
      "${PROJECT_SOURCE_DIR}/include/asc/random/seed.h"
)

set_target_properties(
  asc_random
  PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    EXPORT_NAME random
    OUTPUT_NAME asc_random
    VISIBILITY_INLINES_HIDDEN YES
)
if(APPLE)
  set_property(TARGET asc_random PROPERTY INSTALL_RPATH "@loader_path")
elseif(UNIX)
  set_property(TARGET asc_random PROPERTY INSTALL_RPATH "$ORIGIN")
endif()

target_compile_definitions(asc_random PRIVATE ASC_RANDOM_BUILDING_LIBRARY)
if(NOT BUILD_SHARED_LIBS)
  target_compile_definitions(asc_random PUBLIC ASC_RANDOM_STATIC_DEFINE)
endif()

target_link_libraries(asc_random PUBLIC ASC::core)
asc_target_enable_cxx20(TARGET asc_random PUBLIC)
if(ASC_CPP_WARNINGS_AS_ERRORS)
  asc_target_enable_warnings(TARGET asc_random AS_ERRORS)
else()
  asc_target_enable_warnings(TARGET asc_random)
endif()
if(ASC_CPP_SANITIZER_ARGUMENTS)
  asc_target_enable_sanitizers(
    TARGET asc_random
    ${ASC_CPP_SANITIZER_ARGUMENTS}
  )
endif()

add_library(asc_random_dense INTERFACE)
add_library(ASC::random_dense ALIAS asc_random_dense)
target_sources(
  asc_random_dense
  INTERFACE
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
    FILES "${PROJECT_SOURCE_DIR}/include/asc/random/dense.h"
)
set_target_properties(
  asc_random_dense
  PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    EXPORT_NAME random_dense
)
target_link_libraries(asc_random_dense INTERFACE ASC::random ASC::dense)
target_compile_features(asc_random_dense INTERFACE cxx_std_20)

add_library(asc_random_sparse INTERFACE)
add_library(ASC::random_sparse ALIAS asc_random_sparse)
target_sources(
  asc_random_sparse
  INTERFACE
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
    FILES "${PROJECT_SOURCE_DIR}/include/asc/random/sparse.h"
)
set_target_properties(
  asc_random_sparse
  PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    EXPORT_NAME random_sparse
)
target_link_libraries(asc_random_sparse INTERFACE ASC::random ASC::sparse)
target_compile_features(asc_random_sparse INTERFACE cxx_std_20)

if(ASC_CPP_ENABLE_CUDA)
  function(
    _asc_random_configure_cuda_target
    target
    export_name
    output_name
    building_definition
    static_definition
  )
    set_target_properties(
      "${target}"
      PROPERTIES
        CUDA_EXTENSIONS OFF
        CUDA_STANDARD 20
        CUDA_STANDARD_REQUIRED ON
        CUDA_RUNTIME_LIBRARY Shared
        CUDA_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        EXPORT_NAME "${export_name}"
        OUTPUT_NAME "${output_name}"
        VISIBILITY_INLINES_HIDDEN YES
    )
    if(APPLE)
      set_property(TARGET "${target}" PROPERTY INSTALL_RPATH "@loader_path")
    elseif(UNIX)
      set_property(TARGET "${target}" PROPERTY INSTALL_RPATH "$ORIGIN")
    endif()

    target_compile_definitions("${target}" PRIVATE "${building_definition}")
    if(NOT BUILD_SHARED_LIBS)
      target_compile_definitions("${target}" PUBLIC "${static_definition}")
    endif()

    asc_target_enable_cxx20(TARGET "${target}" PUBLIC)
    if(ASC_CPP_WARNINGS_AS_ERRORS)
      asc_target_enable_warnings(TARGET "${target}" AS_ERRORS)
    else()
      asc_target_enable_warnings(TARGET "${target}")
    endif()
    if(ASC_CPP_SANITIZER_ARGUMENTS)
      asc_target_enable_sanitizers(
        TARGET "${target}"
        ${ASC_CPP_SANITIZER_ARGUMENTS}
      )
    endif()
  endfunction()

  add_library(asc_random_cuda)
  add_library(ASC::random_cuda ALIAS asc_random_cuda)
  target_sources(
    asc_random_cuda
    PRIVATE
      cuda/raw.cc
      cuda/raw_kernels.cu
    PUBLIC
      FILE_SET HEADERS
      BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
      FILES
        "${PROJECT_SOURCE_DIR}/include/asc/random/providers/cuda.h"
        "${PROJECT_SOURCE_DIR}/include/asc/random/providers/cuda_export.h"
  )
  target_include_directories(
    asc_random_cuda
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}"
      "${PROJECT_SOURCE_DIR}/src/core"
      "${CUDAToolkit_INCLUDE_DIRS}"
  )
  target_link_libraries(
    asc_random_cuda
    PUBLIC
      ASC::random
      ASC::core_cuda
  )
  _asc_random_configure_cuda_target(
    asc_random_cuda
    random_cuda
    asc_random_cuda
    ASC_RANDOM_CUDA_BUILDING_LIBRARY
    ASC_RANDOM_CUDA_STATIC_DEFINE
  )

  add_library(asc_random_dense_cuda)
  add_library(ASC::random_dense_cuda ALIAS asc_random_dense_cuda)
  target_sources(
    asc_random_dense_cuda
    PRIVATE
      cuda/dense.cc
      cuda/dense_kernels.cu
    PUBLIC
      FILE_SET HEADERS
      BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
      FILES
        "${PROJECT_SOURCE_DIR}/include/asc/random/providers/dense_cuda.h"
        "${PROJECT_SOURCE_DIR}/include/asc/random/providers/dense_cuda_export.h"
  )
  target_include_directories(
    asc_random_dense_cuda
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}"
      "${PROJECT_SOURCE_DIR}/src/core"
      "${CUDAToolkit_INCLUDE_DIRS}"
  )
  target_link_libraries(
    asc_random_dense_cuda
    PUBLIC
      ASC::random_dense
      ASC::random_cuda
      ASC::core_cuda
  )
  _asc_random_configure_cuda_target(
    asc_random_dense_cuda
    random_dense_cuda
    asc_random_dense_cuda
    ASC_RANDOM_DENSE_CUDA_BUILDING_LIBRARY
    ASC_RANDOM_DENSE_CUDA_STATIC_DEFINE
  )

  add_library(asc_random_sparse_cuda)
  add_library(ASC::random_sparse_cuda ALIAS asc_random_sparse_cuda)
  target_sources(
    asc_random_sparse_cuda
    PRIVATE
      cuda/sparse.cc
      cuda/sparse_kernels.cu
    PUBLIC
      FILE_SET HEADERS
      BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
      FILES
        "${PROJECT_SOURCE_DIR}/include/asc/random/providers/sparse_cuda.h"
        "${PROJECT_SOURCE_DIR}/include/asc/random/providers/sparse_cuda_export.h"
  )
  target_include_directories(
    asc_random_sparse_cuda
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}"
      "${PROJECT_SOURCE_DIR}/src/core"
      "${CUDAToolkit_INCLUDE_DIRS}"
  )
  target_link_libraries(
    asc_random_sparse_cuda
    PUBLIC
      ASC::random_sparse
      ASC::random_cuda
      ASC::core_cuda
  )
  _asc_random_configure_cuda_target(
    asc_random_sparse_cuda
    random_sparse_cuda
    asc_random_sparse_cuda
    ASC_RANDOM_SPARSE_CUDA_BUILDING_LIBRARY
    ASC_RANDOM_SPARSE_CUDA_STATIC_DEFINE
  )
endif()
