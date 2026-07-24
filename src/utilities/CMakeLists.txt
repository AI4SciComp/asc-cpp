add_library(asc_utilities
  cli.cc
  compatibility.cc
  config.cc
  timer.cc
)
add_library(ASC::utilities ALIAS asc_utilities)
asc_cpp_configure_compiled_target(asc_utilities utilities)
target_link_libraries(asc_utilities PUBLIC asc_core)
target_sources(
  asc_utilities
  PUBLIC FILE_SET public_headers TYPE HEADERS
  BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
  FILES
    "${PROJECT_SOURCE_DIR}/include/asc/utilities.h"
    "${PROJECT_SOURCE_DIR}/include/asc/utilities/cli.h"
    "${PROJECT_SOURCE_DIR}/include/asc/utilities/config.h"
    "${PROJECT_SOURCE_DIR}/include/asc/utilities/optparser.h"
    "${PROJECT_SOURCE_DIR}/include/asc/utilities/timer.h"
)
