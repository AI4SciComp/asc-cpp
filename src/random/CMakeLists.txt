add_library(asc_random engines/philox.cc permutation.cc sobol.cc)
add_library(ASC::random ALIAS asc_random)
asc_cpp_configure_compiled_target(asc_random random)
target_link_libraries(asc_random PUBLIC asc_array asc_core)
target_sources(
  asc_random
  PUBLIC FILE_SET public_headers TYPE HEADERS
  BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
  FILES
    "${PROJECT_SOURCE_DIR}/include/asc/random.h"
    "${PROJECT_SOURCE_DIR}/include/asc/random/counter_engine.h"
    "${PROJECT_SOURCE_DIR}/include/asc/random/detail/fill_validation.h"
    "${PROJECT_SOURCE_DIR}/include/asc/random/distribution.h"
    "${PROJECT_SOURCE_DIR}/include/asc/random/fill.h"
    "${PROJECT_SOURCE_DIR}/include/asc/random/types.h"
)
