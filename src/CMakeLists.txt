# Add components in dependency order. Each subdirectory owns one public target.
add_subdirectory(core)
add_subdirectory(utilities)
add_subdirectory(array)
add_subdirectory(linalg)
add_subdirectory(random)

add_library(asc_cpp INTERFACE)
add_library(ASC::cpp ALIAS asc_cpp)
asc_cpp_configure_interface_target(asc_cpp cpp)
target_link_libraries(
  asc_cpp INTERFACE asc_core asc_utilities asc_array asc_linalg asc_random
)
target_sources(
  asc_cpp
  INTERFACE FILE_SET public_headers TYPE HEADERS
  BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
  FILES
    "${PROJECT_SOURCE_DIR}/include/asc/asc.h"
    "${PROJECT_SOURCE_DIR}/include/asc/cpp.h"
    "${PROJECT_SOURCE_DIR}/include/asc/random/generator.h"
    "${PROJECT_SOURCE_DIR}/include/asc/random/halton.h"
    "${PROJECT_SOURCE_DIR}/include/asc/random/hammersley.h"
    "${PROJECT_SOURCE_DIR}/include/asc/random/latin.h"
    "${PROJECT_SOURCE_DIR}/include/asc/random/normal.h"
    "${PROJECT_SOURCE_DIR}/include/asc/random/permutation.h"
    "${PROJECT_SOURCE_DIR}/include/asc/random/pseudo.h"
    "${PROJECT_SOURCE_DIR}/include/asc/random/sampler.h"
    "${PROJECT_SOURCE_DIR}/include/asc/random/sobol.h"
    "${PROJECT_SOURCE_DIR}/include/asc/random/spherical.h"
)
