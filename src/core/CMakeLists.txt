add_library(asc_core
  casts.cc
  cuda.cc
  device.cc
  error.cc
  globals.cc
  math.cc
  memory.cc
  providers/serial/serial_provider.cc
  runtime/contracts.cc
  runtime/event.cc
  runtime/execution_context.cc
  runtime/memory_resource.cc
  runtime/status.cc
)
add_library(ASC::core ALIAS asc_core)
asc_cpp_configure_compiled_target(asc_core core)
target_sources(
  asc_core
  PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS
      "${PROJECT_SOURCE_DIR}/include"
      "${PROJECT_BINARY_DIR}/generated"
    FILES
      "${PROJECT_SOURCE_DIR}/include/asc/core.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/buffer.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/casts.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/config.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/contracts.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/cuda.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/device.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/error.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/event.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/execution_context.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/forall.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/globals.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/math.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/memory.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/memory_impl.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/memory_resource.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/memory_space.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/numeric.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/operators.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/status.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/string.h"
      "${PROJECT_SOURCE_DIR}/include/asc/core/types.h"
      "${PROJECT_BINARY_DIR}/generated/asc/config/_config.h"
)

if(ASC_CPP_ENABLE_OPENMP)
  target_link_libraries(asc_core PUBLIC OpenMP::OpenMP_CXX)
endif()
if(ASC_CPP_ENABLE_CUDA)
  target_link_libraries(asc_core PUBLIC CUDA::cudart CUDA::cusparse CUDA::cublas)
endif()
