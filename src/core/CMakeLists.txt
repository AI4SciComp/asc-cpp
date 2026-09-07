add_library(asc_core)
add_library(ASC::core ALIAS asc_core)

target_sources(
  asc_core
  PRIVATE
    array_format.cc
    array_io.cc
    configuration.cc
    contracts.cc
    execution.cc
    io.cc
    memory.cc
    status.cc
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
    FILES
      "${PROJECT_SOURCE_DIR}/include/asc/core.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/array_format.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/array_io.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/configuration.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/contracts.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/execution.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/export.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/extents.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/io.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/memory.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/result.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/status.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/types.h"
)
target_include_directories(asc_core PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

set_target_properties(
  asc_core
  PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    EXPORT_NAME core
    OUTPUT_NAME asc_core
    VISIBILITY_INLINES_HIDDEN YES
)

target_compile_definitions(asc_core PRIVATE ASC_CORE_BUILDING_LIBRARY)
if(NOT BUILD_SHARED_LIBS)
  target_compile_definitions(asc_core PUBLIC ASC_CORE_STATIC_DEFINE)
endif()

asc_target_enable_cxx20(TARGET asc_core PUBLIC)
if(ASC_CPP_WARNINGS_AS_ERRORS)
  asc_target_enable_warnings(TARGET asc_core AS_ERRORS)
else()
  asc_target_enable_warnings(TARGET asc_core)
endif()
if(ASC_CPP_SANITIZER_ARGUMENTS)
  asc_target_enable_sanitizers(
    TARGET asc_core
    ${ASC_CPP_SANITIZER_ARGUMENTS}
  )
endif()

if(ASC_CPP_ENABLE_CUDA)
  add_library(asc_core_cuda)
  add_library(ASC::core_cuda ALIAS asc_core_cuda)

  target_sources(
    asc_core_cuda
    PRIVATE
      cuda/runtime.cc
    PUBLIC
      FILE_SET HEADERS
      BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
      FILES
        "${PROJECT_SOURCE_DIR}/include/asc/core/providers/cuda.h"
        "${PROJECT_SOURCE_DIR}/include/asc/core/providers/cuda_export.h"
  )
  target_include_directories(
    asc_core_cuda
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}"
      "${PROJECT_SOURCE_DIR}/src/core"
  )

  set_target_properties(
    asc_core_cuda
    PROPERTIES
      CXX_VISIBILITY_PRESET hidden
      EXPORT_NAME core_cuda
      OUTPUT_NAME asc_core_cuda
      VISIBILITY_INLINES_HIDDEN YES
  )
  if(APPLE)
    set_property(TARGET asc_core_cuda PROPERTY INSTALL_RPATH "@loader_path")
  elseif(UNIX)
    set_property(TARGET asc_core_cuda PROPERTY INSTALL_RPATH "$ORIGIN")
  endif()

  target_compile_definitions(
    asc_core_cuda
    PRIVATE ASC_CORE_CUDA_BUILDING_LIBRARY
  )
  if(ASC_CPP_INTERNAL_ENABLE_CUDA_RUNTIME_TEST_HOOKS)
    target_compile_definitions(
      asc_core_cuda
      PRIVATE ASC_CPP_CUDA_RUNTIME_TEST_HOOKS
    )
  endif()
  if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(
      asc_core_cuda
      PUBLIC ASC_CORE_CUDA_STATIC_DEFINE
    )
  endif()

  target_link_libraries(
    asc_core_cuda
    PUBLIC ASC::core
    PRIVATE CUDA::cudart
  )
  asc_target_enable_cxx20(TARGET asc_core_cuda PUBLIC)
  if(ASC_CPP_WARNINGS_AS_ERRORS)
    asc_target_enable_warnings(TARGET asc_core_cuda AS_ERRORS)
  else()
    asc_target_enable_warnings(TARGET asc_core_cuda)
  endif()
  if(ASC_CPP_SANITIZER_ARGUMENTS)
    asc_target_enable_sanitizers(
      TARGET asc_core_cuda
      ${ASC_CPP_SANITIZER_ARGUMENTS}
    )
  endif()
endif()
