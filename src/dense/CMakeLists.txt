add_library(asc_dense)
add_library(ASC::dense ALIAS asc_dense)

target_sources(
  asc_dense
  PRIVATE
    reference_linalg.cc
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
    FILES
      "${PROJECT_SOURCE_DIR}/include/asc/dense.h"
      "${PROJECT_SOURCE_DIR}/include/asc/dense/array.h"
      "${PROJECT_SOURCE_DIR}/include/asc/dense/evaluate.h"
      "${PROJECT_SOURCE_DIR}/include/asc/dense/export.h"
      "${PROJECT_SOURCE_DIR}/include/asc/dense/layout.h"
      "${PROJECT_SOURCE_DIR}/include/asc/dense/linalg.h"
      "${PROJECT_SOURCE_DIR}/include/asc/dense/view.h"
)

set_target_properties(
  asc_dense
  PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    EXPORT_NAME dense
    OUTPUT_NAME asc_dense
    VISIBILITY_INLINES_HIDDEN YES
)

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

if(ASC_CPP_ENABLE_CUDA)
  add_library(asc_dense_cuda)
  add_library(ASC::dense_cuda ALIAS asc_dense_cuda)

  target_sources(
    asc_dense_cuda
    PRIVATE
      cuda/cuda.cc
      cuda/kernels.cu
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
      "${PROJECT_SOURCE_DIR}/src"
  )
  target_link_libraries(
    asc_dense_cuda
    PUBLIC
      ASC::dense
      ASC::core_cuda
    PRIVATE
      CUDA::cublas
  )

  set_target_properties(
    asc_dense_cuda
    PROPERTIES
      CUDA_RUNTIME_LIBRARY Shared
      CUDA_EXTENSIONS OFF
      CUDA_VISIBILITY_PRESET hidden
      CXX_VISIBILITY_PRESET hidden
      EXPORT_NAME dense_cuda
      OUTPUT_NAME asc_dense_cuda
      VISIBILITY_INLINES_HIDDEN YES
  )
  if("cuda_std_20" IN_LIST CMAKE_CUDA_COMPILE_FEATURES)
    set_target_properties(
      asc_dense_cuda
      PROPERTIES
        CUDA_STANDARD 20
        CUDA_STANDARD_REQUIRED ON
    )
  elseif(CMAKE_CUDA_COMPILER_ID STREQUAL "NVIDIA"
         AND CMAKE_CUDA_COMPILER_VERSION VERSION_GREATER_EQUAL 12.0)
    # CMake 3.25's CUDA feature table ends at C++17, including when it
    # identifies an NVCC 12 compiler that supports C++20. Keep the project
    # minimum usable without weakening the CUDA source-language contract.
    target_compile_options(
      asc_dense_cuda
      PRIVATE "$<$<COMPILE_LANGUAGE:CUDA>:--std=c++20>"
    )
  else()
    message(FATAL_ERROR
      "Milestone 6 requires a CUDA compiler with C++20 support known to "
      "CMake, or NVIDIA CUDA compiler 12 or newer."
    )
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
